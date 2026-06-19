#include "BattleWorldUI_CommandCards.h"

#include "BattleWorldUI_JsonTextureResolver.h"
#include "RenderTarget.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "VIBuffer_Rect.h"

NS_BEGIN(Client)

namespace
{
    constexpr wchar_t kBattleWorldCommandHUDJsonPath[] = L"UI/UI_Json/BattleSystem/UI_BattleSystem_HUD.json";
    constexpr uint32 kBattleWorldCommandCompositionRenderScale = 2;
    constexpr uint32 kBattleWorldCommandMaxApSegmentCount = 9;
    const Vec2 kBattleWorldCommandRootWorldSize = Vec2(2.04f, 1.40f);

    enum class BattleWorldCommandSurfaceKind
    {
        Background,
        ApBar,
        Button,
    };

    enum class BattleWorldCommandLayerKind
    {
        Image,
        Text,
    };

    enum class BattleWorldCommandTextSlot
    {
        Literal,
        CurrentAp,
    };

    enum class BattleWorldCommandImageSlot
    {
        Literal,
        ApSegment,
    };

    enum class BattleWorldCommandTextAlign
    {
        Left,
        Center,
        Right,
    };

    enum class BattleWorldCommandImageRenderMode
    {
        Normal,
        PatternOverlay,
        Unknown,
    };

    enum class BattleWorldCommandMaskTextureMode
    {
        Alpha,
        BlackWhite,
        Unknown,
    };

    struct BattleWorldCommandSurfaceContent
    {
        uint32 curAp = 0;
        uint32 maxAp = 0;
    };

    struct BattleWorldCommandImageDef
    {
        BattleWorldCommandImageSlot slot = BattleWorldCommandImageSlot::Literal;
        uint32 apSegmentIndex = 0;
        Shared<Texture> texture{};
        Vec2 center{};
        Vec2 size{};
        Vec2 pivot = Vec2(0.5f, 0.5f);
        float rotationRadians = 0.f;
        Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);
        BattleWorldCommandImageRenderMode imageRenderMode = BattleWorldCommandImageRenderMode::Normal;
        Shared<Texture> maskTexture{};
        BattleWorldCommandMaskTextureMode maskTextureMode = BattleWorldCommandMaskTextureMode::Alpha;
        Shared<Texture> patternMaskTexture{};
        Vec2 patternUvOffset = Vec2(0.f, 0.f);
        Vec2 patternUvScale = Vec2(1.f, 1.f);
    };

    struct BattleWorldCommandTextDef
    {
        BattleWorldCommandTextSlot slot = BattleWorldCommandTextSlot::Literal;
        wstring text;
        wstring fontTag;
        Vec2 size{};
        Vec2 pivot = Vec2(0.f, 0.f);
        Vec2 position{};
        float scale = 1.f;
        float rotationRadians = 0.f;
        Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);
        BattleWorldCommandTextAlign align = BattleWorldCommandTextAlign::Left;
        bool textWrap = false;
    };

    struct BattleWorldCommandSurfaceDef
    {
        BattleWorldCommandSurfaceKind kind = BattleWorldCommandSurfaceKind::Button;
        BattleWorldCommandSurfaceContent content{};
        Vec2 pixelSize = Vec2(1.f, 1.f);
        Vec3 worldOffset = Vec3::Zero;
        Vec2 worldSize = Vec2(1.f, 1.f);
        float rotationRadians = 0.f;
        int32 sortOrder = 0;
        uint32 sequence = 0;
        string sourceName{};

        struct Layer
        {
            BattleWorldCommandLayerKind kind = BattleWorldCommandLayerKind::Image;
            int32 sortOrder = 0;
            uint32 sequence = 0;
            BattleWorldCommandImageDef image{};
            BattleWorldCommandTextDef text{};
        };

        vector<Layer> layers{};
    };

    struct BattleWorldCommandSurfaceTemplate
    {
        BattleWorldCommandSurfaceKind kind = BattleWorldCommandSurfaceKind::Button;
        Vec2 pixelSize = Vec2(1.f, 1.f);
        Vec3 worldOffset = Vec3::Zero;
        Vec2 worldSize = Vec2(1.f, 1.f);
        float rotationRadians = 0.f;
        int32 sortOrder = 0;
        uint32 sequence = 0;
        string sourceName{};
        vector<BattleWorldCommandSurfaceDef::Layer> layers{};
    };

    class IWorldUICommandSurface;

    struct BattleWorldCommandSurfaceInstance
    {
        BattleWorldCommandSurfaceDef def{};
        Unique<IWorldUICommandSurface> surface{};
        Matrix worldMatrix = Matrix::Identity;
        bool visible = false;
        bool dirty = true;
    };

    struct BattleWorldCommandJsonObject
    {
        uint32 sequence = 0;
        int32 id = 0;
        int32 parentId = 0;
        int32 sortOrder = 0;
        string name;
        string type;
        string textureGuid;
        string texturePath;
        string maskTextureGuid;
        string maskTexturePath;
        string maskTextureMode = "Alpha";
        string imageRenderMode = "Normal";
        string patternMaskTextureGuid;
        string patternMaskTexturePath;
        string fontTag;
        string fontPath;
        string text;
        string textAlign = "Left";
        Vec2 position{};
        Vec2 size{};
        Vec2 pivot = Vec2(0.5f, 0.5f);
        Vec2 scale = Vec2(1.f, 1.f);
        Vec2 patternUvOffset = Vec2(0.f, 0.f);
        Vec2 patternUvScale = Vec2(1.f, 1.f);
        float rotationRadians = 0.f;
        float alphaMultiplier = 1.f;
        float fontSize = 32.f;
        float fillRatio = 1.f;
        Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);
        bool visible = true;
        bool textWrap = false;
        bool useFillRatio = false;
    };

    using BattleWorldCommandJsonObjectMap = unordered_map<int32, BattleWorldCommandJsonObject>;

    bool Should_ShowBattleWorldCommandCards(BattleTurnState state)
    {
        return state == BattleTurnState::ChooseAction;
    }

    string Read_JsonString(const json& node, const char* key, const string& fallback = {})
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_string())
            return fallback;

        return iter->get<string>();
    }

    int32 Read_JsonInt(const json& node, const char* key, int32 fallback = 0)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_number())
            return fallback;

        return iter->get<int32>();
    }

    float Read_JsonFloat(const json& node, const char* key, float fallback = 0.f)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_number())
            return fallback;

        return iter->get<float>();
    }

    bool Read_JsonBool(const json& node, const char* key, bool fallback = false)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_boolean())
            return fallback;

        return iter->get<bool>();
    }

    Vec2 Read_JsonVec2(const json& node, const char* key, const Vec2& fallback)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_array() || iter->size() < 2)
            return fallback;

        return Vec2(
            iter->at(0).is_number() ? iter->at(0).get<float>() : fallback.x,
            iter->at(1).is_number() ? iter->at(1).get<float>() : fallback.y);
    }

    Vec4 Read_JsonColor(const json& node, const char* key, const Vec4& fallback)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_array() || iter->size() < 4)
            return fallback;

        Vec4 color(
            iter->at(0).is_number() ? iter->at(0).get<float>() : fallback.x,
            iter->at(1).is_number() ? iter->at(1).get<float>() : fallback.y,
            iter->at(2).is_number() ? iter->at(2).get<float>() : fallback.z,
            iter->at(3).is_number() ? iter->at(3).get<float>() : fallback.w);

        if (color.x > 1.f || color.y > 1.f || color.z > 1.f || color.w > 1.f)
            color *= 1.f / 255.f;

        return color;
    }

    BattleWorldCommandImageRenderMode Parse_JsonImageRenderMode(const string& modeName)
    {
        if (modeName.empty())
            return BattleWorldCommandImageRenderMode::Normal;

        const string mode = String::ToLowerCopy(modeName);
        if ("normal" == mode)
            return BattleWorldCommandImageRenderMode::Normal;
        if ("patternoverlay" == mode || "pattern_overlay" == mode || "pattern overlay" == mode)
            return BattleWorldCommandImageRenderMode::PatternOverlay;

        return BattleWorldCommandImageRenderMode::Unknown;
    }

    BattleWorldCommandMaskTextureMode Parse_JsonMaskTextureMode(const string& modeName)
    {
        if (modeName.empty())
            return BattleWorldCommandMaskTextureMode::Alpha;

        const string mode = String::ToLowerCopy(modeName);
        if ("alpha" == mode)
            return BattleWorldCommandMaskTextureMode::Alpha;
        if ("blackwhite" == mode || "black_white" == mode || "black white" == mode)
            return BattleWorldCommandMaskTextureMode::BlackWhite;

        return BattleWorldCommandMaskTextureMode::Unknown;
    }

    const char* To_LogName(BattleWorldCommandImageRenderMode mode)
    {
        switch (mode)
        {
        case BattleWorldCommandImageRenderMode::Normal:
            return "Normal";
        case BattleWorldCommandImageRenderMode::PatternOverlay:
            return "PatternOverlay";
        case BattleWorldCommandImageRenderMode::Unknown:
        default:
            return "Unknown";
        }
    }

    const char* To_LogName(BattleWorldCommandMaskTextureMode mode)
    {
        switch (mode)
        {
        case BattleWorldCommandMaskTextureMode::Alpha:
            return "Alpha";
        case BattleWorldCommandMaskTextureMode::BlackWhite:
            return "BlackWhite";
        case BattleWorldCommandMaskTextureMode::Unknown:
        default:
            return "Unknown";
        }
    }

    bool Has_TextureRef(const string& textureGuid, const string& texturePath)
    {
        return !textureGuid.empty() || !texturePath.empty();
    }

    BattleWorldCommandJsonObject Parse_JsonObject(const json& node, uint32 sequence)
    {
        BattleWorldCommandJsonObject object{};
        object.sequence = sequence;
        object.id = Read_JsonInt(node, "id");
        object.parentId = Read_JsonInt(node, "parentId");
        object.sortOrder = Read_JsonInt(node, "sortOrder");
        object.name = Read_JsonString(node, "name");
        object.type = Read_JsonString(node, "type");
        object.textureGuid = Read_JsonString(node, "textureGuid");
        object.texturePath = Read_JsonString(node, "texturePath");
        object.maskTextureGuid = Read_JsonString(node, "maskTextureGuid");
        object.maskTexturePath = Read_JsonString(node, "maskTexturePath");
        object.maskTextureMode = Read_JsonString(node, "maskTextureMode", "Alpha");
        object.imageRenderMode = Read_JsonString(node, "imageRenderMode", "Normal");
        object.patternMaskTextureGuid = Read_JsonString(node, "patternMaskTextureGuid");
        object.patternMaskTexturePath = Read_JsonString(node, "patternMaskTexturePath");
        object.fontTag = Read_JsonString(node, "fontTag");
        object.fontPath = Read_JsonString(node, "fontPath");
        object.text = Read_JsonString(node, "text");
        object.textAlign = Read_JsonString(node, "textAlign", "Left");
        object.position = Read_JsonVec2(node, "position", object.position);
        object.size = Read_JsonVec2(node, "size", object.size);
        object.pivot = Read_JsonVec2(node, "pivot", object.pivot);
        object.scale = Read_JsonVec2(node, "scale", object.scale);
        object.rotationRadians = Read_JsonFloat(node, "rotation", 0.f);
        object.alphaMultiplier = Read_JsonFloat(node, "alphaMultiplier", 1.f);
        object.fontSize = Read_JsonFloat(node, "fontSize", 32.f);
        object.fillRatio = Read_JsonFloat(node, "fillRatio", 1.f);
        object.patternUvOffset = Read_JsonVec2(node, "patternUvOffset", object.patternUvOffset);
        object.patternUvScale = Read_JsonVec2(node, "patternUvScale", object.patternUvScale);
        object.color = Read_JsonColor(node, "color", object.color);
        object.visible = Read_JsonBool(node, "visible", true);
        object.textWrap = Read_JsonBool(node, "textWrap", false);
        object.useFillRatio = Read_JsonBool(node, "useFillRatio", false);
        return object;
    }

    Vec2 Rotate_Point(const Vec2& point, float rotationRadians)
    {
        const float cosAngle = cosf(rotationRadians);
        const float sinAngle = sinf(rotationRadians);

        return Vec2(
            point.x * cosAngle - point.y * sinAngle,
            point.x * sinAngle + point.y * cosAngle);
    }

    Vec2 Transform_PointByJsonParent(
        const Vec2& point,
        const BattleWorldCommandJsonObject& parent)
    {
        const Vec2 pivotOffset(
            parent.size.x * parent.pivot.x,
            parent.size.y * parent.pivot.y);
        const Vec2 scaledOffset(
            (point.x - pivotOffset.x) * parent.scale.x,
            (point.y - pivotOffset.y) * parent.scale.y);
        const Vec2 rotatedOffset = Rotate_Point(scaledOffset, parent.rotationRadians);

        return Vec2(
            parent.position.x + rotatedOffset.x,
            parent.position.y + rotatedOffset.y);
    }

    const BattleWorldCommandJsonObject* Find_JsonObject(
        const BattleWorldCommandJsonObjectMap& objectsById,
        int32 objectId)
    {
        const auto iter = objectsById.find(objectId);
        if (iter == objectsById.end())
            return nullptr;

        return &iter->second;
    }

    bool Is_DescendantOf(
        const BattleWorldCommandJsonObject& object,
        int32 panelId,
        const BattleWorldCommandJsonObjectMap& objectsById)
    {
        int32 parentId = object.parentId;
        for (uint32 depth = 0; depth < 256 && parentId != 0; ++depth)
        {
            if (parentId == panelId)
                return true;

            const BattleWorldCommandJsonObject* parent = Find_JsonObject(objectsById, parentId);
            if (parent == nullptr)
                break;

            parentId = parent->parentId;
        }

        return false;
    }

    bool Is_VisibleInPanelHierarchy(
        const BattleWorldCommandJsonObject& object,
        int32 panelId,
        const BattleWorldCommandJsonObjectMap& objectsById)
    {
        if (!object.visible)
            return false;

        int32 parentId = object.parentId;
        for (uint32 depth = 0; depth < 256 && parentId != 0; ++depth)
        {
            if (parentId == panelId)
                return true;

            const BattleWorldCommandJsonObject* parent = Find_JsonObject(objectsById, parentId);
            if (parent == nullptr)
                return false;

            if (!parent->visible)
                return false;

            parentId = parent->parentId;
        }

        return false;
    }

    Vec2 Transform_PointToPanelLocal(
        Vec2 point,
        int32 parentId,
        int32 panelId,
        const BattleWorldCommandJsonObjectMap& objectsById)
    {
        for (uint32 depth = 0; depth < 256 && parentId != 0 && parentId != panelId; ++depth)
        {
            const BattleWorldCommandJsonObject* parent = Find_JsonObject(objectsById, parentId);
            if (parent == nullptr)
                break;

            point = Transform_PointByJsonParent(point, *parent);
            parentId = parent->parentId;
        }

        return point;
    }

    Vec2 Resolve_JsonWorldScale(
        const BattleWorldCommandJsonObject& object,
        int32 panelId,
        const BattleWorldCommandJsonObjectMap& objectsById)
    {
        Vec2 scale = object.scale;
        int32 parentId = object.parentId;

        for (uint32 depth = 0; depth < 256 && parentId != 0 && parentId != panelId; ++depth)
        {
            const BattleWorldCommandJsonObject* parent = Find_JsonObject(objectsById, parentId);
            if (parent == nullptr)
                break;

            scale.x *= parent->scale.x;
            scale.y *= parent->scale.y;
            parentId = parent->parentId;
        }

        return scale;
    }

    float Resolve_JsonWorldRotation(
        const BattleWorldCommandJsonObject& object,
        int32 panelId,
        const BattleWorldCommandJsonObjectMap& objectsById)
    {
        float rotation = object.rotationRadians;
        int32 parentId = object.parentId;

        for (uint32 depth = 0; depth < 256 && parentId != 0 && parentId != panelId; ++depth)
        {
            const BattleWorldCommandJsonObject* parent = Find_JsonObject(objectsById, parentId);
            if (parent == nullptr)
                break;

            rotation += parent->rotationRadians;
            parentId = parent->parentId;
        }

        return rotation;
    }

    float Resolve_JsonAlphaMultiplier(
        const BattleWorldCommandJsonObject& object,
        int32 panelId,
        const BattleWorldCommandJsonObjectMap& objectsById)
    {
        float alphaMultiplier = object.alphaMultiplier;
        int32 parentId = object.parentId;

        for (uint32 depth = 0; depth < 256 && parentId != 0 && parentId != panelId; ++depth)
        {
            const BattleWorldCommandJsonObject* parent = Find_JsonObject(objectsById, parentId);
            if (parent == nullptr)
                break;

            alphaMultiplier *= parent->alphaMultiplier;
            parentId = parent->parentId;
        }

        return alphaMultiplier;
    }

    Vec2 Resolve_JsonImageCenter(const BattleWorldCommandJsonObject& object)
    {
        const Vec2 pivotOffset(
            object.size.x * object.pivot.x,
            object.size.y * object.pivot.y);
        const Vec2 centerOffset(
            object.size.x * 0.5f,
            object.size.y * 0.5f);
        const Vec2 scaledCenterOffset(
            (centerOffset.x - pivotOffset.x) * object.scale.x,
            (centerOffset.y - pivotOffset.y) * object.scale.y);
        const Vec2 rotatedCenterOffset = Rotate_Point(scaledCenterOffset, object.rotationRadians);

        return Vec2(
            object.position.x + rotatedCenterOffset.x,
            object.position.y + rotatedCenterOffset.y);
    }

    wstring Resolve_RuntimeResourcePath(const string& path)
    {
        if (path.empty())
            return {};

        string normalizedPath = path;
        replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

        string loweredPath = String::ToLowerCopy(normalizedPath);
        string resourceRelativePath = normalizedPath;

        constexpr const char* resourcePrefix = "../bin/resources/";
        constexpr const char* binResourcePrefix = "bin/resources/";
        constexpr const char* resourcesPrefix = "resources/";

        if (loweredPath.rfind(resourcePrefix, 0) == 0)
            resourceRelativePath = normalizedPath.substr(strlen(resourcePrefix));
        else if (loweredPath.rfind(binResourcePrefix, 0) == 0)
            resourceRelativePath = normalizedPath.substr(strlen(binResourcePrefix));
        else if (loweredPath.rfind(resourcesPrefix, 0) == 0)
            resourceRelativePath = normalizedPath.substr(strlen(resourcesPrefix));

        return (fs::path(GAME->Get_AssetRoot()) / fs::path(String::ToWString(resourceRelativePath))).lexically_normal().wstring();
    }

    bool Ensure_JsonFontLoaded(const string& fontTag, const string& fontPath)
    {
        if (fontTag.empty())
            return false;

        const wstring resolvedFontTag = String::ToWString(fontTag);
        if (GAME->Has_Font(resolvedFontTag))
            return true;

        const wstring resolvedFontPath = Resolve_RuntimeResourcePath(fontPath);
        if (resolvedFontPath.empty() || !fs::exists(resolvedFontPath))
            return false;

        return SUCCEEDED(GAME->Add_Font(resolvedFontTag, resolvedFontPath.c_str()));
    }

    BattleWorldCommandTextAlign Resolve_TextAlign(const string& textAlign)
    {
        const string normalizedTextAlign = String::ToLowerCopy(textAlign);
        if (normalizedTextAlign == "center")
            return BattleWorldCommandTextAlign::Center;
        if (normalizedTextAlign == "right")
            return BattleWorldCommandTextAlign::Right;

        return BattleWorldCommandTextAlign::Left;
    }

    bool Try_ParseApSegmentIndex(const string& objectName, uint32& outIndex)
    {
        constexpr const char* prefix = "Image_BattlePortrait_Current_Character_AP_";
        const string prefixString = prefix;
        if (objectName.rfind(prefixString, 0) != 0)
            return false;

        const string suffix = objectName.substr(prefixString.size());
        if (suffix.empty())
            return false;

        for (const char value : suffix)
        {
            if (value < '0' || value > '9')
                return false;
        }

        const uint32 index = static_cast<uint32>(stoul(suffix));
        if (index == 0 || index > kBattleWorldCommandMaxApSegmentCount)
            return false;

        outIndex = index;
        return true;
    }

    BattleWorldCommandTextSlot Resolve_TextSlot(const string& objectName)
    {
        if (objectName == "Text_BattlePortrait_Current_Character_Icon_AP" ||
            objectName == "Text_BattlePortrait_Current_Character_Icon_AP_BG")
        {
            return BattleWorldCommandTextSlot::CurrentAp;
        }

        return BattleWorldCommandTextSlot::Literal;
    }

    wstring Resolve_TextForSlot(
        BattleWorldCommandTextSlot slot,
        const BattleWorldCommandSurfaceContent& content,
        const wstring& literalText)
    {
        switch (slot)
        {
        case BattleWorldCommandTextSlot::CurrentAp:
            return to_wstring(min(content.curAp, min(content.maxAp, kBattleWorldCommandMaxApSegmentCount)));
        case BattleWorldCommandTextSlot::Literal:
        default:
            return literalText;
        }
    }

    Vec4 Make_PremultipliedSpriteFontColor(const Vec4& color)
    {
        const float alpha = min(max(color.w, 0.f), 1.f);
        return Vec4(
            min(max(color.x, 0.f), 1.f) * alpha,
            min(max(color.y, 0.f), 1.f) * alpha,
            min(max(color.z, 0.f), 1.f) * alpha,
            alpha);
    }

    Vec2 Resolve_AspectWorldSize(const Vec2& baseWorldSize, const Vec2& pixelSize)
    {
        if (pixelSize.x <= 0.f || pixelSize.y <= 0.f)
            return baseWorldSize;

        return Vec2(baseWorldSize.x, baseWorldSize.x * pixelSize.y / pixelSize.x);
    }

    Vec3 Resolve_SurfaceWorldOffset(
        const BattleWorldCommandJsonObject& panel,
        const BattleWorldCommandJsonObject& rootPanel)
    {
        const float rootWidth = max(1.f, rootPanel.size.x);
        const float rootHeight = max(1.f, rootPanel.size.y);
        const Vec2 local(
            panel.position.x - rootWidth * rootPanel.pivot.x,
            rootHeight * rootPanel.pivot.y - panel.position.y);

        return Vec3(
            local.x / rootWidth * kBattleWorldCommandRootWorldSize.x,
            local.y / rootHeight * kBattleWorldCommandRootWorldSize.y,
            0.f);
    }

    Vec2 Resolve_SurfaceWorldSize(
        const BattleWorldCommandJsonObject& panel,
        const BattleWorldCommandJsonObject& rootPanel)
    {
        const float rootWidth = max(1.f, rootPanel.size.x);
        const float worldWidth =
            kBattleWorldCommandRootWorldSize.x * max(1.f, panel.size.x) / rootWidth;
        return Resolve_AspectWorldSize(Vec2(worldWidth, worldWidth), panel.size);
    }

    Vec4 Resolve_ImageColor(const Vec4& color)
    {
        return color;
    }

    Vec4 Resolve_TextColor(const Vec4& color)
    {
        return color;
    }

    bool Should_DrawImageLayer(
        const BattleWorldCommandImageDef& image,
        const BattleWorldCommandSurfaceContent& content)
    {
        if (image.slot != BattleWorldCommandImageSlot::ApSegment)
            return true;

        const uint32 visibleAp = min(content.curAp, min(content.maxAp, kBattleWorldCommandMaxApSegmentCount));
        return image.apSegmentIndex > 0 && image.apSegmentIndex <= visibleAp;
    }

    bool Try_ExtractJsonImageLayer(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const BattleWorldCommandJsonObject& object,
        const char* panelName,
        int32 panelId,
        const BattleWorldCommandJsonObjectMap& objectsById,
        BattleWorldCommandSurfaceDef::Layer& outLayer)
    {
        BattleWorldCommandImageRenderMode imageRenderMode =
            Parse_JsonImageRenderMode(object.imageRenderMode);
        const BattleWorldCommandMaskTextureMode maskTextureMode =
            Parse_JsonMaskTextureMode(object.maskTextureMode);

        if (imageRenderMode == BattleWorldCommandImageRenderMode::Normal &&
            Has_TextureRef(object.patternMaskTextureGuid, object.patternMaskTexturePath))
        {
            imageRenderMode = BattleWorldCommandImageRenderMode::PatternOverlay;
        }

        if (imageRenderMode == BattleWorldCommandImageRenderMode::Unknown)
        {
            LOG_WARN(
                "BattleWorldUI command json image skipped: unknown imageRenderMode. panel='{}', name='{}', mode='{}'",
                panelName,
                object.name,
                object.imageRenderMode);
            return false;
        }

        if (object.useFillRatio)
        {
            LOG_DEBUG(
                "BattleWorldUI command json image skipped: unsupported fillRatio. panel='{}', name='{}', fillRatio={}",
                panelName,
                object.name,
                object.fillRatio);
            return false;
        }

        if (imageRenderMode != BattleWorldCommandImageRenderMode::PatternOverlay &&
            Has_TextureRef(object.maskTextureGuid, object.maskTexturePath) &&
            maskTextureMode == BattleWorldCommandMaskTextureMode::Unknown)
        {
            LOG_WARN(
                "BattleWorldUI command json image skipped: unknown maskTextureMode. panel='{}', name='{}', mode='{}'",
                panelName,
                object.name,
                object.maskTextureMode);
            return false;
        }

        Shared<Texture> texture = BattleWorldUI_JsonTextureResolver::Load_TextureFromUITextureRef(
            device,
            context,
            object.textureGuid,
            object.texturePath,
            object.name);
        if (!texture)
            return false;

        Shared<Texture> maskTexture{};
        const bool hasMaskRef = Has_TextureRef(object.maskTextureGuid, object.maskTexturePath);
        const bool requiresMaskTexture = hasMaskRef || imageRenderMode == BattleWorldCommandImageRenderMode::PatternOverlay;
        if (requiresMaskTexture)
        {
            if (!hasMaskRef)
            {
                LOG_WARN(
                    "BattleWorldUI command json image skipped: missing required mask ref. panel='{}', name='{}', mode='{}'",
                    panelName,
                    object.name,
                    To_LogName(imageRenderMode));
                return false;
            }

            maskTexture = BattleWorldUI_JsonTextureResolver::Load_TextureFromUITextureRef(
                device,
                context,
                object.maskTextureGuid,
                object.maskTexturePath,
                object.name + "/mask");
            if (!maskTexture)
            {
                LOG_WARN(
                    "BattleWorldUI command json image skipped: missing required mask. panel='{}', name='{}', maskPath='{}'",
                    panelName,
                    object.name,
                    object.maskTexturePath);
                return false;
            }
        }

        Shared<Texture> patternMaskTexture{};
        const bool hasPatternMaskRef = Has_TextureRef(object.patternMaskTextureGuid, object.patternMaskTexturePath);
        if (imageRenderMode == BattleWorldCommandImageRenderMode::PatternOverlay)
        {
            if (!hasPatternMaskRef)
            {
                LOG_WARN(
                    "BattleWorldUI command json image skipped: missing required pattern mask ref. panel='{}', name='{}'",
                    panelName,
                    object.name);
                return false;
            }

            patternMaskTexture = BattleWorldUI_JsonTextureResolver::Load_TextureFromUITextureRef(
                device,
                context,
                object.patternMaskTextureGuid,
                object.patternMaskTexturePath,
                object.name + "/patternMask");
            if (!patternMaskTexture)
            {
                LOG_WARN(
                    "BattleWorldUI command json image skipped: missing required pattern mask. panel='{}', name='{}', patternMaskPath='{}'",
                    panelName,
                    object.name,
                    object.patternMaskTexturePath);
                return false;
            }
        }

        const Vec2 worldScale = Resolve_JsonWorldScale(object, panelId, objectsById);
        const Vec2 localCenter = Resolve_JsonImageCenter(object);
        const Vec2 panelLocalCenter = Transform_PointToPanelLocal(localCenter, object.parentId, panelId, objectsById);

        Vec4 color = object.color;
        color.w *= Resolve_JsonAlphaMultiplier(object, panelId, objectsById);

        uint32 apSegmentIndex = 0;
        const bool isApSegment = Try_ParseApSegmentIndex(object.name, apSegmentIndex);

        outLayer.kind = BattleWorldCommandLayerKind::Image;
        outLayer.sortOrder = object.sortOrder;
        outLayer.sequence = object.sequence;
        outLayer.image.slot = isApSegment ? BattleWorldCommandImageSlot::ApSegment : BattleWorldCommandImageSlot::Literal;
        outLayer.image.apSegmentIndex = apSegmentIndex;
        outLayer.image.texture = std::move(texture);
        outLayer.image.center = panelLocalCenter;
        outLayer.image.size = Vec2(
            max(1.f, object.size.x * worldScale.x),
            max(1.f, object.size.y * worldScale.y));
        outLayer.image.pivot = object.pivot;
        outLayer.image.rotationRadians = Resolve_JsonWorldRotation(object, panelId, objectsById);
        outLayer.image.color = color;
        outLayer.image.imageRenderMode = imageRenderMode;
        outLayer.image.maskTexture = std::move(maskTexture);
        outLayer.image.maskTextureMode = maskTextureMode;
        outLayer.image.patternMaskTexture = std::move(patternMaskTexture);
        outLayer.image.patternUvOffset = object.patternUvOffset;
        outLayer.image.patternUvScale = object.patternUvScale;

        return true;
    }

    bool Try_ExtractJsonTextLayer(
        const BattleWorldCommandJsonObject& object,
        int32 panelId,
        const BattleWorldCommandJsonObjectMap& objectsById,
        BattleWorldCommandSurfaceDef::Layer& outLayer)
    {
        if (object.text.empty())
            return false;

        if (!Ensure_JsonFontLoaded(object.fontTag, object.fontPath))
            return false;

        const Vec2 worldScale = Resolve_JsonWorldScale(object, panelId, objectsById);
        const Vec2 panelLocalPosition = Transform_PointToPanelLocal(object.position, object.parentId, panelId, objectsById);

        Vec4 color = object.color;
        color.w *= Resolve_JsonAlphaMultiplier(object, panelId, objectsById);

        outLayer.kind = BattleWorldCommandLayerKind::Text;
        outLayer.sortOrder = object.sortOrder;
        outLayer.sequence = object.sequence;
        outLayer.text.slot = Resolve_TextSlot(object.name);
        outLayer.text.text = String::ToWString(object.text);
        outLayer.text.fontTag = String::ToWString(object.fontTag);
        outLayer.text.size = object.size;
        outLayer.text.pivot = object.pivot;
        outLayer.text.position = panelLocalPosition;
        outLayer.text.scale = max(0.01f, worldScale.x * object.fontSize / 32.f);
        outLayer.text.rotationRadians = Resolve_JsonWorldRotation(object, panelId, objectsById);
        outLayer.text.color = Make_PremultipliedSpriteFontColor(color);
        outLayer.text.align = Resolve_TextAlign(object.textAlign);
        outLayer.text.textWrap = object.textWrap;
        return true;
    }

    BattleWorldCommandSurfaceKind Resolve_SurfaceKind(const string& objectName)
    {
        if (objectName == "Panel_BattleCommand_BG")
            return BattleWorldCommandSurfaceKind::Background;
        if (objectName == "Panel_BattlePortrait_Current_Character_AP_Bar")
            return BattleWorldCommandSurfaceKind::ApBar;

        return BattleWorldCommandSurfaceKind::Button;
    }

    bool Is_CommandSurfaceRootName(const string& objectName)
    {
        return objectName == "Panel_BattleCommand_BG" ||
            objectName == "Panel_BattlePortrait_Current_Character_AP_Bar" ||
            objectName == "Button_BattleCommand_Shot" ||
            objectName == "Button_BattleCommand_Item" ||
            objectName == "Button_BattleCommand_Skill" ||
            objectName == "Button_BattleCommand_GradientSkill" ||
            objectName == "Button_BattleCommand_Attack";
    }

    BattleWorldCommandSurfaceDef Build_SurfaceDef(
        const BattleWorldCommandSurfaceContent& content,
        const BattleWorldCommandSurfaceTemplate& surfaceTemplate)
    {
        BattleWorldCommandSurfaceDef def{};
        def.kind = surfaceTemplate.kind;
        def.content = content;
        def.pixelSize = surfaceTemplate.pixelSize;
        def.worldOffset = surfaceTemplate.worldOffset;
        def.worldSize = surfaceTemplate.worldSize;
        def.rotationRadians = surfaceTemplate.rotationRadians;
        def.sortOrder = surfaceTemplate.sortOrder;
        def.sequence = surfaceTemplate.sequence;
        def.sourceName = surfaceTemplate.sourceName;
        def.layers = surfaceTemplate.layers;

        for (auto& layer : def.layers)
        {
            if (layer.kind != BattleWorldCommandLayerKind::Text)
                continue;

            layer.text.text = Resolve_TextForSlot(layer.text.slot, content, layer.text.text);
        }

        return def;
    }

    bool Try_LoadJsonCommandSurfaceTemplates(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        vector<BattleWorldCommandSurfaceTemplate>& outTemplates)
    {
        const fs::path jsonPath = (fs::path(GAME->Get_AssetRoot()) / kBattleWorldCommandHUDJsonPath).lexically_normal();
        if (!fs::exists(jsonPath))
        {
            LOG_WARN("BattleWorldUI command json template missing. path='{}'", String::ToString(jsonPath.wstring()));
            return false;
        }

        json root{};
        try
        {
            ifstream file(jsonPath);
            file >> root;
        }
        catch (const exception& exception)
        {
            LOG_WARN("BattleWorldUI command json template parse failed. path='{}', error='{}'",
                String::ToString(jsonPath.wstring()),
                exception.what());
            return false;
        }

        const auto objectsIter = root.find("objects");
        if (objectsIter == root.end() || !objectsIter->is_array())
            return false;

        vector<BattleWorldCommandJsonObject> objects{};
        BattleWorldCommandJsonObjectMap objectsById{};
        objects.reserve(objectsIter->size());

        uint32 sequence = 0;
        for (const auto& node : *objectsIter)
        {
            if (!node.is_object())
                continue;

            BattleWorldCommandJsonObject object = Parse_JsonObject(node, sequence++);
            objectsById.emplace(object.id, object);
            objects.push_back(std::move(object));
        }

        const auto rootPanelIter = find_if(objects.begin(), objects.end(),
            [](const BattleWorldCommandJsonObject& object)
            {
                return object.name == "Panel_BattleCommand";
            });

        if (rootPanelIter == objects.end() || !rootPanelIter->visible)
        {
            LOG_WARN("BattleWorldUI command root panel missing. panel='Panel_BattleCommand'");
            return false;
        }

        vector<BattleWorldCommandJsonObject> surfaceRoots{};
        for (const BattleWorldCommandJsonObject& object : objects)
        {
            if (object.parentId != rootPanelIter->id)
                continue;
            if (!object.visible || !Is_CommandSurfaceRootName(object.name))
                continue;

            surfaceRoots.push_back(object);
        }

        sort(surfaceRoots.begin(), surfaceRoots.end(),
            [](const BattleWorldCommandJsonObject& lhs, const BattleWorldCommandJsonObject& rhs)
            {
                if (lhs.sortOrder != rhs.sortOrder)
                    return lhs.sortOrder < rhs.sortOrder;

                return lhs.sequence < rhs.sequence;
            });

        if (surfaceRoots.size() != 7)
        {
            LOG_WARN("BattleWorldUI command json expected 7 surfaces, found {}.", surfaceRoots.size());
            return false;
        }

        vector<BattleWorldCommandSurfaceTemplate> templates{};
        templates.reserve(surfaceRoots.size());

        for (const BattleWorldCommandJsonObject& surfaceRoot : surfaceRoots)
        {
            BattleWorldCommandSurfaceTemplate surfaceTemplate{};
            surfaceTemplate.kind = Resolve_SurfaceKind(surfaceRoot.name);
            surfaceTemplate.pixelSize = Vec2(max(1.f, surfaceRoot.size.x), max(1.f, surfaceRoot.size.y));
            surfaceTemplate.worldOffset = Resolve_SurfaceWorldOffset(surfaceRoot, *rootPanelIter);
            surfaceTemplate.worldSize = Resolve_SurfaceWorldSize(surfaceRoot, *rootPanelIter);
            surfaceTemplate.rotationRadians = surfaceRoot.rotationRadians;
            surfaceTemplate.sortOrder = surfaceRoot.sortOrder;
            surfaceTemplate.sequence = surfaceRoot.sequence;
            surfaceTemplate.sourceName = surfaceRoot.name;

            for (const BattleWorldCommandJsonObject& object : objects)
            {
                if (!Is_DescendantOf(object, surfaceRoot.id, objectsById))
                    continue;

                if (!Is_VisibleInPanelHierarchy(object, surfaceRoot.id, objectsById))
                    continue;

                BattleWorldCommandSurfaceDef::Layer layer{};
                bool extracted = false;

                if (object.type == "Image")
                    extracted = Try_ExtractJsonImageLayer(device, context, object, surfaceRoot.name.c_str(), surfaceRoot.id, objectsById, layer);
                else if (object.type == "Text")
                    extracted = Try_ExtractJsonTextLayer(object, surfaceRoot.id, objectsById, layer);

                if (extracted)
                    surfaceTemplate.layers.push_back(std::move(layer));
            }

            if (surfaceTemplate.layers.empty())
            {
                LOG_WARN("BattleWorldUI command json surface has no extracted layers. panel='{}'", surfaceRoot.name);
                return false;
            }

            sort(surfaceTemplate.layers.begin(), surfaceTemplate.layers.end(),
                [](const BattleWorldCommandSurfaceDef::Layer& lhs, const BattleWorldCommandSurfaceDef::Layer& rhs)
                {
                    if (lhs.sortOrder != rhs.sortOrder)
                        return lhs.sortOrder < rhs.sortOrder;

                    return lhs.sequence < rhs.sequence;
                });

            templates.push_back(std::move(surfaceTemplate));
        }

        outTemplates = std::move(templates);
        return true;
    }

    HRESULT Bind_WorldUIPlaneShaderState(
        ShaderCom* shader,
        const Matrix& worldMatrix,
        const Vec4& color)
    {
        CHECK_NULL(shader, E_FAIL);

        const Vec2 defaultPatternOffset = Vec2(0.f, 0.f);
        const Vec2 defaultPatternScale = Vec2(1.f, 1.f);
        const Vec2 defaultHoleCenter = Vec2(0.5f, 0.5f);
        const float defaultHoleRadius = 0.f;
        const float defaultHoleFeather = 0.05f;
        const float defaultViewportAspect = 1.f;
        const float defaultFillRatio = 1.f;
        const bool disabled = false;
        const uint32 fillOrigin = 0u;

        CHECK_FAILED(shader->Bind_Matrix("g_WorldMatrix", &worldMatrix), E_FAIL);
        CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::View, shader, "g_ViewMatrix"), E_FAIL);
        CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, shader, "g_ProjMatrix"), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_Color", &color, sizeof(Vec4)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_PatternUvOffset", &defaultPatternOffset, sizeof(Vec2)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_PatternUvScale", &defaultPatternScale, sizeof(Vec2)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_UseGrayScale", &disabled, sizeof(bool)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_IsOverlay", &disabled, sizeof(bool)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_HoleCenterUV", &defaultHoleCenter, sizeof(Vec2)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_HoleRadius", &defaultHoleRadius, sizeof(float)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_HoleFeather", &defaultHoleFeather, sizeof(float)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_ViewportAspect", &defaultViewportAspect, sizeof(float)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_UseFillRatio", &disabled, sizeof(bool)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_FillRatio", &defaultFillRatio, sizeof(float)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_FillOrigin", &fillOrigin, sizeof(uint32)), E_FAIL);

        return S_OK;
    }

    class ScopedRenderTargetState final
    {
    public:
        explicit ScopedRenderTargetState(const ComPtr<Context>& context)
            : _context{ context }
        {
            if (_context == nullptr)
                return;

            _viewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
            _context->RSGetViewports(&_viewportCount, _viewports);
            _context->OMGetRenderTargets(1, _rtv.ReleaseAndGetAddressOf(), _dsv.ReleaseAndGetAddressOf());
            _context->OMGetBlendState(_blendState.ReleaseAndGetAddressOf(), _blendFactor, &_sampleMask);
            _context->OMGetDepthStencilState(_depthStencilState.ReleaseAndGetAddressOf(), &_stencilRef);
            _context->RSGetState(_rasterizerState.ReleaseAndGetAddressOf());
        }

        ~ScopedRenderTargetState()
        {
            if (_context == nullptr)
                return;

            ID3D11RenderTargetView* rtvs[] = { _rtv.Get() };
            _context->OMSetRenderTargets(1, rtvs, _dsv.Get());

            if (_viewportCount > 0)
                _context->RSSetViewports(_viewportCount, _viewports);

            _context->OMSetBlendState(_blendState.Get(), _blendFactor, _sampleMask);
            _context->OMSetDepthStencilState(_depthStencilState.Get(), _stencilRef);
            _context->RSSetState(_rasterizerState.Get());
        }

    private:
        ComPtr<Context> _context{};
        ComPtr<ID3D11RenderTargetView> _rtv{};
        ComPtr<ID3D11DepthStencilView> _dsv{};
        ComPtr<ID3D11BlendState> _blendState{};
        ComPtr<ID3D11DepthStencilState> _depthStencilState{};
        ComPtr<ID3D11RasterizerState> _rasterizerState{};
        D3D11_VIEWPORT _viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
        uint32 _viewportCount = 0;
        float _blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
        uint32 _sampleMask = 0xffffffff;
        uint32 _stencilRef = 0;
    };

    class IWorldUICommandSurface
    {
    public:
        virtual ~IWorldUICommandSurface() = default;

        virtual HRESULT Compose(const BattleWorldCommandSurfaceDef& def) = 0;
        virtual HRESULT Bind_ShaderResource(ShaderCom* shader, const char* name) const = 0;
    };

    class RenderTargetComposedBattleWorldCommandSurface final : public IWorldUICommandSurface
    {
    public:
        RenderTargetComposedBattleWorldCommandSurface(
            const ComPtr<Device>& device,
            const ComPtr<Context>& context,
            const Shared<ShaderCom>& shader,
            const Shared<VIBuffer_Rect>& viBuffer,
            const Vec2& pixelSize)
            : _device{ device }
            , _context{ context }
            , _shader{ shader }
            , _viBuffer{ viBuffer }
            , _pixelSize{ pixelSize }
        {
        }

        HRESULT Initialize(uint32 surfaceIndex)
        {
            _renderTarget = RenderTarget::Create(
                L"BattleWorldCommandSurface_" + to_wstring(surfaceIndex),
                _device,
                _context,
                static_cast<uint32>(max(1.f, roundf(_pixelSize.x * static_cast<float>(kBattleWorldCommandCompositionRenderScale)))),
                static_cast<uint32>(max(1.f, roundf(_pixelSize.y * static_cast<float>(kBattleWorldCommandCompositionRenderScale)))),
                DXGI_FORMAT_R8G8B8A8_UNORM,
                Vec4(0.f, 0.f, 0.f, 0.f),
                false);
            CHECK_NULL(_renderTarget, E_FAIL);

            D3D11_BLEND_DESC blendDesc{};
            blendDesc.RenderTarget[0].BlendEnable = TRUE;
            blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
            blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
            blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            CHECK_FAILED(_device->CreateBlendState(&blendDesc, _compositionBlendState.GetAddressOf()), E_FAIL);

            return S_OK;
        }

        HRESULT Compose(const BattleWorldCommandSurfaceDef& def) override
        {
            CHECK_NULL(_renderTarget, E_FAIL);

            ScopedRenderTargetState restoreState{ _context };

            ID3D11ShaderResourceView* nullSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
            _context->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);
            _context->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);

            CHECK_FAILED(_renderTarget->Begin(true, false), E_FAIL);

            for (const auto& layer : def.layers)
            {
                if (layer.kind == BattleWorldCommandLayerKind::Image &&
                    Should_DrawImageLayer(layer.image, def.content))
                {
                    CHECK_FAILED(Draw_CompositionImage(layer.image), E_FAIL);
                }
            }

            for (const auto& layer : def.layers)
            {
                if (layer.kind == BattleWorldCommandLayerKind::Text)
                    Draw_CompositionText(layer.text);
            }

            _context->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);
            _context->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);

            return S_OK;
        }

        HRESULT Bind_ShaderResource(ShaderCom* shader, const char* name) const override
        {
            CHECK_NULL(_renderTarget, E_FAIL);
            return _renderTarget->Bind_ShaderResource(shader, name);
        }

        static Unique<IWorldUICommandSurface> Create(
            uint32 surfaceIndex,
            const ComPtr<Device>& device,
            const ComPtr<Context>& context,
            const Shared<ShaderCom>& shader,
            const Shared<VIBuffer_Rect>& viBuffer,
            const Vec2& pixelSize)
        {
            auto surface = make_unique<RenderTargetComposedBattleWorldCommandSurface>(
                device,
                context,
                shader,
                viBuffer,
                pixelSize);
            if (FAILED(surface->Initialize(surfaceIndex)))
                return nullptr;

            return surface;
        }

    private:
        HRESULT Draw_CompositionImage(const BattleWorldCommandImageDef& image)
        {
            CHECK_NULL(image.texture, E_FAIL);
            CHECK_NULL(_shader, E_FAIL);
            CHECK_NULL(_viBuffer, E_FAIL);

            const auto& srvs = image.texture->Get_SRVs();
            if (srvs.empty() || srvs[0] == nullptr)
                return E_FAIL;

            uint32 passIndex = 0u;
            if (image.imageRenderMode == BattleWorldCommandImageRenderMode::PatternOverlay)
            {
                if (!image.maskTexture || !image.patternMaskTexture)
                    return E_FAIL;

                const auto& maskSrvs = image.maskTexture->Get_SRVs();
                const auto& patternMaskSrvs = image.patternMaskTexture->Get_SRVs();
                if (maskSrvs.empty() || maskSrvs[0] == nullptr ||
                    patternMaskSrvs.empty() || patternMaskSrvs[0] == nullptr)
                {
                    return E_FAIL;
                }

                CHECK_FAILED(_shader->Bind_SRV("g_MaskTexture", maskSrvs[0].Get()), E_FAIL);
                CHECK_FAILED(_shader->Bind_SRV("g_PatternMaskTexture", patternMaskSrvs[0].Get()), E_FAIL);
                passIndex = 3u;
            }
            else if (image.maskTexture)
            {
                const auto& maskSrvs = image.maskTexture->Get_SRVs();
                if (maskSrvs.empty() || maskSrvs[0] == nullptr)
                    return E_FAIL;

                CHECK_FAILED(_shader->Bind_SRV("g_MaskTexture", maskSrvs[0].Get()), E_FAIL);
                passIndex = (image.maskTextureMode == BattleWorldCommandMaskTextureMode::BlackWhite) ? 5u : 1u;
            }

            const Vec4 drawColor = Resolve_ImageColor(image.color);
            const float viewportWidth = max(1.f, _pixelSize.x);
            const float viewportHeight = max(1.f, _pixelSize.y);

            Matrix worldMatrix =
                Matrix::CreateScale(image.size.x, image.size.y, 1.f) *
                Matrix::CreateRotationZ(-image.rotationRadians) *
                Matrix::CreateTranslation(
                    image.center.x - viewportWidth * 0.5f,
                    -image.center.y + viewportHeight * 0.5f,
                    0.f);

            Matrix viewMatrix = Matrix::Identity;
            Matrix projMatrix;
            XMStoreFloat4x4(&projMatrix, XMMatrixOrthographicLH(viewportWidth, viewportHeight, 0.f, 1.f));

            const Vec2 defaultPatternOffset = Vec2(0.f, 0.f);
            const Vec2 defaultPatternScale = Vec2(1.f, 1.f);
            const Vec2 defaultHoleCenter = Vec2(0.5f, 0.5f);
            const float defaultHoleRadius = 0.f;
            const float defaultHoleFeather = 0.05f;
            const float defaultViewportAspect = viewportWidth / viewportHeight;
            const float defaultFillRatio = 1.f;
            const float defaultGrayAmount = 0.f;
            const bool disabled = false;
            const uint32 fillOrigin = 0u;
            const Vec2 patternUvOffset =
                image.imageRenderMode == BattleWorldCommandImageRenderMode::PatternOverlay ? image.patternUvOffset : defaultPatternOffset;
            const Vec2 patternUvScale =
                image.imageRenderMode == BattleWorldCommandImageRenderMode::PatternOverlay ? image.patternUvScale : defaultPatternScale;

            CHECK_FAILED(_shader->Bind_Matrix("g_WorldMatrix", &worldMatrix), E_FAIL);
            CHECK_FAILED(_shader->Bind_Matrix("g_ViewMatrix", &viewMatrix), E_FAIL);
            CHECK_FAILED(_shader->Bind_Matrix("g_ProjMatrix", &projMatrix), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_Color", &drawColor, sizeof(Vec4)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_PatternUvOffset", &patternUvOffset, sizeof(Vec2)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_PatternUvScale", &patternUvScale, sizeof(Vec2)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_UseGrayScale", &disabled, sizeof(bool)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_GrayAmount", &defaultGrayAmount, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_IsOverlay", &disabled, sizeof(bool)), E_FAIL);
            CHECK_FAILED(_shader->Bind_SRV("g_Texture", srvs[0].Get()), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_HoleCenterUV", &defaultHoleCenter, sizeof(Vec2)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_HoleRadius", &defaultHoleRadius, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_HoleFeather", &defaultHoleFeather, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_ViewportAspect", &defaultViewportAspect, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_UseFillRatio", &disabled, sizeof(bool)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_FillRatio", &defaultFillRatio, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_FillOrigin", &fillOrigin, sizeof(uint32)), E_FAIL);

            CHECK_FAILED(_shader->Begin(passIndex), E_FAIL);
            const float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
            _context->OMSetBlendState(_compositionBlendState.Get(), blendFactor, 0xffffffff);
            CHECK_FAILED(_viBuffer->Bind_Resources(), E_FAIL);
            CHECK_FAILED(_viBuffer->Render(), E_FAIL);

            return S_OK;
        }

        void Draw_CompositionText(const BattleWorldCommandTextDef& text)
        {
            if (text.text.empty() || text.fontTag.empty())
                return;

            const float textScale = max(0.01f, text.scale);
            const float boxWidth = max(0.f, text.size.x * textScale);
            const Vec2 boxTopLeft(
                text.position.x - text.size.x * text.pivot.x * textScale,
                text.position.y - text.size.y * text.pivot.y * textScale);
            const Vec2 measured = GAME->Measure_Font(text.fontTag, text.text.c_str());
            const float textWidth = measured.x * textScale;

            float offsetX = 0.f;
            if (text.align == BattleWorldCommandTextAlign::Center)
                offsetX = (boxWidth - textWidth) * 0.5f;
            else if (text.align == BattleWorldCommandTextAlign::Right)
                offsetX = boxWidth - textWidth;

            const Vec2 alignedTopLeft(boxTopLeft.x + offsetX, boxTopLeft.y);
            const Vec2 pivot(
                (text.position.x - alignedTopLeft.x) / textScale,
                (text.position.y - alignedTopLeft.y) / textScale);

            const Vec4 drawColor = Resolve_TextColor(text.color);

            GAME->Draw_Font(
                text.fontTag,
                text.text.c_str(),
                text.position * static_cast<float>(kBattleWorldCommandCompositionRenderScale),
                XMVectorSet(drawColor.x, drawColor.y, drawColor.z, drawColor.w),
                text.rotationRadians,
                pivot,
                textScale * static_cast<float>(kBattleWorldCommandCompositionRenderScale));
        }

        ComPtr<Device> _device{};
        ComPtr<Context> _context{};
        Shared<ShaderCom> _shader{};
        Shared<VIBuffer_Rect> _viBuffer{};
        Shared<RenderTarget> _renderTarget{};
        ComPtr<ID3D11BlendState> _compositionBlendState{};
        Vec2 _pixelSize = Vec2(1.f, 1.f);
    };
}

struct BattleWorldUI_CommandCards::Impl
{
    Shared<ShaderCom> shader{};
    Shared<VIBuffer_Rect> viBuffer{};
    vector<BattleWorldCommandSurfaceTemplate> templates{};
    vector<BattleWorldCommandSurfaceInstance> surfaces{};
};

BattleWorldUI_CommandCards::BattleWorldUI_CommandCards(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

BattleWorldUI_CommandCards::~BattleWorldUI_CommandCards() = default;

HRESULT BattleWorldUI_CommandCards::Initialize(void* arg)
{
    CHECK_FAILED(GameObject::Initialize(arg), E_FAIL);

    _impl = make_unique<Impl>();
    CHECK_NULL(_impl, E_FAIL);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Shader_UI"), _impl->shader), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("VIBuffer_Rect"), _impl->viBuffer), E_FAIL);

    CHECK_FAILED(Try_LoadJsonCommandSurfaceTemplates(_device, _context, _impl->templates) ? S_OK : E_FAIL, E_FAIL);

    _impl->surfaces.clear();
    _impl->surfaces.reserve(_impl->templates.size());

    for (uint32 surfaceIndex = 0; surfaceIndex < _impl->templates.size(); ++surfaceIndex)
    {
        const BattleWorldCommandSurfaceTemplate& surfaceTemplate = _impl->templates[surfaceIndex];

        BattleWorldCommandSurfaceInstance surface{};
        surface.def = Build_SurfaceDef({}, surfaceTemplate);
        surface.surface = RenderTargetComposedBattleWorldCommandSurface::Create(
            surfaceIndex,
            _device,
            _context,
            _impl->shader,
            _impl->viBuffer,
            surface.def.pixelSize);
        CHECK_NULL(surface.surface, E_FAIL);

        _impl->surfaces.push_back(std::move(surface));
    }

    Hide();
    return S_OK;
}

void BattleWorldUI_CommandCards::Show_ForBattleState(
    const Vec3& anchorWorldPos,
    BattleTurnState currentState,
    uint32 curAp,
    uint32 maxAp)
{
    if (_impl == nullptr || !Should_ShowBattleWorldCommandCards(currentState))
    {
        Hide();
        return;
    }

    const BattleWorldCommandSurfaceContent content
    {
        min(curAp, kBattleWorldCommandMaxApSegmentCount),
        min(maxAp, kBattleWorldCommandMaxApSegmentCount)
    };

    bool anyVisible = false;
    for (uint32 surfaceIndex = 0; surfaceIndex < _impl->surfaces.size(); ++surfaceIndex)
    {
        BattleWorldCommandSurfaceInstance& surface = _impl->surfaces[surfaceIndex];
        surface.visible = false;

        if (surface.def.kind == BattleWorldCommandSurfaceKind::ApBar &&
            (surface.def.content.curAp != content.curAp || surface.def.content.maxAp != content.maxAp))
        {
            surface.def = Build_SurfaceDef(content, _impl->templates[surfaceIndex]);
            surface.dirty = true;
        }

        if (surface.dirty)
        {
            if (surface.surface == nullptr || FAILED(surface.surface->Compose(surface.def)))
                continue;

            surface.dirty = false;
        }

        const Vec3 position = anchorWorldPos + surface.def.worldOffset;
        surface.worldMatrix =
            Matrix::CreateScale(surface.def.worldSize.x, surface.def.worldSize.y, 1.f) *
            Matrix::CreateRotationZ(-surface.def.rotationRadians) *
            Matrix::CreateTranslation(position.x, position.y, position.z);
        surface.visible = true;
        anyVisible = true;
    }

    Set_Visible(anyVisible);
}

void BattleWorldUI_CommandCards::Hide()
{
    Set_Visible(false);

    if (_impl == nullptr)
        return;

    for (auto& surface : _impl->surfaces)
        surface.visible = false;
}

void BattleWorldUI_CommandCards::Late_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!Is_Visible())
        return;

    GAME->Add_RenderGroup(RenderGroup::UIEffect, GetSharedPtr<GameObject>());
}

HRESULT BattleWorldUI_CommandCards::Render()
{
    if (!Is_Visible() || _impl == nullptr || _impl->shader == nullptr || _impl->viBuffer == nullptr)
        return S_OK;

    const Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);

    for (const auto& surface : _impl->surfaces)
    {
        if (!surface.visible || surface.surface == nullptr)
            continue;

        CHECK_FAILED(Bind_WorldUIPlaneShaderState(_impl->shader.get(), surface.worldMatrix, color), E_FAIL);
        CHECK_FAILED(surface.surface->Bind_ShaderResource(_impl->shader.get(), "g_Texture"), E_FAIL);
        CHECK_FAILED(_impl->shader->Begin(0), E_FAIL);
        CHECK_FAILED(_impl->viBuffer->Bind_Resources(), E_FAIL);
        CHECK_FAILED(_impl->viBuffer->Render(), E_FAIL);
    }

    return S_OK;
}

Shared<GameObject> BattleWorldUI_CommandCards::Clone(void* arg)
{
    UNREFERENCED_PARAMETER(arg);
    return nullptr;
}

void BattleWorldUI_CommandCards::Free()
{
    if (_impl)
    {
        _impl->surfaces.clear();
        _impl->templates.clear();
        _impl->shader.reset();
        _impl->viBuffer.reset();
        _impl.reset();
    }

    __super::Free();
}

Shared<BattleWorldUI_CommandCards> BattleWorldUI_CommandCards::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context)
{
    auto instance = make_shared<BattleWorldUI_CommandCards>(device, context);
    if (FAILED(instance->Initialize(nullptr)))
    {
        instance->Free();
        return nullptr;
    }

    return instance;
}

NS_END
