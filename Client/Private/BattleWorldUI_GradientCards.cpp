#include "BattleWorldUI_GradientCards.h"

#include "BattleWorldUI_JsonTextureResolver.h"
#include "BattleWorldUI_PlacementUtils.h"
#include "BattleWorldUI_SurfaceAnim.h"
#include "Camera.h"
#include "ClientInstance.h"
#include "Debug_Profile.h"
#include "RenderTarget.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "TransformCom.h"
#include "VIBuffer_Rect.h"

NS_BEGIN(Client)

namespace
{
    constexpr wchar_t kBattleWorldGradientGustaveJsonPath[] =
        L"UI/UI_Json/BattleSystem/Gustave/UI_BattleSystem_Gradient_Gustave.json";
    constexpr wchar_t kBattleWorldGradientLuneJsonPath[] =
        L"UI/UI_Json/BattleSystem/Lune/UI_BattleSystem_Gradient_Lune.json";
    constexpr wchar_t kBattleWorldGradientMaelleJsonPath[] =
        L"UI/UI_Json/BattleSystem/Maelle/UI_BattleSystem_Gradient_Maelle.json";
    constexpr wchar_t kBattleWorldGradientPlacementJsonPath[] =
        L"UI/UI_Json/BattleSystem/UI_BattleSystem_Gradient_WorldPlacement.json";
    constexpr uint32 kBattleWorldGradientPlacementHeroCount = static_cast<uint32>(HeroId::END);
    constexpr uint32 kBattleWorldGradientFallbackPixelWidth = 900;
    constexpr uint32 kBattleWorldGradientFallbackPixelHeight = 550;
    constexpr float kBattleWorldGradientCompositionRenderScale = 2.f;
    constexpr float kBattleWorldGradientDegreesToRadians = 0.017453292519943295f;
    constexpr float kBattleWorldGradientRadiansToDegrees = 57.29577951308232f;
    const Vec2 kBattleWorldGradientDefaultWorldSize = Vec2(1.50f, 0.92f);
    constexpr float kBattleWorldGradientDefaultScale = 1.f;

    struct BattleWorldGradientHeroDef
    {
        HeroId heroId = HeroId::END;
        const wchar_t* jsonPath = L"";
        const char* rootPanelName = "";
        const wchar_t* surfaceName = L"";
    };

    const BattleWorldGradientHeroDef kBattleWorldGradientHeroDefs[kBattleWorldGradientPlacementHeroCount] =
    {
        {
            HeroId::Gustave,
            kBattleWorldGradientGustaveJsonPath,
            "Panel_BattleSystem_Gradient_Gustave",
            L"BattleWorldGradientSurface_Gustave"
        },
        {
            HeroId::Lune,
            kBattleWorldGradientLuneJsonPath,
            "Panel_BattleSystem_Gradient_Lune",
            L"BattleWorldGradientSurface_Lune"
        },
        {
            HeroId::Maelle,
            kBattleWorldGradientMaelleJsonPath,
            "Panel_BattleSystem_Gradient_Maelle",
            L"BattleWorldGradientSurface_Maelle"
        },
    };

    enum class BattleWorldGradientLayerKind
    {
        Image,
        Text,
    };

    enum class BattleWorldGradientTextAlign
    {
        Left,
        Center,
        Right,
    };

    enum class BattleWorldGradientImageRenderMode
    {
        Normal,
        PatternOverlay,
        Unknown,
    };

    enum class BattleWorldGradientMaskTextureMode
    {
        Alpha,
        BlackWhite,
        Unknown,
    };

    struct BattleWorldGradientTextDef
    {
        wstring text{};
        wstring fontTag{};
        Vec2 size{};
        Vec2 pivot = Vec2(0.f, 0.f);
        Vec2 position{};
        float scale = 1.f;
        float rotationRadians = 0.f;
        Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);
        BattleWorldGradientTextAlign align = BattleWorldGradientTextAlign::Left;
        bool textWrap = false;
    };

    struct BattleWorldGradientImageDef
    {
        Shared<Texture> texture{};
        Vec2 center{};
        Vec2 size{};
        Vec2 pivot = Vec2(0.5f, 0.5f);
        float rotationRadians = 0.f;
        Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);
        BattleWorldGradientImageRenderMode imageRenderMode = BattleWorldGradientImageRenderMode::Normal;
        Shared<Texture> maskTexture{};
        BattleWorldGradientMaskTextureMode maskTextureMode = BattleWorldGradientMaskTextureMode::Alpha;
        Shared<Texture> patternMaskTexture{};
        Vec2 patternUvOffset = Vec2(0.f, 0.f);
        Vec2 patternUvScale = Vec2(1.f, 1.f);
    };

    struct BattleWorldGradientPanelTemplate
    {
        struct Layer
        {
            BattleWorldGradientLayerKind kind = BattleWorldGradientLayerKind::Image;
            int32 sortOrder = 0;
            uint32 sequence = 0;
            BattleWorldGradientImageDef image{};
            BattleWorldGradientTextDef text{};
        };

        Vec2 pixelSize = Vec2(
            static_cast<float>(kBattleWorldGradientFallbackPixelWidth),
            static_cast<float>(kBattleWorldGradientFallbackPixelHeight));
        vector<Layer> layers{};
        bool valid = false;
    };

    class IWorldUIGradientSurface;

    struct BattleWorldGradientPanelInstance
    {
        BattleWorldGradientPanelTemplate panelTemplate{};
        Unique<IWorldUIGradientSurface> surface{};
        Matrix worldMatrix = Matrix::Identity;
        BattleWorldSurfaceAnimState introAnim{};
        Vec3 introStartOffset = Vec3::Zero;
        bool visible = false;
        bool dirty = true;
    };

    struct BattleWorldGradientWorldPlacementTuning
    {
        Vec3 screenPosition = Vec3::Zero;
        Vec3 baseOffset = Vec3::Zero;
        Vec2 worldSize = kBattleWorldGradientDefaultWorldSize;
        float visualScale = kBattleWorldGradientDefaultScale;
        float pitchRadians = 0.f;
        float rollRadians = 0.f;
        float yawOffsetRadians = 0.f;
    };

    struct BattleWorldGradientJsonObject
    {
        uint32 sequence = 0;
        int32 id = 0;
        int32 parentId = 0;
        int32 sortOrder = 0;
        string name{};
        string type{};
        string textureGuid{};
        string texturePath{};
        string maskTextureGuid{};
        string maskTexturePath{};
        string maskTextureMode = "Alpha";
        string imageRenderMode = "Normal";
        string patternMaskTextureGuid{};
        string patternMaskTexturePath{};
        string fontTag{};
        string fontPath{};
        string text{};
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

    using BattleWorldGradientJsonObjectMap = unordered_map<int32, BattleWorldGradientJsonObject>;

    bool Should_ShowBattleWorldGradientCards(BattleTurnState state)
    {
        return state == BattleTurnState::ChooseGradient ||
            state == BattleTurnState::SkillTargeting;
    }

    HeroId Normalize_PlacementHeroId(HeroId heroId)
    {
        const uint32 heroIndex = static_cast<uint32>(heroId);
        if (heroIndex < kBattleWorldGradientPlacementHeroCount)
            return heroId;

        return HeroId::Gustave;
    }

    uint32 Resolve_PlacementHeroIndex(HeroId heroId)
    {
        return static_cast<uint32>(Normalize_PlacementHeroId(heroId));
    }

    const char* To_PlacementHeroJsonName(HeroId heroId)
    {
        switch (Normalize_PlacementHeroId(heroId))
        {
        case HeroId::Gustave:
            return "Gustave";
        case HeroId::Lune:
            return "Lune";
        case HeroId::Maelle:
            return "Maelle";
        case HeroId::END:
        default:
            return "Gustave";
        }
    }

    bool Try_ParsePlacementHeroJsonName(const string& heroName, HeroId& outHeroId)
    {
        const string lowerName = String::ToLowerCopy(heroName);
        if (lowerName == "gustave")
        {
            outHeroId = HeroId::Gustave;
            return true;
        }

        if (lowerName == "lune")
        {
            outHeroId = HeroId::Lune;
            return true;
        }

        if (lowerName == "maelle")
        {
            outHeroId = HeroId::Maelle;
            return true;
        }

        return false;
    }

    BattleWorldGradientWorldPlacementTuning Make_DefaultGradientWorldPlacementTuning(HeroId heroId)
    {
        BattleWorldGradientWorldPlacementTuning tuning{};
        tuning.worldSize = kBattleWorldGradientDefaultWorldSize;

        switch (Normalize_PlacementHeroId(heroId))
        {
        case HeroId::Gustave:
            tuning.screenPosition = Vec3(3.30f, 0.37f, 2.22f);
            tuning.baseOffset = Vec3(3.18f, 1.22f, 0.f);
            tuning.yawOffsetRadians = -8.f * kBattleWorldGradientDegreesToRadians;
            break;
        case HeroId::Lune:
            tuning.screenPosition = Vec3(2.77f, 0.47f, 1.20f);
            tuning.baseOffset = Vec3(2.13f, 1.26f, -1.12f);
            tuning.yawOffsetRadians = 6.f * kBattleWorldGradientDegreesToRadians;
            break;
        case HeroId::Maelle:
            tuning.screenPosition = Vec3(4.09f, 0.48f, 1.91f);
            tuning.baseOffset = Vec3(3.52f, 1.26f, -1.12f);
            tuning.yawOffsetRadians = -13.f * kBattleWorldGradientDegreesToRadians;
            break;
        case HeroId::END:
        default:
            break;
        }

        return tuning;
    }

    array<BattleWorldGradientWorldPlacementTuning, kBattleWorldGradientPlacementHeroCount>
        Make_DefaultGradientWorldPlacementTunings()
    {
        array<BattleWorldGradientWorldPlacementTuning, kBattleWorldGradientPlacementHeroCount> tunings{};
        for (uint32 heroIndex = 0; heroIndex < kBattleWorldGradientPlacementHeroCount; ++heroIndex)
            tunings[heroIndex] = Make_DefaultGradientWorldPlacementTuning(static_cast<HeroId>(heroIndex));

        return tunings;
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
        if (iter == node.end() || !iter->is_number_integer())
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

    Vec3 Read_JsonVec3(const json& node, const char* key, const Vec3& fallback)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_array() || iter->size() < 3)
            return fallback;

        return Vec3(
            iter->at(0).is_number() ? iter->at(0).get<float>() : fallback.x,
            iter->at(1).is_number() ? iter->at(1).get<float>() : fallback.y,
            iter->at(2).is_number() ? iter->at(2).get<float>() : fallback.z);
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

    BattleWorldGradientImageRenderMode Parse_JsonImageRenderMode(const string& modeName)
    {
        if (modeName.empty())
            return BattleWorldGradientImageRenderMode::Normal;

        const string mode = String::ToLowerCopy(modeName);
        if ("normal" == mode)
            return BattleWorldGradientImageRenderMode::Normal;
        if ("patternoverlay" == mode || "pattern_overlay" == mode || "pattern overlay" == mode)
            return BattleWorldGradientImageRenderMode::PatternOverlay;

        return BattleWorldGradientImageRenderMode::Unknown;
    }

    BattleWorldGradientMaskTextureMode Parse_JsonMaskTextureMode(const string& modeName)
    {
        if (modeName.empty())
            return BattleWorldGradientMaskTextureMode::Alpha;

        const string mode = String::ToLowerCopy(modeName);
        if ("alpha" == mode)
            return BattleWorldGradientMaskTextureMode::Alpha;
        if ("blackwhite" == mode || "black_white" == mode || "black white" == mode)
            return BattleWorldGradientMaskTextureMode::BlackWhite;

        return BattleWorldGradientMaskTextureMode::Unknown;
    }

    const char* To_LogName(BattleWorldGradientImageRenderMode mode)
    {
        switch (mode)
        {
        case BattleWorldGradientImageRenderMode::Normal:
            return "Normal";
        case BattleWorldGradientImageRenderMode::PatternOverlay:
            return "PatternOverlay";
        case BattleWorldGradientImageRenderMode::Unknown:
        default:
            return "Unknown";
        }
    }

    const char* To_LogName(BattleWorldGradientMaskTextureMode mode)
    {
        switch (mode)
        {
        case BattleWorldGradientMaskTextureMode::Alpha:
            return "Alpha";
        case BattleWorldGradientMaskTextureMode::BlackWhite:
            return "BlackWhite";
        case BattleWorldGradientMaskTextureMode::Unknown:
        default:
            return "Unknown";
        }
    }

    bool Has_TextureRef(const string& textureGuid, const string& texturePath)
    {
        return !textureGuid.empty() || !texturePath.empty();
    }

    bool Should_SkipSharedBattleCommandBackgroundPaint(const string& texturePath)
    {
        if (texturePath.empty())
            return false;

        string loweredPath = String::ToLowerCopy(texturePath);
        replace(loweredPath.begin(), loweredPath.end(), '\\', '/');

        return loweredPath.find("/battle/battlewheel/items/t_bw_items_backgroundpaint") != string::npos ||
            loweredPath.find("/battle/battlewheel/items/t_bw_items_paint_background") != string::npos ||
            loweredPath.find("/battle/battlewheel/gradientattack/t_bw_gradientattack_backgroundpaint") != string::npos ||
            loweredPath.find("/battle/battlewheel/gradientattack/t_bw_gradientattack_paint_background") != string::npos;
    }

    BattleWorldGradientJsonObject Parse_JsonObject(const json& node, uint32 sequence)
    {
        BattleWorldGradientJsonObject object{};
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
        const BattleWorldGradientJsonObject& parent)
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

    const BattleWorldGradientJsonObject* Find_JsonObject(
        const BattleWorldGradientJsonObjectMap& objectsById,
        int32 objectId)
    {
        const auto iter = objectsById.find(objectId);
        if (iter == objectsById.end())
            return nullptr;

        return &iter->second;
    }

    bool Is_DescendantOf(
        const BattleWorldGradientJsonObject& object,
        int32 panelId,
        const BattleWorldGradientJsonObjectMap& objectsById)
    {
        int32 parentId = object.parentId;
        for (uint32 depth = 0; depth < 256 && parentId != 0; ++depth)
        {
            if (parentId == panelId)
                return true;

            const BattleWorldGradientJsonObject* parent = Find_JsonObject(objectsById, parentId);
            if (parent == nullptr)
                break;

            parentId = parent->parentId;
        }

        return false;
    }

    bool Is_VisibleInPanelHierarchy(
        const BattleWorldGradientJsonObject& object,
        int32 panelId,
        const BattleWorldGradientJsonObjectMap& objectsById)
    {
        if (!object.visible)
            return false;

        int32 parentId = object.parentId;
        for (uint32 depth = 0; depth < 256 && parentId != 0; ++depth)
        {
            if (parentId == panelId)
                return true;

            const BattleWorldGradientJsonObject* parent = Find_JsonObject(objectsById, parentId);
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
        const BattleWorldGradientJsonObjectMap& objectsById)
    {
        for (uint32 depth = 0; depth < 256 && parentId != 0 && parentId != panelId; ++depth)
        {
            const BattleWorldGradientJsonObject* parent = Find_JsonObject(objectsById, parentId);
            if (parent == nullptr)
                break;

            point = Transform_PointByJsonParent(point, *parent);
            parentId = parent->parentId;
        }

        return point;
    }

    Vec2 Resolve_JsonWorldScale(
        const BattleWorldGradientJsonObject& object,
        int32 panelId,
        const BattleWorldGradientJsonObjectMap& objectsById)
    {
        Vec2 scale = object.scale;
        int32 parentId = object.parentId;

        for (uint32 depth = 0; depth < 256 && parentId != 0 && parentId != panelId; ++depth)
        {
            const BattleWorldGradientJsonObject* parent = Find_JsonObject(objectsById, parentId);
            if (parent == nullptr)
                break;

            scale.x *= parent->scale.x;
            scale.y *= parent->scale.y;
            parentId = parent->parentId;
        }

        return scale;
    }

    float Resolve_JsonWorldRotation(
        const BattleWorldGradientJsonObject& object,
        int32 panelId,
        const BattleWorldGradientJsonObjectMap& objectsById)
    {
        float rotation = object.rotationRadians;
        int32 parentId = object.parentId;

        for (uint32 depth = 0; depth < 256 && parentId != 0 && parentId != panelId; ++depth)
        {
            const BattleWorldGradientJsonObject* parent = Find_JsonObject(objectsById, parentId);
            if (parent == nullptr)
                break;

            rotation += parent->rotationRadians;
            parentId = parent->parentId;
        }

        return rotation;
    }

    float Resolve_JsonAlphaMultiplier(
        const BattleWorldGradientJsonObject& object,
        int32 panelId,
        const BattleWorldGradientJsonObjectMap& objectsById)
    {
        float alphaMultiplier = object.alphaMultiplier;
        int32 parentId = object.parentId;

        for (uint32 depth = 0; depth < 256 && parentId != 0 && parentId != panelId; ++depth)
        {
            const BattleWorldGradientJsonObject* parent = Find_JsonObject(objectsById, parentId);
            if (parent == nullptr)
                break;

            alphaMultiplier *= parent->alphaMultiplier;
            parentId = parent->parentId;
        }

        return alphaMultiplier;
    }

    Vec2 Resolve_JsonImageCenter(const BattleWorldGradientJsonObject& object)
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

    BattleWorldGradientTextAlign Resolve_TextAlign(const string& textAlign)
    {
        const string normalizedTextAlign = String::ToLowerCopy(textAlign);
        if (normalizedTextAlign == "center")
            return BattleWorldGradientTextAlign::Center;
        if (normalizedTextAlign == "right")
            return BattleWorldGradientTextAlign::Right;

        return BattleWorldGradientTextAlign::Left;
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

    bool Try_ExtractJsonImageLayer(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const BattleWorldGradientJsonObject& object,
        const char* panelName,
        int32 panelId,
        const BattleWorldGradientJsonObjectMap& objectsById,
        BattleWorldGradientPanelTemplate::Layer& outLayer)
    {
        BattleWorldGradientImageRenderMode imageRenderMode =
            Parse_JsonImageRenderMode(object.imageRenderMode);
        const BattleWorldGradientMaskTextureMode maskTextureMode =
            Parse_JsonMaskTextureMode(object.maskTextureMode);

        if (Should_SkipSharedBattleCommandBackgroundPaint(object.texturePath))
            return false;

        if (imageRenderMode == BattleWorldGradientImageRenderMode::Normal &&
            Has_TextureRef(object.patternMaskTextureGuid, object.patternMaskTexturePath))
        {
            imageRenderMode = BattleWorldGradientImageRenderMode::PatternOverlay;
        }

        if (imageRenderMode == BattleWorldGradientImageRenderMode::Unknown)
        {
            LOG_WARN(
                "BattleWorldUI gradient json image skipped: unknown imageRenderMode. panel='{}', name='{}', mode='{}'",
                panelName,
                object.name,
                object.imageRenderMode);
            return false;
        }

        if (object.useFillRatio)
        {
            LOG_DEBUG(
                "BattleWorldUI gradient json image skipped: unsupported fillRatio. panel='{}', name='{}', fillRatio={}",
                panelName,
                object.name,
                object.fillRatio);
            return false;
        }

        if (imageRenderMode != BattleWorldGradientImageRenderMode::PatternOverlay &&
            Has_TextureRef(object.maskTextureGuid, object.maskTexturePath) &&
            maskTextureMode == BattleWorldGradientMaskTextureMode::Unknown)
        {
            LOG_WARN(
                "BattleWorldUI gradient json image skipped: unknown maskTextureMode. panel='{}', name='{}', mode='{}'",
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
        const bool requiresMaskTexture = hasMaskRef || imageRenderMode == BattleWorldGradientImageRenderMode::PatternOverlay;
        if (requiresMaskTexture)
        {
            if (!hasMaskRef)
            {
                LOG_WARN(
                    "BattleWorldUI gradient json image skipped: missing required mask ref. panel='{}', name='{}', mode='{}'",
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
                    "BattleWorldUI gradient json image skipped: missing required mask. panel='{}', name='{}', maskPath='{}'",
                    panelName,
                    object.name,
                    object.maskTexturePath);
                return false;
            }
        }

        Shared<Texture> patternMaskTexture{};
        const bool hasPatternMaskRef = Has_TextureRef(object.patternMaskTextureGuid, object.patternMaskTexturePath);
        if (imageRenderMode == BattleWorldGradientImageRenderMode::PatternOverlay)
        {
            if (!hasPatternMaskRef)
            {
                LOG_WARN(
                    "BattleWorldUI gradient json image skipped: missing required pattern mask ref. panel='{}', name='{}'",
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
                    "BattleWorldUI gradient json image skipped: missing required pattern mask. panel='{}', name='{}', patternMaskPath='{}'",
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

        outLayer.kind = BattleWorldGradientLayerKind::Image;
        outLayer.sortOrder = object.sortOrder;
        outLayer.sequence = object.sequence;
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

        LOG_DEBUG(
            "BattleWorldUI gradient json image supported. panel='{}', name='{}', sort={}, mode='{}', maskMode='{}', hasMask={}, hasPatternMask={}",
            panelName,
            object.name,
            object.sortOrder,
            To_LogName(imageRenderMode),
            To_LogName(maskTextureMode),
            hasMaskRef,
            hasPatternMaskRef);

        return true;
    }

    bool Try_ExtractJsonTextLayer(
        const BattleWorldGradientJsonObject& object,
        int32 panelId,
        const BattleWorldGradientJsonObjectMap& objectsById,
        BattleWorldGradientPanelTemplate::Layer& outLayer)
    {
        if (object.text.empty())
            return false;

        if (!Ensure_JsonFontLoaded(object.fontTag, object.fontPath))
            return false;

        const Vec2 worldScale = Resolve_JsonWorldScale(object, panelId, objectsById);
        const Vec2 panelLocalPosition = Transform_PointToPanelLocal(object.position, object.parentId, panelId, objectsById);

        Vec4 color = object.color;
        color.w *= Resolve_JsonAlphaMultiplier(object, panelId, objectsById);

        outLayer.kind = BattleWorldGradientLayerKind::Text;
        outLayer.sortOrder = object.sortOrder;
        outLayer.sequence = object.sequence;
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

    bool Try_LoadJsonGradientPanelTemplate(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const BattleWorldGradientHeroDef& heroDef,
        BattleWorldGradientPanelTemplate& outTemplate)
    {
        const fs::path jsonPath = (fs::path(GAME->Get_AssetRoot()) / heroDef.jsonPath).lexically_normal();
        if (!fs::exists(jsonPath))
        {
            LOG_WARN("BattleWorldUI gradient json template missing. path='{}'", String::ToString(jsonPath.wstring()));
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
            LOG_WARN("BattleWorldUI gradient json template parse failed. path='{}', error='{}'",
                String::ToString(jsonPath.wstring()),
                exception.what());
            return false;
        }

        const auto objectsIter = root.find("objects");
        if (objectsIter == root.end() || !objectsIter->is_array())
            return false;

        vector<BattleWorldGradientJsonObject> objects{};
        BattleWorldGradientJsonObjectMap objectsById{};
        objects.reserve(objectsIter->size());

        uint32 sequence = 0;
        for (const auto& node : *objectsIter)
        {
            if (!node.is_object())
                continue;

            BattleWorldGradientJsonObject object = Parse_JsonObject(node, sequence++);
            objectsById.emplace(object.id, object);
            objects.push_back(std::move(object));
        }

        const auto panelIter = find_if(objects.begin(), objects.end(),
            [&](const BattleWorldGradientJsonObject& object)
            {
                return object.name == heroDef.rootPanelName;
            });

        if (panelIter == objects.end() || !panelIter->visible)
        {
            LOG_WARN("BattleWorldUI gradient json panel missing. panel='{}'", heroDef.rootPanelName);
            return false;
        }

        BattleWorldGradientPanelTemplate panelTemplate{};
        panelTemplate.pixelSize = Vec2(
            max(1.f, panelIter->size.x),
            max(1.f, panelIter->size.y));

        for (const BattleWorldGradientJsonObject& object : objects)
        {
            if (!Is_DescendantOf(object, panelIter->id, objectsById))
                continue;

            if (!Is_VisibleInPanelHierarchy(object, panelIter->id, objectsById))
                continue;

            BattleWorldGradientPanelTemplate::Layer layer{};
            bool extracted = false;

            if (object.type == "Image")
                extracted = Try_ExtractJsonImageLayer(device, context, object, heroDef.rootPanelName, panelIter->id, objectsById, layer);
            else if (object.type == "Text")
                extracted = Try_ExtractJsonTextLayer(object, panelIter->id, objectsById, layer);

            if (extracted)
                panelTemplate.layers.push_back(std::move(layer));
        }

        if (panelTemplate.layers.empty())
        {
            LOG_WARN("BattleWorldUI gradient json panel has no extracted layers. panel='{}'", heroDef.rootPanelName);
            return false;
        }

        sort(panelTemplate.layers.begin(), panelTemplate.layers.end(),
            [](const BattleWorldGradientPanelTemplate::Layer& lhs, const BattleWorldGradientPanelTemplate::Layer& rhs)
            {
                if (lhs.sortOrder != rhs.sortOrder)
                    return lhs.sortOrder < rhs.sortOrder;

                return lhs.sequence < rhs.sequence;
            });

        panelTemplate.valid = true;
        outTemplate = std::move(panelTemplate);
        return true;
    }

    fs::path Resolve_GradientWorldPlacementJsonPath()
    {
        return (fs::path(GAME->Get_AssetRoot()) / kBattleWorldGradientPlacementJsonPath).lexically_normal();
    }

    BattleWorldGradientWorldPlacementTuning Read_GradientWorldPlacementTuningNode(
        const json& root,
        const BattleWorldGradientWorldPlacementTuning& fallback)
    {
        BattleWorldGradientWorldPlacementTuning tuning = fallback;
        tuning.screenPosition = Read_JsonVec3(root, "screenPosition", tuning.screenPosition);
        tuning.baseOffset = Read_JsonVec3(root, "baseOffset", tuning.baseOffset);
        tuning.worldSize = Read_JsonVec2(root, "worldSize", tuning.worldSize);
        tuning.visualScale = Read_JsonFloat(root, "visualScale", tuning.visualScale);
        tuning.pitchRadians =
            Read_JsonFloat(root, "pitchDegrees", tuning.pitchRadians * kBattleWorldGradientRadiansToDegrees) *
            kBattleWorldGradientDegreesToRadians;
        tuning.rollRadians =
            Read_JsonFloat(root, "rollDegrees", tuning.rollRadians * kBattleWorldGradientRadiansToDegrees) *
            kBattleWorldGradientDegreesToRadians;
        tuning.yawOffsetRadians =
            Read_JsonFloat(root, "yawOffsetDegrees", tuning.yawOffsetRadians * kBattleWorldGradientRadiansToDegrees) *
            kBattleWorldGradientDegreesToRadians;
        return tuning;
    }

    bool Try_LoadGradientWorldPlacementTunings(
        const fs::path& jsonPath,
        array<BattleWorldGradientWorldPlacementTuning, kBattleWorldGradientPlacementHeroCount>& outTunings)
    {
        if (!fs::exists(jsonPath))
            return false;

        json root{};
        try
        {
            ifstream file(jsonPath);
            file >> root;
        }
        catch (const exception& e)
        {
            LOG_WARN("BattleWorldUI gradient placement json parse failed. path='{}', error='{}'",
                String::ToString(jsonPath.wstring()),
                e.what());
            return false;
        }

        auto tunings = Make_DefaultGradientWorldPlacementTunings();
        if (const auto charactersIter = root.find("characters");
            charactersIter != root.end() && charactersIter->is_object())
        {
            for (const auto& [heroName, heroNode] : charactersIter->items())
            {
                if (!heroNode.is_object())
                    continue;

                HeroId heroId = HeroId::END;
                if (!Try_ParsePlacementHeroJsonName(heroName, heroId))
                {
                    LOG_WARN("BattleWorldUI gradient placement json has unknown character. character='{}'", heroName);
                    continue;
                }

                const uint32 heroIndex = Resolve_PlacementHeroIndex(heroId);
                tunings[heroIndex] = Read_GradientWorldPlacementTuningNode(heroNode, tunings[heroIndex]);
            }
        }
        else
        {
            const BattleWorldGradientWorldPlacementTuning flatTuning =
                Read_GradientWorldPlacementTuningNode(root, BattleWorldGradientWorldPlacementTuning{});
            for (auto& tuning : tunings)
                tuning = flatTuning;
        }

        outTunings = tunings;
        return true;
    }

    json To_JsonVec2(const Vec2& value)
    {
        return json::array({ value.x, value.y });
    }

    json To_JsonVec3(const Vec3& value)
    {
        return json::array({ value.x, value.y, value.z });
    }

    json To_GradientWorldPlacementTuningJson(const BattleWorldGradientWorldPlacementTuning& tuning)
    {
        json root = json::object();
        root["screenPosition"] = To_JsonVec3(tuning.screenPosition);
        root["baseOffset"] = To_JsonVec3(tuning.baseOffset);
        root["worldSize"] = To_JsonVec2(tuning.worldSize);
        root["visualScale"] = tuning.visualScale;
        root["pitchDegrees"] = tuning.pitchRadians * kBattleWorldGradientRadiansToDegrees;
        root["rollDegrees"] = tuning.rollRadians * kBattleWorldGradientRadiansToDegrees;
        root["yawOffsetDegrees"] = tuning.yawOffsetRadians * kBattleWorldGradientRadiansToDegrees;
        return root;
    }

    bool Write_GradientWorldPlacementTuningsToFile(
        const array<BattleWorldGradientWorldPlacementTuning, kBattleWorldGradientPlacementHeroCount>& tunings,
        fs::file_time_type* outWriteTime)
    {
        const fs::path jsonPath = Resolve_GradientWorldPlacementJsonPath();

        json root = json::object();
        root["characters"] = json::object();
        for (uint32 heroIndex = 0; heroIndex < kBattleWorldGradientPlacementHeroCount; ++heroIndex)
        {
            const HeroId heroId = static_cast<HeroId>(heroIndex);
            root["characters"][To_PlacementHeroJsonName(heroId)] =
                To_GradientWorldPlacementTuningJson(tunings[heroIndex]);
        }

        error_code errorCode{};
        fs::create_directories(jsonPath.parent_path(), errorCode);
        if (errorCode)
        {
            LOG_WARN("BattleWorldUI gradient placement json directory create failed. path='{}'",
                String::ToString(jsonPath.parent_path().wstring()));
            return false;
        }

        try
        {
            ofstream file(jsonPath);
            if (!file.is_open())
            {
                LOG_WARN("BattleWorldUI gradient placement json open failed. path='{}'",
                    String::ToString(jsonPath.wstring()));
                return false;
            }

            file << root.dump(2) << '\n';
        }
        catch (const exception& e)
        {
            LOG_WARN("BattleWorldUI gradient placement json write failed. path='{}', error='{}'",
                String::ToString(jsonPath.wstring()),
                e.what());
            return false;
        }

        if (outWriteTime != nullptr)
        {
            const fs::file_time_type writeTime = fs::last_write_time(jsonPath, errorCode);
            if (!errorCode)
                *outWriteTime = writeTime;
        }

        LOG_INFO("BattleWorldUI gradient placement json saved. path='{}'",
            String::ToString(jsonPath.wstring()));
        return true;
    }

    RuntimeWorldUIPlacementTuning To_EditorWorldPlacementTuning(
        const BattleWorldGradientWorldPlacementTuning& tuning)
    {
        RuntimeWorldUIPlacementTuning editorTuning{};
        editorTuning.screenPosition = tuning.screenPosition;
        editorTuning.baseOffset = tuning.baseOffset;
        editorTuning.showKeyOffsets = false;
        editorTuning.worldSize = tuning.worldSize;
        editorTuning.visualScale = tuning.visualScale;
        editorTuning.pitchDegrees = tuning.pitchRadians * kBattleWorldGradientRadiansToDegrees;
        editorTuning.rollDegrees = tuning.rollRadians * kBattleWorldGradientRadiansToDegrees;
        editorTuning.yawOffsetDegrees = tuning.yawOffsetRadians * kBattleWorldGradientRadiansToDegrees;
        return editorTuning;
    }

    BattleWorldGradientWorldPlacementTuning From_EditorWorldPlacementTuning(
        const RuntimeWorldUIPlacementTuning& editorTuning)
    {
        BattleWorldGradientWorldPlacementTuning tuning{};
        tuning.screenPosition = editorTuning.screenPosition;
        tuning.baseOffset = editorTuning.baseOffset;
        tuning.worldSize = Vec2(
            max(0.01f, editorTuning.worldSize.x),
            max(0.01f, editorTuning.worldSize.y));
        tuning.visualScale = max(0.01f, editorTuning.visualScale);
        tuning.pitchRadians = editorTuning.pitchDegrees * kBattleWorldGradientDegreesToRadians;
        tuning.rollRadians = editorTuning.rollDegrees * kBattleWorldGradientDegreesToRadians;
        tuning.yawOffsetRadians = editorTuning.yawOffsetDegrees * kBattleWorldGradientDegreesToRadians;
        return tuning;
    }

    void Reload_GradientWorldPlacementTuningIfNeeded(
        array<BattleWorldGradientWorldPlacementTuning, kBattleWorldGradientPlacementHeroCount>& tunings,
        fs::file_time_type& knownWriteTime,
        bool& hasKnownFile)
    {
        const fs::path jsonPath = Resolve_GradientWorldPlacementJsonPath();
        if (!fs::exists(jsonPath))
        {
            if (hasKnownFile)
            {
                tunings = Make_DefaultGradientWorldPlacementTunings();
                knownWriteTime = {};
                hasKnownFile = false;
                LOG_INFO("BattleWorldUI gradient placement json removed. using defaults.");
            }
            return;
        }

        error_code errorCode{};
        const fs::file_time_type writeTime = fs::last_write_time(jsonPath, errorCode);
        if (errorCode)
            return;

        if (hasKnownFile && writeTime == knownWriteTime)
            return;

        array<BattleWorldGradientWorldPlacementTuning, kBattleWorldGradientPlacementHeroCount> loadedTunings{};
        knownWriteTime = writeTime;
        hasKnownFile = true;

        if (!Try_LoadGradientWorldPlacementTunings(jsonPath, loadedTunings))
            return;

        tunings = loadedTunings;
        LOG_INFO("BattleWorldUI gradient placement json reloaded. path='{}'",
            String::ToString(jsonPath.wstring()));
    }

    Matrix Resolve_CameraFacingYawRotation(const Vec3& cardPosition)
    {
        const Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
        if (activeCamera == nullptr || activeCamera->Get_Transform() == nullptr)
            return Matrix::Identity;

        Vec3 toCamera = activeCamera->Get_Transform()->Get_WorldPosition() - cardPosition;
        toCamera.y = 0.f;

        if (toCamera.LengthSquared() <= 0.0001f)
            return Matrix::Identity;

        toCamera.Normalize();
        return Matrix::CreateRotationY(atan2f(-toCamera.x, -toCamera.z));
    }

    Vec3 Resolve_CameraScreenPositionDelta(const Vec3& screenPosition)
    {
        const Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
        if (activeCamera == nullptr || activeCamera->Get_Transform() == nullptr)
            return Vec3(screenPosition.x, -screenPosition.y, screenPosition.z);

        Shared<TransformCom> cameraTransform = activeCamera->Get_Transform();
        return
            cameraTransform->Get_WorldRight() * screenPosition.x -
            cameraTransform->Get_WorldUp() * screenPosition.y +
            cameraTransform->Get_WorldForward() * screenPosition.z;
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
        const float defaultGrayAmount = 0.f;
        const uint32 disabled = 0u;
        const uint32 fillOrigin = 0u;

        CHECK_FAILED(shader->Bind_Matrix("g_WorldMatrix", &worldMatrix), E_FAIL);
        CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::View, shader, "g_ViewMatrix"), E_FAIL);
        CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, shader, "g_ProjMatrix"), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_Color", &color, sizeof(Vec4)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_PatternUvOffset", &defaultPatternOffset, sizeof(Vec2)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_PatternUvScale", &defaultPatternScale, sizeof(Vec2)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_UseGrayScale", &disabled, sizeof(uint32)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_GrayAmount", &defaultGrayAmount, sizeof(float)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_IsOverlay", &disabled, sizeof(uint32)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_HoleCenterUV", &defaultHoleCenter, sizeof(Vec2)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_HoleRadius", &defaultHoleRadius, sizeof(float)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_HoleFeather", &defaultHoleFeather, sizeof(float)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_ViewportAspect", &defaultViewportAspect, sizeof(float)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_UseFillRatio", &disabled, sizeof(uint32)), E_FAIL);
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

    class IWorldUIGradientSurface
    {
    public:
        virtual ~IWorldUIGradientSurface() = default;

        virtual HRESULT Compose(const BattleWorldGradientPanelTemplate& panelTemplate) = 0;
        virtual HRESULT Bind_ShaderResource(ShaderCom* shader, const char* name) const = 0;
    };

    class RenderTargetComposedBattleWorldGradientSurface final : public IWorldUIGradientSurface
    {
    public:
        RenderTargetComposedBattleWorldGradientSurface(
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

        HRESULT Initialize(const wchar_t* surfaceName)
        {
            _renderTarget = RenderTarget::Create(
                surfaceName,
                _device,
                _context,
                static_cast<uint32>(max(1.f, roundf(_pixelSize.x * kBattleWorldGradientCompositionRenderScale))),
                static_cast<uint32>(max(1.f, roundf(_pixelSize.y * kBattleWorldGradientCompositionRenderScale))),
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

        HRESULT Compose(const BattleWorldGradientPanelTemplate& panelTemplate) override
        {
            ENGINE_PROFILE_SCOPE_BASIC("BattleWorldUI.Gradient.Compose");

            CHECK_NULL(_renderTarget, E_FAIL);

            ScopedRenderTargetState restoreState{ _context };

            ID3D11ShaderResourceView* nullSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
            _context->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);
            _context->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);

            CHECK_FAILED(_renderTarget->Begin(true, false), E_FAIL);

            for (const auto& layer : panelTemplate.layers)
            {
                if (layer.kind == BattleWorldGradientLayerKind::Image)
                    CHECK_FAILED(Draw_CompositionImage(layer.image), E_FAIL);
            }

            for (const auto& layer : panelTemplate.layers)
            {
                if (layer.kind == BattleWorldGradientLayerKind::Text)
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

        static Unique<IWorldUIGradientSurface> Create(
            const wchar_t* surfaceName,
            const ComPtr<Device>& device,
            const ComPtr<Context>& context,
            const Shared<ShaderCom>& shader,
            const Shared<VIBuffer_Rect>& viBuffer,
            const Vec2& pixelSize)
        {
            auto surface = make_unique<RenderTargetComposedBattleWorldGradientSurface>(
                device,
                context,
                shader,
                viBuffer,
                pixelSize);
            if (FAILED(surface->Initialize(surfaceName)))
                return nullptr;

            return surface;
        }

    private:
        HRESULT Draw_CompositionImage(const BattleWorldGradientImageDef& image)
        {
            CHECK_NULL(image.texture, E_FAIL);
            CHECK_NULL(_shader, E_FAIL);
            CHECK_NULL(_viBuffer, E_FAIL);

            const auto& srvs = image.texture->Get_SRVs();
            if (srvs.empty() || srvs[0] == nullptr)
                return E_FAIL;

            uint32 passIndex = 0u;
            if (image.imageRenderMode == BattleWorldGradientImageRenderMode::PatternOverlay)
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
                passIndex = (image.maskTextureMode == BattleWorldGradientMaskTextureMode::BlackWhite) ? 5u : 1u;
            }

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
            const uint32 disabled = 0u;
            const uint32 fillOrigin = 0u;
            const Vec2 patternUvOffset =
                image.imageRenderMode == BattleWorldGradientImageRenderMode::PatternOverlay ? image.patternUvOffset : defaultPatternOffset;
            const Vec2 patternUvScale =
                image.imageRenderMode == BattleWorldGradientImageRenderMode::PatternOverlay ? image.patternUvScale : defaultPatternScale;

            CHECK_FAILED(_shader->Bind_Matrix("g_WorldMatrix", &worldMatrix), E_FAIL);
            CHECK_FAILED(_shader->Bind_Matrix("g_ViewMatrix", &viewMatrix), E_FAIL);
            CHECK_FAILED(_shader->Bind_Matrix("g_ProjMatrix", &projMatrix), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_Color", &image.color, sizeof(Vec4)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_PatternUvOffset", &patternUvOffset, sizeof(Vec2)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_PatternUvScale", &patternUvScale, sizeof(Vec2)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_UseGrayScale", &disabled, sizeof(uint32)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_GrayAmount", &defaultGrayAmount, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_IsOverlay", &disabled, sizeof(uint32)), E_FAIL);
            CHECK_FAILED(_shader->Bind_SRV("g_Texture", srvs[0].Get()), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_HoleCenterUV", &defaultHoleCenter, sizeof(Vec2)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_HoleRadius", &defaultHoleRadius, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_HoleFeather", &defaultHoleFeather, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_ViewportAspect", &defaultViewportAspect, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_UseFillRatio", &disabled, sizeof(uint32)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_FillRatio", &defaultFillRatio, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_FillOrigin", &fillOrigin, sizeof(uint32)), E_FAIL);

            CHECK_FAILED(_shader->Begin(passIndex), E_FAIL);
            const float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
            _context->OMSetBlendState(_compositionBlendState.Get(), blendFactor, 0xffffffff);
            CHECK_FAILED(_viBuffer->Bind_Resources(), E_FAIL);
            CHECK_FAILED(_viBuffer->Render(), E_FAIL);

            return S_OK;
        }

        void Draw_CompositionText(const BattleWorldGradientTextDef& text)
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
            if (text.align == BattleWorldGradientTextAlign::Center)
                offsetX = (boxWidth - textWidth) * 0.5f;
            else if (text.align == BattleWorldGradientTextAlign::Right)
                offsetX = boxWidth - textWidth;

            const Vec2 alignedTopLeft(boxTopLeft.x + offsetX, boxTopLeft.y);
            const Vec2 pivot(
                (text.position.x - alignedTopLeft.x) / textScale,
                (text.position.y - alignedTopLeft.y) / textScale);

            GAME->Draw_Font(
                text.fontTag,
                text.text.c_str(),
                text.position * kBattleWorldGradientCompositionRenderScale,
                XMVectorSet(text.color.x, text.color.y, text.color.z, text.color.w),
                text.rotationRadians,
                pivot,
                textScale * kBattleWorldGradientCompositionRenderScale);
        }

        ComPtr<Device> _device{};
        ComPtr<Context> _context{};
        Shared<ShaderCom> _shader{};
        Shared<VIBuffer_Rect> _viBuffer{};
        Shared<RenderTarget> _renderTarget{};
        ComPtr<ID3D11BlendState> _compositionBlendState{};
        Vec2 _pixelSize = Vec2(
            static_cast<float>(kBattleWorldGradientFallbackPixelWidth),
            static_cast<float>(kBattleWorldGradientFallbackPixelHeight));
    };
}

struct BattleWorldUI_GradientCards::Impl
{
    Shared<ShaderCom> shader{};
    Shared<VIBuffer_Rect> viBuffer{};
    array<BattleWorldGradientPanelInstance, kBattleWorldGradientPlacementHeroCount> panels{};
    array<BattleWorldGradientWorldPlacementTuning, kBattleWorldGradientPlacementHeroCount> placementTunings =
        Make_DefaultGradientWorldPlacementTunings();
    fs::file_time_type placementTuningWriteTime{};
    bool hasPlacementTuningFile = false;
    HeroId activePlacementHeroId = HeroId::END;
    HeroId editorPlacementHeroId = HeroId::END;
    BattleTurnState introState = BattleTurnState::None;
    HeroId introHeroId = HeroId::END;
    bool hasIntroKey = false;
};

BattleWorldUI_GradientCards::BattleWorldUI_GradientCards(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

BattleWorldUI_GradientCards::~BattleWorldUI_GradientCards() = default;

HRESULT BattleWorldUI_GradientCards::Initialize(void* arg)
{
    CHECK_FAILED(GameObject::Initialize(arg), E_FAIL);

    _impl = make_unique<Impl>();
    CHECK_NULL(_impl, E_FAIL);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Shader_UI"), _impl->shader), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("VIBuffer_Rect"), _impl->viBuffer), E_FAIL);

    for (uint32 heroIndex = 0; heroIndex < kBattleWorldGradientPlacementHeroCount; ++heroIndex)
    {
        auto& panel = _impl->panels[heroIndex];
        const BattleWorldGradientHeroDef& heroDef = kBattleWorldGradientHeroDefs[heroIndex];
        Try_LoadJsonGradientPanelTemplate(
            _device,
            _context,
            heroDef,
            panel.panelTemplate);

        panel.surface = RenderTargetComposedBattleWorldGradientSurface::Create(
            heroDef.surfaceName,
            _device,
            _context,
            _impl->shader,
            _impl->viBuffer,
            panel.panelTemplate.pixelSize);
        CHECK_NULL(panel.surface, E_FAIL);
    }

    Reload_GradientWorldPlacementTuningIfNeeded(
        _impl->placementTunings,
        _impl->placementTuningWriteTime,
        _impl->hasPlacementTuningFile);

    Hide();
    return S_OK;
}

HRESULT BattleWorldUI_GradientCards::Warmup_ForBattleState(HeroId actorHeroId)
{
    ENGINE_PROFILE_SCOPE_BASIC("BattleWorldUI.Gradient.Warmup");

    if (_impl == nullptr)
        return E_FAIL;

    const HeroId placementHeroId = Normalize_PlacementHeroId(actorHeroId);
    const uint32 placementHeroIndex = Resolve_PlacementHeroIndex(placementHeroId);
    _impl->activePlacementHeroId = placementHeroId;
    if (_impl->editorPlacementHeroId == HeroId::END)
        _impl->editorPlacementHeroId = placementHeroId;

    BattleWorldGradientPanelInstance& panel = _impl->panels[placementHeroIndex];
    if (!panel.panelTemplate.valid || panel.surface == nullptr)
        return E_FAIL;

    if (!panel.dirty)
        return S_OK;

    CHECK_FAILED(panel.surface->Compose(panel.panelTemplate), E_FAIL);
    panel.dirty = false;
    return S_OK;
}

void BattleWorldUI_GradientCards::Show_ForBattleState(
    const Vec3& anchorWorldPos,
    BattleTurnState currentState,
    HeroId actorHeroId,
    float timeDelta)
{
    ENGINE_PROFILE_SCOPE_DETAIL("BattleWorldUI.Gradient.Show");

    if (_impl == nullptr || !Should_ShowBattleWorldGradientCards(currentState))
    {
        Hide();
        return;
    }

    const HeroId placementHeroId = Normalize_PlacementHeroId(actorHeroId);
    const uint32 placementHeroIndex = Resolve_PlacementHeroIndex(placementHeroId);
    _impl->activePlacementHeroId = placementHeroId;
    if (_impl->editorPlacementHeroId == HeroId::END)
        _impl->editorPlacementHeroId = placementHeroId;

    const bool restartIntro =
        !_impl->hasIntroKey ||
        _impl->introState != currentState ||
        _impl->introHeroId != placementHeroId;
    if (restartIntro)
    {
        _impl->introState = currentState;
        _impl->introHeroId = placementHeroId;
        _impl->hasIntroKey = true;
        Restart_BattleWorldSurfaceAnim(_impl->panels[placementHeroIndex].introAnim, 0);
    }

    for (auto& panel : _impl->panels)
    {
        panel.visible = false;
        Advance_BattleWorldSurfaceAnim(panel.introAnim, timeDelta);
    }

    BattleWorldGradientPanelInstance& panel = _impl->panels[placementHeroIndex];
    if (!panel.panelTemplate.valid || panel.surface == nullptr)
    {
        Set_Visible(false);
        return;
    }

    if (panel.dirty)
    {
        if (FAILED(panel.surface->Compose(panel.panelTemplate)))
        {
            Set_Visible(false);
            return;
        }

        panel.dirty = false;
    }

    const BattleWorldGradientWorldPlacementTuning& placementTuning =
        _impl->placementTunings[placementHeroIndex];
    const Vec3 position =
        anchorWorldPos +
        Resolve_CameraScreenPositionDelta(placementTuning.screenPosition) +
        BattleWorldUIPlacement::Resolve_CameraHorizontalPlacementDelta(placementTuning.baseOffset);
    panel.introStartOffset = Resolve_CameraScreenPositionDelta(Vec3(0.f, -0.08f, 0.f));
    const Vec2 aspectWorldSize = Resolve_AspectWorldSize(
        placementTuning.worldSize,
        panel.panelTemplate.pixelSize);

    panel.worldMatrix =
        Matrix::CreateScale(
            aspectWorldSize.x * placementTuning.visualScale,
            aspectWorldSize.y * placementTuning.visualScale,
            1.f) *
        Matrix::CreateRotationZ(placementTuning.rollRadians) *
        Matrix::CreateRotationX(placementTuning.pitchRadians) *
        Matrix::CreateRotationY(placementTuning.yawOffsetRadians) *
        Resolve_CameraFacingYawRotation(position) *
        Matrix::CreateTranslation(position.x, position.y, position.z);
    panel.visible = true;

    Set_Visible(true);
}

void BattleWorldUI_GradientCards::Hide()
{
    Set_Visible(false);

    if (_impl == nullptr)
        return;

    _impl->hasIntroKey = false;
    for (auto& panel : _impl->panels)
        panel.visible = false;
}

HeroId BattleWorldUI_GradientCards::Get_EditorWorldPlacementHeroId() const
{
    if (_impl == nullptr)
        return HeroId::Gustave;

    if (_impl->editorPlacementHeroId != HeroId::END)
        return Normalize_PlacementHeroId(_impl->editorPlacementHeroId);

    if (_impl->activePlacementHeroId != HeroId::END)
        return Normalize_PlacementHeroId(_impl->activePlacementHeroId);

    return HeroId::Gustave;
}

void BattleWorldUI_GradientCards::Set_EditorWorldPlacementHeroId(HeroId heroId)
{
    if (_impl == nullptr)
        return;

    _impl->editorPlacementHeroId = Normalize_PlacementHeroId(heroId);
}

RuntimeWorldUIPlacementTuning BattleWorldUI_GradientCards::Get_EditorWorldPlacementTuning() const
{
    if (_impl == nullptr)
        return To_EditorWorldPlacementTuning(Make_DefaultGradientWorldPlacementTuning(HeroId::Gustave));

    const uint32 heroIndex = Resolve_PlacementHeroIndex(Get_EditorWorldPlacementHeroId());
    return To_EditorWorldPlacementTuning(_impl->placementTunings[heroIndex]);
}

void BattleWorldUI_GradientCards::Set_EditorWorldPlacementTuning(
    const RuntimeWorldUIPlacementTuning& tuning,
    bool saveImmediately)
{
    if (_impl == nullptr)
        return;

    const uint32 heroIndex = Resolve_PlacementHeroIndex(Get_EditorWorldPlacementHeroId());
    _impl->placementTunings[heroIndex] = From_EditorWorldPlacementTuning(tuning);

    if (!saveImmediately)
        return;

    if (Write_GradientWorldPlacementTuningsToFile(
        _impl->placementTunings,
        &_impl->placementTuningWriteTime))
    {
        _impl->hasPlacementTuningFile = true;
    }
}

void BattleWorldUI_GradientCards::Reload_EditorWorldPlacementTuning()
{
    if (_impl == nullptr)
        return;

    const fs::path jsonPath = Resolve_GradientWorldPlacementJsonPath();
    if (!fs::exists(jsonPath))
    {
        _impl->placementTunings = Make_DefaultGradientWorldPlacementTunings();
        _impl->placementTuningWriteTime = {};
        _impl->hasPlacementTuningFile = false;
        LOG_INFO("BattleWorldUI gradient placement json reload requested, but file is missing. using defaults.");
        return;
    }

    array<BattleWorldGradientWorldPlacementTuning, kBattleWorldGradientPlacementHeroCount> loadedTunings{};
    if (!Try_LoadGradientWorldPlacementTunings(jsonPath, loadedTunings))
        return;

    error_code errorCode{};
    const fs::file_time_type writeTime = fs::last_write_time(jsonPath, errorCode);
    if (!errorCode)
        _impl->placementTuningWriteTime = writeTime;

    _impl->placementTunings = loadedTunings;
    _impl->hasPlacementTuningFile = true;
}

void BattleWorldUI_GradientCards::Reset_EditorWorldPlacementTuning()
{
    if (_impl == nullptr)
        return;

    const HeroId heroId = Get_EditorWorldPlacementHeroId();
    _impl->placementTunings[Resolve_PlacementHeroIndex(heroId)] =
        Make_DefaultGradientWorldPlacementTuning(heroId);
    if (Write_GradientWorldPlacementTuningsToFile(
        _impl->placementTunings,
        &_impl->placementTuningWriteTime))
    {
        _impl->hasPlacementTuningFile = true;
    }
}

void BattleWorldUI_GradientCards::Late_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!Is_Visible())
        return;

    GAME->Add_RenderGroup(RenderGroup::UIEffect, GetSharedPtr<GameObject>());
}

HRESULT BattleWorldUI_GradientCards::Render()
{
    if (!Is_Visible() || _impl == nullptr || _impl->shader == nullptr || _impl->viBuffer == nullptr)
        return S_OK;

    const Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);

    for (const auto& panel : _impl->panels)
    {
        if (!panel.visible || panel.surface == nullptr)
            continue;

        const BattleWorldSurfaceAnimRenderState renderState =
            Apply_BattleWorldSurfaceAnim(panel.introAnim, panel.worldMatrix, color, panel.introStartOffset);
        CHECK_FAILED(Bind_WorldUIPlaneShaderState(_impl->shader.get(), renderState.worldMatrix, renderState.color), E_FAIL);
        CHECK_FAILED(panel.surface->Bind_ShaderResource(_impl->shader.get(), "g_Texture"), E_FAIL);
        CHECK_FAILED(_impl->shader->Begin(0), E_FAIL);
        CHECK_FAILED(_impl->viBuffer->Bind_Resources(), E_FAIL);
        CHECK_FAILED(_impl->viBuffer->Render(), E_FAIL);
    }

    return S_OK;
}

Shared<GameObject> BattleWorldUI_GradientCards::Clone(void* arg)
{
    UNREFERENCED_PARAMETER(arg);
    return nullptr;
}

void BattleWorldUI_GradientCards::Free()
{
    __super::Free();

    _impl.reset();
}

Shared<BattleWorldUI_GradientCards> BattleWorldUI_GradientCards::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context)
{
    auto instance = make_shared<BattleWorldUI_GradientCards>(device, context);
    if (FAILED(instance->Initialize(nullptr)))
    {
        MSG_BOX("Failed to create BattleWorldUI_GradientCards");
        return nullptr;
    }

    return instance;
}

NS_END
