#include "BattleWorldUI_SkillCards.h"

#include "BattleWorldUI_JsonTextureResolver.h"
#include "ClientInstance.h"
#include "CombatStatCom.h"
#include "RenderTarget.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "VIBuffer_Rect.h"

NS_BEGIN(Client)

namespace
{
    constexpr char kBattleWorldSkillCardTextureGuid[] = "bec2ba8a-a265-40d8-9bd1-bc85420ebaa2";
    constexpr char kBattleWorldKeyFrameTextureGuid[] = "72de5c45-b20f-490d-9d69-396a830b514f";
    constexpr wchar_t kBattleWorldSkillPanelJsonPath[] = L"UI/UI_Json/BattleSystem/Gustave/UI_BattleSystem_Skill_Gustave.json";
    constexpr uint32 kBattleWorldSkillFallbackPixelWidth = 768;
    constexpr uint32 kBattleWorldSkillFallbackPixelHeight = 192;
    constexpr uint32 kBattleWorldSkillSlotCount = 3;
    constexpr float kBattleWorldSkillCompositionRenderScale = 2.f;
    const Vec2 kBattleWorldSkillListWorldSize = Vec2(1.05f, 0.262f);
    const Vec2 kBattleWorldSkillTargetingWorldSize = Vec2(1.12f, 0.28f);
    const Vec3 kBattleWorldSkillTargetingOffset = Vec3(0.52f, 1.00f, 0.f);
    const Vec3 kBattleWorldSkillListOffsets[kBattleWorldSkillSlotCount] =
    {
        Vec3(0.50f, 1.04f, 0.f),
        Vec3(0.50f, 0.76f, 0.f),
        Vec3(0.50f, 0.48f, 0.f),
    };
    constexpr float kBattleWorldSkillDefaultScale = 1.f;
    constexpr float kBattleWorldSkillSelectedScale = 1.05f;

    enum class BattleWorldSkillCardTextSlot
    {
        Literal,
        KeyPrompt,
        Title,
        Description,
        ApCost,
        ApCostShadow,
    };

    enum class BattleWorldSkillCardLayerKind
    {
        Image,
        Text,
    };

    enum class BattleWorldSkillTextAlign
    {
        Left,
        Center,
        Right,
    };

    enum class BattleWorldSkillImageRenderMode
    {
        Normal,
        PatternOverlay,
        Unknown,
    };

    enum class BattleWorldSkillMaskTextureMode
    {
        Alpha,
        BlackWhite,
        Unknown,
    };

    struct BattleWorldSkillCardTextDef
    {
        BattleWorldSkillCardTextSlot slot = BattleWorldSkillCardTextSlot::Literal;
        wstring text;
        wstring fontTag;
        Vec2 size{};
        Vec2 pivot = Vec2(0.f, 0.f);
        Vec2 position{};
        float scale = 1.f;
        float rotationRadians = 0.f;
        Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);
        BattleWorldSkillTextAlign align = BattleWorldSkillTextAlign::Left;
        bool textWrap = false;
        bool useRuntimeText = false;
    };

    struct BattleWorldSkillCardImageDef
    {
        Shared<Texture> texture{};
        Vec2 center{};
        Vec2 size{};
        Vec2 pivot = Vec2(0.5f, 0.5f);
        float rotationRadians = 0.f;
        Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);
        BattleWorldSkillImageRenderMode imageRenderMode = BattleWorldSkillImageRenderMode::Normal;
        Shared<Texture> maskTexture{};
        BattleWorldSkillMaskTextureMode maskTextureMode = BattleWorldSkillMaskTextureMode::Alpha;
        Shared<Texture> patternMaskTexture{};
        Vec2 patternUvOffset = Vec2(0.f, 0.f);
        Vec2 patternUvScale = Vec2(1.f, 1.f);
    };

    struct BattleWorldSkillCardContent
    {
        string skillKey;
        wstring keyPrompt;
        wstring title;
        wstring description;
        wstring subDescription;
        int32 apCost = 0;
        int32 hitCount = 0;
        ElementalType element = ElementalType::None;
    };

    struct BattleWorldSkillCardDef
    {
        BattleWorldSkillCardContent content{};
        Vec2 pixelSize = Vec2(
            static_cast<float>(kBattleWorldSkillFallbackPixelWidth),
            static_cast<float>(kBattleWorldSkillFallbackPixelHeight));

        struct Layer
        {
            BattleWorldSkillCardLayerKind kind = BattleWorldSkillCardLayerKind::Image;
            int32 sortOrder = 0;
            uint32 sequence = 0;
            BattleWorldSkillCardImageDef image{};
            BattleWorldSkillCardTextDef text{};
        };

        vector<Layer> layers{};
    };

    struct BattleWorldSkillCardTemplate
    {
        Vec2 pixelSize = Vec2(
            static_cast<float>(kBattleWorldSkillFallbackPixelWidth),
            static_cast<float>(kBattleWorldSkillFallbackPixelHeight));
        vector<BattleWorldSkillCardDef::Layer> layers{};
    };

    class IWorldUISkillCardSurface;

    struct BattleWorldSkillCardInstance
    {
        BattleWorldSkillCardDef def{};
        Unique<IWorldUISkillCardSurface> surface{};
        Matrix worldMatrix = Matrix::Identity;
        bool hasContent = false;
        bool visible = false;
        bool selected = false;
        bool dimmed = false;
        bool dirty = true;
    };

    struct BattleWorldSkillCardPresentation
    {
        bool visible = false;
        bool selected = false;
        bool dimmed = false;
        Vec3 worldOffset = Vec3::Zero;
        Vec2 worldSize = kBattleWorldSkillListWorldSize;
        float visualScale = kBattleWorldSkillDefaultScale;
    };

    bool Should_ShowBattleWorldSkillCards(BattleTurnState state)
    {
        return state == BattleTurnState::ChooseSkill ||
            state == BattleTurnState::SkillTargeting;
    }

    bool Is_TargetingSkillCardState(BattleTurnState state)
    {
        return state == BattleTurnState::SkillTargeting;
    }

    const wchar_t* Resolve_KeyPrompt(uint32 slotIndex)
    {
        switch (slotIndex)
        {
        case 0:
            return L"Q";
        case 1:
            return L"W";
        case 2:
            return L"E";
        default:
            return L"";
        }
    }

    wstring Resolve_ElementText(ElementalType element)
    {
        switch (element)
        {
        case ElementalType::Fire:
            return L"Fire";
        case ElementalType::Ice:
            return L"Ice";
        case ElementalType::Lightning:
            return L"Lightning";
        case ElementalType::Earth:
            return L"Earth";
        case ElementalType::Dark:
            return L"Dark";
        case ElementalType::Light:
            return L"Light";
        case ElementalType::None:
        default:
            return L"None";
        }
    }

    wstring Resolve_SkillTitle(const Skill_Data& skill)
    {
        if (!skill.skillKey.empty())
            return String::ToWString(skill.skillKey);

        return L"Skill";
    }

    wstring Resolve_SkillDescription(const Skill_Data& skill)
    {
        wstring description = L"AP ";
        description += to_wstring(max(skill.apCost, 0));
        description += L" / Hits ";
        description += to_wstring(max(skill.AttackCount, 0));
        return description;
    }

    wstring Resolve_SkillSubDescription(const Skill_Data& skill)
    {
        return L"Element " + Resolve_ElementText(skill.element);
    }

    BattleWorldSkillCardContent Build_SkillCardContent(
        uint32 slotIndex,
        const string& skillKey,
        const Skill_Data& skill)
    {
        BattleWorldSkillCardContent content{};
        content.skillKey = skillKey;
        content.keyPrompt = Resolve_KeyPrompt(slotIndex);
        content.title = Resolve_SkillTitle(skill);
        content.description = Resolve_SkillDescription(skill);
        content.subDescription = Resolve_SkillSubDescription(skill);
        content.apCost = skill.apCost;
        content.hitCount = skill.AttackCount;
        content.element = skill.element;
        return content;
    }

    bool Is_SameContent(const BattleWorldSkillCardContent& lhs, const BattleWorldSkillCardContent& rhs)
    {
        return lhs.skillKey == rhs.skillKey &&
            lhs.keyPrompt == rhs.keyPrompt &&
            lhs.title == rhs.title &&
            lhs.description == rhs.description &&
            lhs.subDescription == rhs.subDescription &&
            lhs.apCost == rhs.apCost &&
            lhs.hitCount == rhs.hitCount &&
            lhs.element == rhs.element;
    }

    struct BattleWorldSkillJsonObject
    {
        int32 id = 0;
        int32 parentId = 0;
        int32 sortOrder = 0;
        uint32 sequence = 0;
        string name{};
        string type{};
        string textureGuid{};
        string texturePath{};
        string maskTextureGuid{};
        string maskTexturePath{};
        string maskTextureMode{};
        string imageRenderMode{};
        string patternMaskTextureGuid{};
        string patternMaskTexturePath{};
        string fontTag{};
        string fontPath{};
        string text{};
        string textAlign{};
        Vec2 position = Vec2(0.f, 0.f);
        Vec2 size = Vec2(1.f, 1.f);
        Vec2 pivot = Vec2(0.5f, 0.5f);
        Vec2 scale = Vec2(1.f, 1.f);
        float rotationRadians = 0.f;
        float alphaMultiplier = 1.f;
        float fontSize = 32.f;
        float fillRatio = 1.f;
        Vec2 patternUvOffset = Vec2(0.f, 0.f);
        Vec2 patternUvScale = Vec2(1.f, 1.f);
        Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);
        bool visible = true;
        bool textWrap = false;
        bool useFillRatio = false;
    };

    struct BattleWorldSkillJsonPanelSpec
    {
        const char* panelName = "";
        const char* textPrefix = "";
    };

    using BattleWorldSkillJsonObjectMap = unordered_map<int32, BattleWorldSkillJsonObject>;

    const BattleWorldSkillJsonPanelSpec kBattleWorldSkillJsonPanelSpecs[kBattleWorldSkillSlotCount] =
    {
        { "Panel_LumiereAssault", "LumiereAssault" },
        { "Panel_UnleashedCharge", "UnleashedCharge" },
        { "Panel_MarkingShot", "MarkingShot" },
    };

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

    BattleWorldSkillImageRenderMode Parse_JsonImageRenderMode(const string& modeName)
    {
        if (modeName.empty())
            return BattleWorldSkillImageRenderMode::Normal;

        const string mode = String::ToLowerCopy(modeName);
        if ("normal" == mode)
            return BattleWorldSkillImageRenderMode::Normal;
        if ("patternoverlay" == mode || "pattern_overlay" == mode || "pattern overlay" == mode)
            return BattleWorldSkillImageRenderMode::PatternOverlay;

        return BattleWorldSkillImageRenderMode::Unknown;
    }

    BattleWorldSkillMaskTextureMode Parse_JsonMaskTextureMode(const string& modeName)
    {
        if (modeName.empty())
            return BattleWorldSkillMaskTextureMode::Alpha;

        const string mode = String::ToLowerCopy(modeName);
        if ("alpha" == mode)
            return BattleWorldSkillMaskTextureMode::Alpha;
        if ("blackwhite" == mode || "black_white" == mode || "black white" == mode)
            return BattleWorldSkillMaskTextureMode::BlackWhite;

        return BattleWorldSkillMaskTextureMode::Unknown;
    }

    const char* To_LogName(BattleWorldSkillImageRenderMode mode)
    {
        switch (mode)
        {
        case BattleWorldSkillImageRenderMode::Normal:
            return "Normal";
        case BattleWorldSkillImageRenderMode::PatternOverlay:
            return "PatternOverlay";
        case BattleWorldSkillImageRenderMode::Unknown:
        default:
            return "Unknown";
        }
    }

    const char* To_LogName(BattleWorldSkillMaskTextureMode mode)
    {
        switch (mode)
        {
        case BattleWorldSkillMaskTextureMode::Alpha:
            return "Alpha";
        case BattleWorldSkillMaskTextureMode::BlackWhite:
            return "BlackWhite";
        case BattleWorldSkillMaskTextureMode::Unknown:
        default:
            return "Unknown";
        }
    }

    bool Has_TextureRef(const string& textureGuid, const string& texturePath)
    {
        return !textureGuid.empty() || !texturePath.empty();
    }

    BattleWorldSkillJsonObject Parse_JsonObject(const json& node, uint32 sequence)
    {
        BattleWorldSkillJsonObject object{};
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
        const BattleWorldSkillJsonObject& parent)
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

    const BattleWorldSkillJsonObject* Find_JsonObject(
        const BattleWorldSkillJsonObjectMap& objectsById,
        int32 objectId)
    {
        const auto iter = objectsById.find(objectId);
        if (iter == objectsById.end())
            return nullptr;

        return &iter->second;
    }

    bool Is_DescendantOf(
        const BattleWorldSkillJsonObject& object,
        int32 panelId,
        const BattleWorldSkillJsonObjectMap& objectsById)
    {
        int32 parentId = object.parentId;
        for (uint32 depth = 0; depth < 256 && parentId != 0; ++depth)
        {
            if (parentId == panelId)
                return true;

            const BattleWorldSkillJsonObject* parent = Find_JsonObject(objectsById, parentId);
            if (parent == nullptr)
                break;

            parentId = parent->parentId;
        }

        return false;
    }

    bool Is_VisibleInPanelHierarchy(
        const BattleWorldSkillJsonObject& object,
        int32 panelId,
        const BattleWorldSkillJsonObjectMap& objectsById)
    {
        if (!object.visible)
            return false;

        int32 parentId = object.parentId;
        for (uint32 depth = 0; depth < 256 && parentId != 0; ++depth)
        {
            if (parentId == panelId)
                return true;

            const BattleWorldSkillJsonObject* parent = Find_JsonObject(objectsById, parentId);
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
        const BattleWorldSkillJsonObjectMap& objectsById)
    {
        for (uint32 depth = 0; depth < 256 && parentId != 0 && parentId != panelId; ++depth)
        {
            const BattleWorldSkillJsonObject* parent = Find_JsonObject(objectsById, parentId);
            if (parent == nullptr)
                break;

            point = Transform_PointByJsonParent(point, *parent);
            parentId = parent->parentId;
        }

        return point;
    }

    Vec2 Resolve_JsonWorldScale(
        const BattleWorldSkillJsonObject& object,
        int32 panelId,
        const BattleWorldSkillJsonObjectMap& objectsById)
    {
        Vec2 scale = object.scale;
        int32 parentId = object.parentId;

        for (uint32 depth = 0; depth < 256 && parentId != 0 && parentId != panelId; ++depth)
        {
            const BattleWorldSkillJsonObject* parent = Find_JsonObject(objectsById, parentId);
            if (parent == nullptr)
                break;

            scale.x *= parent->scale.x;
            scale.y *= parent->scale.y;
            parentId = parent->parentId;
        }

        return scale;
    }

    float Resolve_JsonWorldRotation(
        const BattleWorldSkillJsonObject& object,
        int32 panelId,
        const BattleWorldSkillJsonObjectMap& objectsById)
    {
        float rotation = object.rotationRadians;
        int32 parentId = object.parentId;

        for (uint32 depth = 0; depth < 256 && parentId != 0 && parentId != panelId; ++depth)
        {
            const BattleWorldSkillJsonObject* parent = Find_JsonObject(objectsById, parentId);
            if (parent == nullptr)
                break;

            rotation += parent->rotationRadians;
            parentId = parent->parentId;
        }

        return rotation;
    }

    float Resolve_JsonAlphaMultiplier(
        const BattleWorldSkillJsonObject& object,
        int32 panelId,
        const BattleWorldSkillJsonObjectMap& objectsById)
    {
        float alphaMultiplier = object.alphaMultiplier;
        int32 parentId = object.parentId;

        for (uint32 depth = 0; depth < 256 && parentId != 0 && parentId != panelId; ++depth)
        {
            const BattleWorldSkillJsonObject* parent = Find_JsonObject(objectsById, parentId);
            if (parent == nullptr)
                break;

            alphaMultiplier *= parent->alphaMultiplier;
            parentId = parent->parentId;
        }

        return alphaMultiplier;
    }

    Vec2 Resolve_JsonImageCenter(const BattleWorldSkillJsonObject& object)
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

    BattleWorldSkillTextAlign Resolve_TextAlign(const string& textAlign)
    {
        const string normalizedTextAlign = String::ToLowerCopy(textAlign);
        if (normalizedTextAlign == "center")
            return BattleWorldSkillTextAlign::Center;
        if (normalizedTextAlign == "right")
            return BattleWorldSkillTextAlign::Right;

        return BattleWorldSkillTextAlign::Left;
    }

    BattleWorldSkillCardTextSlot Resolve_TextSlot(const string& objectName, const string& textPrefix)
    {
        if (objectName.rfind("Text_Key_", 0) == 0)
            return BattleWorldSkillCardTextSlot::KeyPrompt;

        const string titleName = "Text_" + textPrefix;
        if (objectName == titleName)
            return BattleWorldSkillCardTextSlot::Title;

        if (objectName == titleName + "_Description_1")
            return BattleWorldSkillCardTextSlot::Description;

        if (objectName == titleName + "_AP")
            return BattleWorldSkillCardTextSlot::ApCost;

        if (objectName == titleName + "_AP_BG")
            return BattleWorldSkillCardTextSlot::ApCostShadow;

        return BattleWorldSkillCardTextSlot::Literal;
    }

    wstring Resolve_TextForSlot(
        BattleWorldSkillCardTextSlot slot,
        const BattleWorldSkillCardContent& content,
        const wstring& literalText)
    {
        switch (slot)
        {
        case BattleWorldSkillCardTextSlot::KeyPrompt:
            return content.keyPrompt;
        case BattleWorldSkillCardTextSlot::Title:
            return content.title;
        case BattleWorldSkillCardTextSlot::Description:
            if (!content.subDescription.empty())
                return content.description + L"\n" + content.subDescription;

            return content.description;
        case BattleWorldSkillCardTextSlot::ApCost:
        case BattleWorldSkillCardTextSlot::ApCostShadow:
            return to_wstring(max(content.apCost, 0));
        case BattleWorldSkillCardTextSlot::Literal:
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

    Vec4 Resolve_ImageColor(const Vec4& color, bool selected, bool dimmed)
    {
        if (selected)
        {
            return Vec4(
                min(color.x * 1.18f + 0.08f, 1.f),
                min(color.y * 1.18f + 0.08f, 1.f),
                min(color.z * 1.18f + 0.08f, 1.f),
                color.w);
        }

        if (dimmed)
            return Vec4(color.x * 0.55f, color.y * 0.55f, color.z * 0.55f, color.w * 0.58f);

        return color;
    }

    Vec4 Resolve_TextColor(const Vec4& color, bool selected, bool dimmed)
    {
        if (selected)
        {
            return Vec4(
                min(color.x * 1.14f + 0.06f, 1.f),
                min(color.y * 1.14f + 0.06f, 1.f),
                min(color.z * 1.14f + 0.06f, 1.f),
                color.w);
        }

        if (dimmed)
            return Vec4(color.x * 0.68f, color.y * 0.68f, color.z * 0.68f, color.w * 0.62f);

        return color;
    }

    BattleWorldSkillCardTemplate Build_FallbackSkillCardTemplate(
        const Shared<Texture>& cardTexture,
        const Shared<Texture>& keyFrameTexture)
    {
        BattleWorldSkillCardTemplate cardTemplate{};
        cardTemplate.pixelSize = Vec2(
            static_cast<float>(kBattleWorldSkillFallbackPixelWidth),
            static_cast<float>(kBattleWorldSkillFallbackPixelHeight));

        cardTemplate.layers.push_back(
            {
                BattleWorldSkillCardLayerKind::Image,
                0,
                0,
                {
                    cardTexture,
                    Vec2(388.f, 96.f),
                    Vec2(690.f, 156.f),
                    Vec2(0.5f, 0.5f),
                    0.f,
                    Vec4(74.f / 255.f, 104.f / 255.f, 170.f / 255.f, 1.f)
                }
            });

        cardTemplate.layers.push_back(
            {
                BattleWorldSkillCardLayerKind::Image,
                1,
                1,
                {
                    keyFrameTexture,
                    Vec2(102.f, 96.f),
                    Vec2(62.f, 62.f),
                    Vec2(0.5f, 0.5f),
                    0.f,
                    Vec4(1.f, 1.f, 1.f, 0.95f)
                }
            });

        cardTemplate.layers.push_back(
            {
                BattleWorldSkillCardLayerKind::Text,
                2,
                2,
                {},
                {
                    BattleWorldSkillCardTextSlot::KeyPrompt,
                    {},
                    L"33_SubKR_Font",
                    Vec2(62.f, 62.f),
                    Vec2(0.5f, 0.5f),
                    Vec2(102.f, 95.f),
                    1.12f,
                    0.f,
                    Make_PremultipliedSpriteFontColor(Vec4(236.f / 255.f, 207.f / 255.f, 165.f / 255.f, 1.f)),
                    BattleWorldSkillTextAlign::Center,
                    false,
                    true
                }
            });

        cardTemplate.layers.push_back(
            {
                BattleWorldSkillCardLayerKind::Text,
                3,
                3,
                {},
                {
                    BattleWorldSkillCardTextSlot::Title,
                    {},
                    L"33_MainKR_Font",
                    Vec2(420.f, 44.f),
                    Vec2(0.f, 0.f),
                    Vec2(158.f, 46.f),
                    1.20f,
                    0.f,
                    Make_PremultipliedSpriteFontColor(Vec4(1.f, 0.92f, 0.78f, 1.f)),
                    BattleWorldSkillTextAlign::Left,
                    false,
                    true
                }
            });

        cardTemplate.layers.push_back(
            {
                BattleWorldSkillCardLayerKind::Text,
                4,
                4,
                {},
                {
                    BattleWorldSkillCardTextSlot::Description,
                    {},
                    L"33_SubKR_Font",
                    Vec2(470.f, 70.f),
                    Vec2(0.f, 0.f),
                    Vec2(160.f, 94.f),
                    0.78f,
                    0.f,
                    Make_PremultipliedSpriteFontColor(Vec4(0.90f, 0.88f, 1.f, 0.96f)),
                    BattleWorldSkillTextAlign::Left,
                    false,
                    true
                }
            });

        return cardTemplate;
    }

    BattleWorldSkillCardDef Build_SkillCardDef(
        const BattleWorldSkillCardContent& content,
        const BattleWorldSkillCardTemplate& cardTemplate)
    {
        BattleWorldSkillCardDef def{};
        def.content = content;
        def.pixelSize = cardTemplate.pixelSize;
        def.layers = cardTemplate.layers;

        for (auto& layer : def.layers)
        {
            if (layer.kind != BattleWorldSkillCardLayerKind::Text)
                continue;

            if (layer.text.useRuntimeText)
                layer.text.text = Resolve_TextForSlot(layer.text.slot, content, layer.text.text);
        }

        return def;
    }

    bool Try_ExtractJsonImageLayer(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const BattleWorldSkillJsonObject& object,
        const char* panelName,
        int32 panelId,
        const BattleWorldSkillJsonObjectMap& objectsById,
        BattleWorldSkillCardDef::Layer& outLayer)
    {
        const BattleWorldSkillImageRenderMode imageRenderMode =
            Parse_JsonImageRenderMode(object.imageRenderMode);
        const BattleWorldSkillMaskTextureMode maskTextureMode =
            Parse_JsonMaskTextureMode(object.maskTextureMode);

        if (imageRenderMode == BattleWorldSkillImageRenderMode::Unknown)
        {
            LOG_WARN(
                "BattleWorldUI skill json image skipped: unknown imageRenderMode. panel='{}', name='{}', mode='{}'",
                panelName,
                object.name,
                object.imageRenderMode);
            return false;
        }

        if (object.useFillRatio)
        {
            LOG_DEBUG(
                "BattleWorldUI skill json image skipped: unsupported fillRatio. panel='{}', name='{}', fillRatio={}",
                panelName,
                object.name,
                object.fillRatio);
            return false;
        }

        if (imageRenderMode != BattleWorldSkillImageRenderMode::PatternOverlay &&
            Has_TextureRef(object.maskTextureGuid, object.maskTexturePath) &&
            maskTextureMode == BattleWorldSkillMaskTextureMode::Unknown)
        {
            LOG_WARN(
                "BattleWorldUI skill json image skipped: unknown maskTextureMode. panel='{}', name='{}', mode='{}'",
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
        const bool requiresMaskTexture = hasMaskRef || imageRenderMode == BattleWorldSkillImageRenderMode::PatternOverlay;
        if (requiresMaskTexture)
        {
            if (!hasMaskRef)
            {
                LOG_WARN(
                    "BattleWorldUI skill json image skipped: missing required mask ref. panel='{}', name='{}', mode='{}'",
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
                    "BattleWorldUI skill json image skipped: missing required mask. panel='{}', name='{}', maskPath='{}'",
                    panelName,
                    object.name,
                    object.maskTexturePath);
                return false;
            }
        }

        Shared<Texture> patternMaskTexture{};
        const bool hasPatternMaskRef = Has_TextureRef(object.patternMaskTextureGuid, object.patternMaskTexturePath);
        if (imageRenderMode == BattleWorldSkillImageRenderMode::PatternOverlay)
        {
            if (!hasPatternMaskRef)
            {
                LOG_WARN(
                    "BattleWorldUI skill json image skipped: missing required pattern mask ref. panel='{}', name='{}'",
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
                    "BattleWorldUI skill json image skipped: missing required pattern mask. panel='{}', name='{}', patternMaskPath='{}'",
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

        outLayer.kind = BattleWorldSkillCardLayerKind::Image;
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
            "BattleWorldUI skill json image supported. panel='{}', name='{}', sort={}, mode='{}', maskMode='{}', hasMask={}, hasPatternMask={}",
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
        const BattleWorldSkillJsonObject& object,
        const BattleWorldSkillJsonPanelSpec& panelSpec,
        int32 panelId,
        const BattleWorldSkillJsonObjectMap& objectsById,
        BattleWorldSkillCardDef::Layer& outLayer)
    {
        UNREFERENCED_PARAMETER(panelSpec);

        if (object.text.empty())
            return false;

        if (!Ensure_JsonFontLoaded(object.fontTag, object.fontPath))
            return false;

        const Vec2 worldScale = Resolve_JsonWorldScale(object, panelId, objectsById);
        const Vec2 panelLocalPosition = Transform_PointToPanelLocal(object.position, object.parentId, panelId, objectsById);

        Vec4 color = object.color;
        color.w *= Resolve_JsonAlphaMultiplier(object, panelId, objectsById);

        outLayer.kind = BattleWorldSkillCardLayerKind::Text;
        outLayer.sortOrder = object.sortOrder;
        outLayer.sequence = object.sequence;
        outLayer.text.slot = BattleWorldSkillCardTextSlot::Literal;
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
        outLayer.text.useRuntimeText = false;
        return true;
    }

    bool Try_LoadJsonSkillCardTemplates(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        array<BattleWorldSkillCardTemplate, kBattleWorldSkillSlotCount>& outTemplates)
    {
        const fs::path jsonPath = (fs::path(GAME->Get_AssetRoot()) / kBattleWorldSkillPanelJsonPath).lexically_normal();
        if (!fs::exists(jsonPath))
        {
            LOG_WARN("BattleWorldUI skill json template missing. path='{}'", String::ToString(jsonPath.wstring()));
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
            LOG_WARN("BattleWorldUI skill json template parse failed. path='{}', error='{}'",
                String::ToString(jsonPath.wstring()),
                exception.what());
            return false;
        }

        const auto objectsIter = root.find("objects");
        if (objectsIter == root.end() || !objectsIter->is_array())
            return false;

        vector<BattleWorldSkillJsonObject> objects{};
        BattleWorldSkillJsonObjectMap objectsById{};
        objects.reserve(objectsIter->size());

        uint32 sequence = 0;
        for (const auto& node : *objectsIter)
        {
            if (!node.is_object())
                continue;

            BattleWorldSkillJsonObject object = Parse_JsonObject(node, sequence++);
            objectsById.emplace(object.id, object);
            objects.push_back(std::move(object));
        }

        for (uint32 slotIndex = 0; slotIndex < kBattleWorldSkillSlotCount; ++slotIndex)
        {
            const BattleWorldSkillJsonPanelSpec& panelSpec = kBattleWorldSkillJsonPanelSpecs[slotIndex];
            const auto panelIter = find_if(objects.begin(), objects.end(),
                [&](const BattleWorldSkillJsonObject& object)
                {
                    return object.name == panelSpec.panelName;
                });

            if (panelIter == objects.end() || !panelIter->visible)
            {
                LOG_WARN("BattleWorldUI skill json panel missing. panel='{}'", panelSpec.panelName);
                return false;
            }

            BattleWorldSkillCardTemplate cardTemplate{};
            cardTemplate.pixelSize = Vec2(
                max(1.f, panelIter->size.x),
                max(1.f, panelIter->size.y));

            for (const BattleWorldSkillJsonObject& object : objects)
            {
                if (!Is_DescendantOf(object, panelIter->id, objectsById))
                    continue;

                if (!Is_VisibleInPanelHierarchy(object, panelIter->id, objectsById))
                    continue;

                BattleWorldSkillCardDef::Layer layer{};
                bool extracted = false;

                if (object.type == "Image")
                    extracted = Try_ExtractJsonImageLayer(device, context, object, panelSpec.panelName, panelIter->id, objectsById, layer);
                else if (object.type == "Text")
                    extracted = Try_ExtractJsonTextLayer(object, panelSpec, panelIter->id, objectsById, layer);

                if (extracted)
                    cardTemplate.layers.push_back(std::move(layer));
            }

            if (cardTemplate.layers.empty())
            {
                LOG_WARN("BattleWorldUI skill json panel has no extracted layers. panel='{}'", panelSpec.panelName);
                return false;
            }

            sort(cardTemplate.layers.begin(), cardTemplate.layers.end(),
                [](const BattleWorldSkillCardDef::Layer& lhs, const BattleWorldSkillCardDef::Layer& rhs)
                {
                    if (lhs.sortOrder != rhs.sortOrder)
                        return lhs.sortOrder < rhs.sortOrder;

                    return lhs.sequence < rhs.sequence;
                });

            outTemplates[slotIndex] = std::move(cardTemplate);
        }

        return true;
    }

    BattleWorldSkillCardPresentation Resolve_SkillPresentation(
        uint32 slotIndex,
        const BattleWorldSkillCardDef& def,
        BattleTurnState state,
        const string& selectedSkillKey)
    {
        BattleWorldSkillCardPresentation presentation{};

        if (!Should_ShowBattleWorldSkillCards(state))
            return presentation;

        if (Is_TargetingSkillCardState(state))
        {
            if (selectedSkillKey.empty() || def.content.skillKey != selectedSkillKey)
                return presentation;

            presentation.visible = true;
            presentation.selected = true;
            presentation.worldOffset = kBattleWorldSkillTargetingOffset;
            presentation.worldSize = kBattleWorldSkillTargetingWorldSize;
            presentation.visualScale = kBattleWorldSkillSelectedScale;
            return presentation;
        }

        if (slotIndex >= kBattleWorldSkillSlotCount)
            return presentation;

        presentation.visible = true;
        presentation.worldOffset = kBattleWorldSkillListOffsets[slotIndex];
        presentation.worldSize = kBattleWorldSkillListWorldSize;
        presentation.visualScale = kBattleWorldSkillDefaultScale;
        return presentation;
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

    class IWorldUISkillCardSurface
    {
    public:
        virtual ~IWorldUISkillCardSurface() = default;

        virtual HRESULT Compose(
            const BattleWorldSkillCardDef& def,
            bool selected,
            bool dimmed) = 0;
        virtual HRESULT Bind_ShaderResource(ShaderCom* shader, const char* name) const = 0;
    };

    class RenderTargetComposedBattleWorldSkillCardSurface final : public IWorldUISkillCardSurface
    {
    public:
        RenderTargetComposedBattleWorldSkillCardSurface(
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

        HRESULT Initialize(uint32 slotIndex)
        {
            _renderTarget = RenderTarget::Create(
                L"BattleWorldSkillCardSurface_" + to_wstring(slotIndex),
                _device,
                _context,
                static_cast<uint32>(max(1.f, roundf(_pixelSize.x * kBattleWorldSkillCompositionRenderScale))),
                static_cast<uint32>(max(1.f, roundf(_pixelSize.y * kBattleWorldSkillCompositionRenderScale))),
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

        HRESULT Compose(
            const BattleWorldSkillCardDef& def,
            bool selected,
            bool dimmed) override
        {
            CHECK_NULL(_renderTarget, E_FAIL);

            ScopedRenderTargetState restoreState{ _context };

            ID3D11ShaderResourceView* nullSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
            _context->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);
            _context->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);

            CHECK_FAILED(_renderTarget->Begin(true, false), E_FAIL);

            for (const auto& layer : def.layers)
            {
                if (layer.kind == BattleWorldSkillCardLayerKind::Image)
                    CHECK_FAILED(Draw_CompositionImage(layer.image, selected, dimmed), E_FAIL);
            }

            for (const auto& layer : def.layers)
            {
                if (layer.kind == BattleWorldSkillCardLayerKind::Text)
                    Draw_CompositionText(layer.text, selected, dimmed);
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

        static Unique<IWorldUISkillCardSurface> Create(
            uint32 slotIndex,
            const ComPtr<Device>& device,
            const ComPtr<Context>& context,
            const Shared<ShaderCom>& shader,
            const Shared<VIBuffer_Rect>& viBuffer,
            const Vec2& pixelSize)
        {
            auto surface = make_unique<RenderTargetComposedBattleWorldSkillCardSurface>(
                device,
                context,
                shader,
                viBuffer,
                pixelSize);
            if (FAILED(surface->Initialize(slotIndex)))
                return nullptr;

            return surface;
        }

    private:
        HRESULT Draw_CompositionImage(const BattleWorldSkillCardImageDef& image, bool selected, bool dimmed)
        {
            CHECK_NULL(image.texture, E_FAIL);
            CHECK_NULL(_shader, E_FAIL);
            CHECK_NULL(_viBuffer, E_FAIL);

            const auto& srvs = image.texture->Get_SRVs();
            if (srvs.empty() || srvs[0] == nullptr)
                return E_FAIL;

            uint32 passIndex = 0u;
            if (image.imageRenderMode == BattleWorldSkillImageRenderMode::PatternOverlay)
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
                passIndex = (image.maskTextureMode == BattleWorldSkillMaskTextureMode::BlackWhite) ? 5u : 1u;
            }

            const Vec4 drawColor = Resolve_ImageColor(image.color, selected, dimmed);

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
                image.imageRenderMode == BattleWorldSkillImageRenderMode::PatternOverlay ? image.patternUvOffset : defaultPatternOffset;
            const Vec2 patternUvScale =
                image.imageRenderMode == BattleWorldSkillImageRenderMode::PatternOverlay ? image.patternUvScale : defaultPatternScale;

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

        void Draw_CompositionText(const BattleWorldSkillCardTextDef& text, bool selected, bool dimmed)
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
            if (text.align == BattleWorldSkillTextAlign::Center)
                offsetX = (boxWidth - textWidth) * 0.5f;
            else if (text.align == BattleWorldSkillTextAlign::Right)
                offsetX = boxWidth - textWidth;

            const Vec2 alignedTopLeft(boxTopLeft.x + offsetX, boxTopLeft.y);
            const Vec2 pivot(
                (text.position.x - alignedTopLeft.x) / textScale,
                (text.position.y - alignedTopLeft.y) / textScale);

            const Vec4 drawColor = Resolve_TextColor(text.color, selected, dimmed);

            GAME->Draw_Font(
                text.fontTag,
                text.text.c_str(),
                text.position * kBattleWorldSkillCompositionRenderScale,
                XMVectorSet(drawColor.x, drawColor.y, drawColor.z, drawColor.w),
                text.rotationRadians,
                pivot,
                textScale * kBattleWorldSkillCompositionRenderScale);
        }

        ComPtr<Device> _device{};
        ComPtr<Context> _context{};
        Shared<ShaderCom> _shader{};
        Shared<VIBuffer_Rect> _viBuffer{};
        Shared<RenderTarget> _renderTarget{};
        ComPtr<ID3D11BlendState> _compositionBlendState{};
        Vec2 _pixelSize = Vec2(
            static_cast<float>(kBattleWorldSkillFallbackPixelWidth),
            static_cast<float>(kBattleWorldSkillFallbackPixelHeight));
    };

}

struct BattleWorldUI_SkillCards::Impl
{
    Shared<ShaderCom> shader{};
    Shared<VIBuffer_Rect> viBuffer{};
    array<BattleWorldSkillCardTemplate, kBattleWorldSkillSlotCount> templates{};
    array<BattleWorldSkillCardInstance, kBattleWorldSkillSlotCount> cards{};
};

BattleWorldUI_SkillCards::BattleWorldUI_SkillCards(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

BattleWorldUI_SkillCards::~BattleWorldUI_SkillCards() = default;

HRESULT BattleWorldUI_SkillCards::Initialize(void* arg)
{
    CHECK_FAILED(GameObject::Initialize(arg), E_FAIL);

    _impl = make_unique<Impl>();
    CHECK_NULL(_impl, E_FAIL);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Shader_UI"), _impl->shader), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("VIBuffer_Rect"), _impl->viBuffer), E_FAIL);

    Shared<Texture> fallbackCardTexture = BattleWorldUI_JsonTextureResolver::Load_TextureFromUIImage(
        _device,
        _context,
        kBattleWorldSkillPanelJsonPath,
        "Image_LumiereAssault_Frame",
        kBattleWorldSkillCardTextureGuid);
    CHECK_NULL(fallbackCardTexture, E_FAIL);

    Shared<Texture> fallbackKeyFrameTexture = BattleWorldUI_JsonTextureResolver::Load_TextureFromUIImage(
        _device,
        _context,
        kBattleWorldSkillPanelJsonPath,
        "Image_KeyFrame_Q",
        kBattleWorldKeyFrameTextureGuid);
    CHECK_NULL(fallbackKeyFrameTexture, E_FAIL);

    const BattleWorldSkillCardTemplate fallbackTemplate =
        Build_FallbackSkillCardTemplate(fallbackCardTexture, fallbackKeyFrameTexture);
    for (auto& cardTemplate : _impl->templates)
        cardTemplate = fallbackTemplate;

    array<BattleWorldSkillCardTemplate, kBattleWorldSkillSlotCount> jsonTemplates{};
    if (Try_LoadJsonSkillCardTemplates(_device, _context, jsonTemplates))
        _impl->templates = std::move(jsonTemplates);

    for (uint32 slotIndex = 0; slotIndex < kBattleWorldSkillSlotCount; ++slotIndex)
    {
        _impl->cards[slotIndex].surface = RenderTargetComposedBattleWorldSkillCardSurface::Create(
            slotIndex,
            _device,
            _context,
            _impl->shader,
            _impl->viBuffer,
            _impl->templates[slotIndex].pixelSize);
        CHECK_NULL(_impl->cards[slotIndex].surface, E_FAIL);
    }

    Hide();
    return S_OK;
}

void BattleWorldUI_SkillCards::Show_ForBattleState(
    const Vec3& anchorWorldPos,
    BattleTurnState currentState,
    const BattleChar& actor,
    const string& selectedSkillKey)
{
    if (_impl == nullptr || !Should_ShowBattleWorldSkillCards(currentState) || actor.statCom == nullptr)
    {
        Hide();
        return;
    }

    bool anyVisible = false;
    for (uint32 slotIndex = 0; slotIndex < kBattleWorldSkillSlotCount; ++slotIndex)
    {
        auto& card = _impl->cards[slotIndex];
        card.visible = false;

        string skillKey{};
        if (!actor.statCom->Try_GetSkillKeyBySlot(slotIndex, skillKey))
        {
            card.hasContent = false;
            continue;
        }

        const Skill_Data* skill = CLIENT->Get_Skill(skillKey);
        if (skill == nullptr)
        {
            card.hasContent = false;
            continue;
        }

        const BattleWorldSkillCardContent content = Build_SkillCardContent(slotIndex, skillKey, *skill);
        if (!card.hasContent || !Is_SameContent(card.def.content, content))
        {
            card.def = Build_SkillCardDef(content, _impl->templates[slotIndex]);
            card.hasContent = true;
            card.dirty = true;
        }

        const BattleWorldSkillCardPresentation presentation =
            Resolve_SkillPresentation(slotIndex, card.def, currentState, selectedSkillKey);

        if (card.selected != presentation.selected || card.dimmed != presentation.dimmed)
        {
            card.selected = presentation.selected;
            card.dimmed = presentation.dimmed;
            card.dirty = true;
        }

        if (!presentation.visible)
            continue;

        if (card.dirty)
        {
            if (card.surface == nullptr || FAILED(card.surface->Compose(card.def, card.selected, card.dimmed)))
                continue;

            card.dirty = false;
        }

        const Vec3 position = anchorWorldPos + presentation.worldOffset;
        const Vec2 aspectWorldSize = Resolve_AspectWorldSize(presentation.worldSize, card.def.pixelSize);
        card.worldMatrix =
            Matrix::CreateScale(
                aspectWorldSize.x * presentation.visualScale,
                aspectWorldSize.y * presentation.visualScale,
                1.f) *
            Matrix::CreateTranslation(position.x, position.y, position.z);
        card.visible = true;
        anyVisible = true;
    }

    Set_Visible(anyVisible);
}

void BattleWorldUI_SkillCards::Hide()
{
    Set_Visible(false);

    if (_impl == nullptr)
        return;

    for (auto& card : _impl->cards)
        card.visible = false;
}

void BattleWorldUI_SkillCards::Late_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!Is_Visible())
        return;

    GAME->Add_RenderGroup(RenderGroup::UIEffect, GetSharedPtr<GameObject>());
}

HRESULT BattleWorldUI_SkillCards::Render()
{
    if (!Is_Visible() || _impl == nullptr || _impl->shader == nullptr || _impl->viBuffer == nullptr)
        return S_OK;

    const Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);

    for (const auto& card : _impl->cards)
    {
        if (!card.visible || card.surface == nullptr)
            continue;

        CHECK_FAILED(Bind_WorldUIPlaneShaderState(_impl->shader.get(), card.worldMatrix, color), E_FAIL);
        CHECK_FAILED(card.surface->Bind_ShaderResource(_impl->shader.get(), "g_Texture"), E_FAIL);
        CHECK_FAILED(_impl->shader->Begin(0), E_FAIL);
        CHECK_FAILED(_impl->viBuffer->Bind_Resources(), E_FAIL);
        CHECK_FAILED(_impl->viBuffer->Render(), E_FAIL);
    }

    return S_OK;
}

Shared<GameObject> BattleWorldUI_SkillCards::Clone(void* arg)
{
    UNREFERENCED_PARAMETER(arg);
    return nullptr;
}

void BattleWorldUI_SkillCards::Free()
{
    if (_impl)
    {
        for (auto& card : _impl->cards)
            card.surface.reset();

        _impl->shader.reset();
        _impl->viBuffer.reset();
        _impl.reset();
    }

    __super::Free();
}

Shared<BattleWorldUI_SkillCards> BattleWorldUI_SkillCards::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context)
{
    auto instance = make_shared<BattleWorldUI_SkillCards>(device, context);
    if (FAILED(instance->Initialize(nullptr)))
    {
        instance->Free();
        return nullptr;
    }

    return instance;
}

NS_END
