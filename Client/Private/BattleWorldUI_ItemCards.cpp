#include "BattleWorldUI_ItemCards.h"

#include "BattleWorldUI_JsonTextureResolver.h"
#include "Camera.h"
#include "ClientInstance.h"
#include "RenderTarget.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "TransformCom.h"
#include "VIBuffer_Rect.h"

NS_BEGIN(Client)

namespace
{
    constexpr char kBattleWorldItemCardTextureGuid[] = "bec2ba8a-a265-40d8-9bd1-bc85420ebaa2";
    constexpr char kBattleWorldKeyFrameTextureGuid[] = "72de5c45-b20f-490d-9d69-396a830b514f";
    constexpr wchar_t kBattleWorldItemPanelJsonPath[] = L"UI/UI_Json/BattleSystem/Item/UI_BattleSystem_Item.json";
    constexpr wchar_t kBattleWorldItemPlacementJsonPath[] = L"UI/UI_Json/BattleSystem/Item/UI_BattleSystem_Item_WorldPlacement.json";
    constexpr uint32 kBattleWorldItemFallbackPixelWidth = 768;
    constexpr uint32 kBattleWorldItemFallbackPixelHeight = 192;
    constexpr uint32 kBattleWorldItemSlotCount = 3;
    constexpr uint32 kBattleWorldItemPlacementHeroCount = static_cast<uint32>(HeroId::END);
    constexpr float kBattleWorldItemCompositionRenderScale = 2.f;
    constexpr float kBattleWorldItemDegreesToRadians = 0.017453292519943295f;
    constexpr float kBattleWorldItemRadiansToDegrees = 57.29577951308232f;
    const Vec2 kBattleWorldItemListWorldSize = Vec2(1.00f, 0.25f);
    const Vec3 kBattleWorldItemListOffsets[kBattleWorldItemSlotCount] =
    {
        Vec3(0.50f, 1.02f, 0.f),
        Vec3(0.50f, 0.75f, 0.f),
        Vec3(0.50f, 0.48f, 0.f),
    };
    constexpr float kBattleWorldItemDefaultScale = 1.f;

    enum class BattleWorldItemCardTextSlot
    {
        Literal,
        KeyPrompt,
        Title,
        Description,
        SubDescription,
        Count,
        PrimaryValue,
    };

    enum class BattleWorldItemCardLayerKind
    {
        Image,
        Text,
    };

    enum class BattleWorldItemTextAlign
    {
        Left,
        Center,
        Right,
    };

    enum class BattleWorldItemImageRenderMode
    {
        Normal,
        PatternOverlay,
        Unknown,
    };

    enum class BattleWorldItemMaskTextureMode
    {
        Alpha,
        BlackWhite,
        Unknown,
    };

    struct BattleWorldItemSlotDef
    {
        BattleItemType itemType = BattleItemType::Invalid;
        const wchar_t* keyPrompt = L"";
        const char* panelName = "";
        const char* textPrefix = "";
    };

    const BattleWorldItemSlotDef kBattleWorldItemSlots[kBattleWorldItemSlotCount] =
    {
        { BattleItemType::HealingTint, L"Q", "Panel_Healing_Tint", "Healing_Tint" },
        { BattleItemType::EnergyTint, L"W", "Panel_Energy_Tint", "Energy_Tint" },
        { BattleItemType::ReviveTint, L"E", "Panel_Revive_Tint", "Revive_Tint" },
    };

    struct BattleWorldItemCardTextDef
    {
        BattleWorldItemCardTextSlot slot = BattleWorldItemCardTextSlot::Literal;
        wstring text;
        wstring fontTag;
        Vec2 size{};
        Vec2 pivot = Vec2(0.f, 0.f);
        Vec2 position{};
        float scale = 1.f;
        float rotationRadians = 0.f;
        Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);
        BattleWorldItemTextAlign align = BattleWorldItemTextAlign::Left;
        bool textWrap = false;
        bool useRuntimeText = false;
    };

    struct BattleWorldItemCardImageDef
    {
        Shared<Texture> texture{};
        Vec2 center{};
        Vec2 size{};
        Vec2 pivot = Vec2(0.5f, 0.5f);
        float rotationRadians = 0.f;
        Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);
        BattleWorldItemImageRenderMode imageRenderMode = BattleWorldItemImageRenderMode::Normal;
        Shared<Texture> maskTexture{};
        BattleWorldItemMaskTextureMode maskTextureMode = BattleWorldItemMaskTextureMode::Alpha;
        Shared<Texture> patternMaskTexture{};
        Vec2 patternUvOffset = Vec2(0.f, 0.f);
        Vec2 patternUvScale = Vec2(1.f, 1.f);
    };

    struct BattleWorldItemCardContent
    {
        BattleItemType itemType = BattleItemType::Invalid;
        wstring keyPrompt;
        wstring title;
        wstring description;
        wstring subDescription;
        int32 count = 0;
        int32 primaryValue = 0;
    };

    struct BattleWorldItemCardDef
    {
        BattleWorldItemCardContent content{};
        Vec2 pixelSize = Vec2(
            static_cast<float>(kBattleWorldItemFallbackPixelWidth),
            static_cast<float>(kBattleWorldItemFallbackPixelHeight));

        struct Layer
        {
            BattleWorldItemCardLayerKind kind = BattleWorldItemCardLayerKind::Image;
            int32 sortOrder = 0;
            uint32 sequence = 0;
            BattleWorldItemCardImageDef image{};
            BattleWorldItemCardTextDef text{};
        };

        vector<Layer> layers{};
    };

    struct BattleWorldItemCardTemplate
    {
        Vec2 pixelSize = Vec2(
            static_cast<float>(kBattleWorldItemFallbackPixelWidth),
            static_cast<float>(kBattleWorldItemFallbackPixelHeight));
        vector<BattleWorldItemCardDef::Layer> layers{};
    };

    class IWorldUIItemCardSurface;

    struct BattleWorldItemCardInstance
    {
        BattleWorldItemCardDef def{};
        Unique<IWorldUIItemCardSurface> surface{};
        Matrix worldMatrix = Matrix::Identity;
        bool hasContent = false;
        bool visible = false;
        bool selected = false;
        bool dimmed = false;
        bool dirty = true;
    };

    struct BattleWorldItemCardPresentation
    {
        bool visible = false;
        bool selected = false;
        bool dimmed = false;
        Vec3 screenPosition = Vec3::Zero;
        Vec3 worldOffset = Vec3::Zero;
        Vec2 worldSize = kBattleWorldItemListWorldSize;
        float visualScale = kBattleWorldItemDefaultScale;
        float pitchRadians = 0.f;
        float rollRadians = 0.f;
        float yawOffsetRadians = 0.f;
    };

    struct BattleWorldItemWorldPlacementTuning
    {
        Vec3 screenPosition = Vec3::Zero;
        Vec3 baseOffset = Vec3::Zero;
        array<Vec3, kBattleWorldItemSlotCount> slotOffsets =
        {
            kBattleWorldItemListOffsets[0],
            kBattleWorldItemListOffsets[1],
            kBattleWorldItemListOffsets[2],
        };
        Vec2 worldSize = kBattleWorldItemListWorldSize;
        float visualScale = kBattleWorldItemDefaultScale;
        float pitchRadians = 0.f;
        float rollRadians = 0.f;
        float yawOffsetRadians = 0.f;
    };

    HeroId Normalize_PlacementHeroId(HeroId heroId)
    {
        const uint32 heroIndex = static_cast<uint32>(heroId);
        if (heroIndex < kBattleWorldItemPlacementHeroCount)
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

    BattleWorldItemWorldPlacementTuning Make_DefaultItemWorldPlacementTuning(HeroId heroId)
    {
        BattleWorldItemWorldPlacementTuning tuning{};
        tuning.slotOffsets[0] = Vec3(0.50f, 1.00f, 0.f);
        tuning.slotOffsets[1] = Vec3(0.50f, 0.67f, 0.f);
        tuning.slotOffsets[2] = Vec3(0.50f, 0.34f, 0.f);
        tuning.worldSize = Vec2(1.15f, 0.25f);

        switch (Normalize_PlacementHeroId(heroId))
        {
        case HeroId::Gustave:
            tuning.screenPosition = Vec3(3.30f, 0.37f, 2.22f);
            tuning.baseOffset = Vec3(2.68f, 0.55f, 0.f);
            tuning.yawOffsetRadians = -8.f * kBattleWorldItemDegreesToRadians;
            break;
        case HeroId::Lune:
            tuning.screenPosition = Vec3(2.77f, 0.47f, 1.20f);
            tuning.baseOffset = Vec3(1.63f, 0.59f, -1.12f);
            tuning.yawOffsetRadians = 6.f * kBattleWorldItemDegreesToRadians;
            break;
        case HeroId::Maelle:
            tuning.screenPosition = Vec3(4.09f, 0.48f, 1.91f);
            tuning.baseOffset = Vec3(3.02f, 0.59f, -1.12f);
            tuning.yawOffsetRadians = -13.f * kBattleWorldItemDegreesToRadians;
            break;
        case HeroId::END:
        default:
            break;
        }

        return tuning;
    }

    array<BattleWorldItemWorldPlacementTuning, kBattleWorldItemPlacementHeroCount>
        Make_DefaultItemWorldPlacementTunings()
    {
        array<BattleWorldItemWorldPlacementTuning, kBattleWorldItemPlacementHeroCount> tunings{};
        for (uint32 heroIndex = 0; heroIndex < kBattleWorldItemPlacementHeroCount; ++heroIndex)
            tunings[heroIndex] = Make_DefaultItemWorldPlacementTuning(static_cast<HeroId>(heroIndex));

        return tunings;
    }

    struct BattleWorldItemJsonObject
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

    using BattleWorldItemJsonObjectMap = unordered_map<int32, BattleWorldItemJsonObject>;

    bool Should_ShowBattleWorldItemCards(BattleTurnState state)
    {
        return state == BattleTurnState::ChooseItem;
    }

    wstring Resolve_ItemFallbackTitle(BattleItemType itemType)
    {
        switch (itemType)
        {
        case BattleItemType::HealingTint:
            return L"Healing Tint";
        case BattleItemType::EnergyTint:
            return L"Energy Tint";
        case BattleItemType::ReviveTint:
            return L"Revive Tint";
        case BattleItemType::Invalid:
        default:
            return L"Item";
        }
    }

    wstring Resolve_ItemTitle(const Item_Data& item)
    {
        if (!item.displayName.empty())
            return item.displayName;

        return Resolve_ItemFallbackTitle(item.itemType);
    }

    int32 Resolve_ItemPrimaryValue(const Item_Data& item)
    {
        switch (item.itemType)
        {
        case BattleItemType::HealingTint:
            return item.healHp;
        case BattleItemType::EnergyTint:
            return item.restoreAp;
        case BattleItemType::ReviveTint:
            return item.reviveHp;
        case BattleItemType::Invalid:
        default:
            return 0;
        }
    }

    wstring Resolve_ItemDescription(const Item_Data& item)
    {
        switch (item.itemType)
        {
        case BattleItemType::HealingTint:
            return L"Heal " + to_wstring(max(0, item.healHp)) + L"%";
        case BattleItemType::EnergyTint:
            return L"Restore " + to_wstring(max(0, item.restoreAp)) + L" AP";
        case BattleItemType::ReviveTint:
            return L"Revive " + to_wstring(max(0, item.reviveHp)) + L"% HP";
        case BattleItemType::Invalid:
        default:
            return L"";
        }
    }

    BattleWorldItemCardContent Build_ItemCardContent(
        const BattleWorldItemSlotDef& slot,
        const Item_Data& item)
    {
        BattleWorldItemCardContent content{};
        content.itemType = item.itemType;
        content.keyPrompt = slot.keyPrompt;
        content.title = Resolve_ItemTitle(item);
        content.description = Resolve_ItemDescription(item);
        content.subDescription = L"Count " + to_wstring(max(0, item.count));
        content.count = item.count;
        content.primaryValue = Resolve_ItemPrimaryValue(item);
        return content;
    }

    bool Is_SameContent(const BattleWorldItemCardContent& lhs, const BattleWorldItemCardContent& rhs)
    {
        return lhs.itemType == rhs.itemType &&
            lhs.keyPrompt == rhs.keyPrompt &&
            lhs.title == rhs.title &&
            lhs.description == rhs.description &&
            lhs.subDescription == rhs.subDescription &&
            lhs.count == rhs.count &&
            lhs.primaryValue == rhs.primaryValue;
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

    BattleWorldItemImageRenderMode Parse_JsonImageRenderMode(const string& modeName)
    {
        if (modeName.empty())
            return BattleWorldItemImageRenderMode::Normal;

        const string mode = String::ToLowerCopy(modeName);
        if ("normal" == mode)
            return BattleWorldItemImageRenderMode::Normal;
        if ("patternoverlay" == mode || "pattern_overlay" == mode || "pattern overlay" == mode)
            return BattleWorldItemImageRenderMode::PatternOverlay;

        return BattleWorldItemImageRenderMode::Unknown;
    }

    BattleWorldItemMaskTextureMode Parse_JsonMaskTextureMode(const string& modeName)
    {
        if (modeName.empty())
            return BattleWorldItemMaskTextureMode::Alpha;

        const string mode = String::ToLowerCopy(modeName);
        if ("alpha" == mode)
            return BattleWorldItemMaskTextureMode::Alpha;
        if ("blackwhite" == mode || "black_white" == mode || "black white" == mode)
            return BattleWorldItemMaskTextureMode::BlackWhite;

        return BattleWorldItemMaskTextureMode::Unknown;
    }

    const char* To_LogName(BattleWorldItemImageRenderMode mode)
    {
        switch (mode)
        {
        case BattleWorldItemImageRenderMode::Normal:
            return "Normal";
        case BattleWorldItemImageRenderMode::PatternOverlay:
            return "PatternOverlay";
        case BattleWorldItemImageRenderMode::Unknown:
        default:
            return "Unknown";
        }
    }

    const char* To_LogName(BattleWorldItemMaskTextureMode mode)
    {
        switch (mode)
        {
        case BattleWorldItemMaskTextureMode::Alpha:
            return "Alpha";
        case BattleWorldItemMaskTextureMode::BlackWhite:
            return "BlackWhite";
        case BattleWorldItemMaskTextureMode::Unknown:
        default:
            return "Unknown";
        }
    }

    bool Has_TextureRef(const string& textureGuid, const string& texturePath)
    {
        return !textureGuid.empty() || !texturePath.empty();
    }

    BattleWorldItemJsonObject Parse_JsonObject(const json& node, uint32 sequence)
    {
        BattleWorldItemJsonObject object{};
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
        const BattleWorldItemJsonObject& parent)
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

    const BattleWorldItemJsonObject* Find_JsonObject(
        const BattleWorldItemJsonObjectMap& objectsById,
        int32 objectId)
    {
        const auto iter = objectsById.find(objectId);
        if (iter == objectsById.end())
            return nullptr;

        return &iter->second;
    }

    bool Is_DescendantOf(
        const BattleWorldItemJsonObject& object,
        int32 panelId,
        const BattleWorldItemJsonObjectMap& objectsById)
    {
        int32 parentId = object.parentId;
        for (uint32 depth = 0; depth < 256 && parentId != 0; ++depth)
        {
            if (parentId == panelId)
                return true;

            const BattleWorldItemJsonObject* parent = Find_JsonObject(objectsById, parentId);
            if (parent == nullptr)
                break;

            parentId = parent->parentId;
        }

        return false;
    }

    bool Is_VisibleInPanelHierarchy(
        const BattleWorldItemJsonObject& object,
        int32 panelId,
        const BattleWorldItemJsonObjectMap& objectsById)
    {
        if (!object.visible)
            return false;

        int32 parentId = object.parentId;
        for (uint32 depth = 0; depth < 256 && parentId != 0; ++depth)
        {
            if (parentId == panelId)
                return true;

            const BattleWorldItemJsonObject* parent = Find_JsonObject(objectsById, parentId);
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
        const BattleWorldItemJsonObjectMap& objectsById)
    {
        for (uint32 depth = 0; depth < 256 && parentId != 0 && parentId != panelId; ++depth)
        {
            const BattleWorldItemJsonObject* parent = Find_JsonObject(objectsById, parentId);
            if (parent == nullptr)
                break;

            point = Transform_PointByJsonParent(point, *parent);
            parentId = parent->parentId;
        }

        return point;
    }

    Vec2 Resolve_JsonWorldScale(
        const BattleWorldItemJsonObject& object,
        int32 panelId,
        const BattleWorldItemJsonObjectMap& objectsById)
    {
        Vec2 scale = object.scale;
        int32 parentId = object.parentId;

        for (uint32 depth = 0; depth < 256 && parentId != 0 && parentId != panelId; ++depth)
        {
            const BattleWorldItemJsonObject* parent = Find_JsonObject(objectsById, parentId);
            if (parent == nullptr)
                break;

            scale.x *= parent->scale.x;
            scale.y *= parent->scale.y;
            parentId = parent->parentId;
        }

        return scale;
    }

    float Resolve_JsonWorldRotation(
        const BattleWorldItemJsonObject& object,
        int32 panelId,
        const BattleWorldItemJsonObjectMap& objectsById)
    {
        float rotation = object.rotationRadians;
        int32 parentId = object.parentId;

        for (uint32 depth = 0; depth < 256 && parentId != 0 && parentId != panelId; ++depth)
        {
            const BattleWorldItemJsonObject* parent = Find_JsonObject(objectsById, parentId);
            if (parent == nullptr)
                break;

            rotation += parent->rotationRadians;
            parentId = parent->parentId;
        }

        return rotation;
    }

    float Resolve_JsonAlphaMultiplier(
        const BattleWorldItemJsonObject& object,
        int32 panelId,
        const BattleWorldItemJsonObjectMap& objectsById)
    {
        float alphaMultiplier = object.alphaMultiplier;
        int32 parentId = object.parentId;

        for (uint32 depth = 0; depth < 256 && parentId != 0 && parentId != panelId; ++depth)
        {
            const BattleWorldItemJsonObject* parent = Find_JsonObject(objectsById, parentId);
            if (parent == nullptr)
                break;

            alphaMultiplier *= parent->alphaMultiplier;
            parentId = parent->parentId;
        }

        return alphaMultiplier;
    }

    Vec2 Resolve_JsonImageCenter(const BattleWorldItemJsonObject& object)
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

    BattleWorldItemTextAlign Resolve_TextAlign(const string& textAlign)
    {
        const string normalizedTextAlign = String::ToLowerCopy(textAlign);
        if (normalizedTextAlign == "center")
            return BattleWorldItemTextAlign::Center;
        if (normalizedTextAlign == "right")
            return BattleWorldItemTextAlign::Right;

        return BattleWorldItemTextAlign::Left;
    }

    BattleWorldItemCardTextSlot Resolve_ItemJsonTextSlot(
        const string& objectName,
        const string& textPrefix)
    {
        const string countName = "Text_" + textPrefix + "_Count";
        if (objectName == countName)
            return BattleWorldItemCardTextSlot::Count;

        const string valueName = "Text_" + textPrefix + "_Description_2";
        if (objectName == valueName)
            return BattleWorldItemCardTextSlot::PrimaryValue;

        return BattleWorldItemCardTextSlot::Literal;
    }

    wstring Resolve_TextForSlot(
        BattleWorldItemCardTextSlot slot,
        const BattleWorldItemCardContent& content,
        const wstring& literalText)
    {
        switch (slot)
        {
        case BattleWorldItemCardTextSlot::KeyPrompt:
            return content.keyPrompt;
        case BattleWorldItemCardTextSlot::Title:
            return content.title;
        case BattleWorldItemCardTextSlot::Description:
            return content.description;
        case BattleWorldItemCardTextSlot::SubDescription:
            return content.subDescription;
        case BattleWorldItemCardTextSlot::Count:
            return to_wstring(max(content.count, 0));
        case BattleWorldItemCardTextSlot::PrimaryValue:
            return to_wstring(max(content.primaryValue, 0));
        case BattleWorldItemCardTextSlot::Literal:
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
            return Vec4(color.x * 0.48f, color.y * 0.48f, color.z * 0.48f, color.w * 0.48f);

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
            return Vec4(color.x * 0.60f, color.y * 0.60f, color.z * 0.60f, color.w * 0.55f);

        return color;
    }

    Vec4 Resolve_ItemTint(BattleItemType itemType)
    {
        switch (itemType)
        {
        case BattleItemType::HealingTint:
            return Vec4(118.f / 255.f, 152.f / 255.f, 104.f / 255.f, 1.f);
        case BattleItemType::EnergyTint:
            return Vec4(74.f / 255.f, 104.f / 255.f, 170.f / 255.f, 1.f);
        case BattleItemType::ReviveTint:
            return Vec4(172.f / 255.f, 132.f / 255.f, 91.f / 255.f, 1.f);
        case BattleItemType::Invalid:
        default:
            return Vec4(1.f, 1.f, 1.f, 1.f);
        }
    }

    BattleWorldItemCardTemplate Build_FallbackItemCardTemplate(
        const Shared<Texture>& cardTexture,
        const Shared<Texture>& keyFrameTexture)
    {
        BattleWorldItemCardTemplate cardTemplate{};
        cardTemplate.pixelSize = Vec2(
            static_cast<float>(kBattleWorldItemFallbackPixelWidth),
            static_cast<float>(kBattleWorldItemFallbackPixelHeight));

        cardTemplate.layers.push_back(
            {
                BattleWorldItemCardLayerKind::Image,
                0,
                0,
                {
                    cardTexture,
                    Vec2(388.f, 96.f),
                    Vec2(690.f, 156.f),
                    Vec2(0.5f, 0.5f),
                    0.f,
                    Vec4(1.f, 1.f, 1.f, 1.f)
                }
            });

        cardTemplate.layers.push_back(
            {
                BattleWorldItemCardLayerKind::Image,
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
                BattleWorldItemCardLayerKind::Text,
                2,
                2,
                {},
                {
                    BattleWorldItemCardTextSlot::KeyPrompt,
                    {},
                    L"33_SubKR_Font",
                    Vec2(62.f, 62.f),
                    Vec2(0.5f, 0.5f),
                    Vec2(102.f, 95.f),
                    1.12f,
                    0.f,
                    Make_PremultipliedSpriteFontColor(Vec4(236.f / 255.f, 207.f / 255.f, 165.f / 255.f, 1.f)),
                    BattleWorldItemTextAlign::Center,
                    false,
                    true
                }
            });

        cardTemplate.layers.push_back(
            {
                BattleWorldItemCardLayerKind::Text,
                3,
                3,
                {},
                {
                    BattleWorldItemCardTextSlot::Title,
                    {},
                    L"33_MainKR_Font",
                    Vec2(420.f, 44.f),
                    Vec2(0.f, 0.f),
                    Vec2(158.f, 46.f),
                    1.20f,
                    0.f,
                    Make_PremultipliedSpriteFontColor(Vec4(1.f, 0.92f, 0.78f, 1.f)),
                    BattleWorldItemTextAlign::Left,
                    false,
                    true
                }
            });

        cardTemplate.layers.push_back(
            {
                BattleWorldItemCardLayerKind::Text,
                4,
                4,
                {},
                {
                    BattleWorldItemCardTextSlot::Description,
                    {},
                    L"33_SubKR_Font",
                    Vec2(470.f, 36.f),
                    Vec2(0.f, 0.f),
                    Vec2(160.f, 94.f),
                    0.80f,
                    0.f,
                    Make_PremultipliedSpriteFontColor(Vec4(0.95f, 0.90f, 0.80f, 0.96f)),
                    BattleWorldItemTextAlign::Left,
                    false,
                    true
                }
            });

        cardTemplate.layers.push_back(
            {
                BattleWorldItemCardLayerKind::Text,
                5,
                5,
                {},
                {
                    BattleWorldItemCardTextSlot::SubDescription,
                    {},
                    L"33_SubKR_Font",
                    Vec2(470.f, 34.f),
                    Vec2(0.f, 0.f),
                    Vec2(160.f, 128.f),
                    0.72f,
                    0.f,
                    Make_PremultipliedSpriteFontColor(Vec4(0.86f, 0.92f, 1.f, 0.82f)),
                    BattleWorldItemTextAlign::Left,
                    false,
                    true
                }
            });

        return cardTemplate;
    }

    BattleWorldItemCardDef Build_ItemCardDef(
        const BattleWorldItemCardContent& content,
        const BattleWorldItemCardTemplate& cardTemplate)
    {
        BattleWorldItemCardDef def{};
        def.content = content;
        def.pixelSize = cardTemplate.pixelSize;
        def.layers = cardTemplate.layers;

        for (auto& layer : def.layers)
        {
            if (layer.kind != BattleWorldItemCardLayerKind::Text)
                continue;

            if (layer.text.useRuntimeText)
                layer.text.text = Resolve_TextForSlot(layer.text.slot, content, layer.text.text);
        }

        return def;
    }

    bool Try_ExtractJsonImageLayer(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const BattleWorldItemJsonObject& object,
        const char* panelName,
        int32 panelId,
        const BattleWorldItemJsonObjectMap& objectsById,
        BattleWorldItemCardDef::Layer& outLayer)
    {
        BattleWorldItemImageRenderMode imageRenderMode =
            Parse_JsonImageRenderMode(object.imageRenderMode);
        const BattleWorldItemMaskTextureMode maskTextureMode =
            Parse_JsonMaskTextureMode(object.maskTextureMode);

        if (imageRenderMode == BattleWorldItemImageRenderMode::Normal &&
            Has_TextureRef(object.patternMaskTextureGuid, object.patternMaskTexturePath))
        {
            imageRenderMode = BattleWorldItemImageRenderMode::PatternOverlay;
        }

        if (imageRenderMode == BattleWorldItemImageRenderMode::Unknown)
        {
            LOG_WARN(
                "BattleWorldUI item json image skipped: unknown imageRenderMode. panel='{}', name='{}', mode='{}'",
                panelName,
                object.name,
                object.imageRenderMode);
            return false;
        }

        if (object.useFillRatio)
        {
            LOG_DEBUG(
                "BattleWorldUI item json image skipped: unsupported fillRatio. panel='{}', name='{}', fillRatio={}",
                panelName,
                object.name,
                object.fillRatio);
            return false;
        }

        if (imageRenderMode != BattleWorldItemImageRenderMode::PatternOverlay &&
            Has_TextureRef(object.maskTextureGuid, object.maskTexturePath) &&
            maskTextureMode == BattleWorldItemMaskTextureMode::Unknown)
        {
            LOG_WARN(
                "BattleWorldUI item json image skipped: unknown maskTextureMode. panel='{}', name='{}', mode='{}'",
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
        const bool requiresMaskTexture = hasMaskRef || imageRenderMode == BattleWorldItemImageRenderMode::PatternOverlay;
        if (requiresMaskTexture)
        {
            if (!hasMaskRef)
            {
                LOG_WARN(
                    "BattleWorldUI item json image skipped: missing required mask ref. panel='{}', name='{}', mode='{}'",
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
                    "BattleWorldUI item json image skipped: missing required mask. panel='{}', name='{}', maskPath='{}'",
                    panelName,
                    object.name,
                    object.maskTexturePath);
                return false;
            }
        }

        Shared<Texture> patternMaskTexture{};
        const bool hasPatternMaskRef = Has_TextureRef(object.patternMaskTextureGuid, object.patternMaskTexturePath);
        if (imageRenderMode == BattleWorldItemImageRenderMode::PatternOverlay)
        {
            if (!hasPatternMaskRef)
            {
                LOG_WARN(
                    "BattleWorldUI item json image skipped: missing required pattern mask ref. panel='{}', name='{}'",
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
                    "BattleWorldUI item json image skipped: missing required pattern mask. panel='{}', name='{}', patternMaskPath='{}'",
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

        outLayer.kind = BattleWorldItemCardLayerKind::Image;
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
            "BattleWorldUI item json image supported. panel='{}', name='{}', sort={}, mode='{}', maskMode='{}', hasMask={}, hasPatternMask={}",
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
        const BattleWorldItemJsonObject& object,
        const BattleWorldItemSlotDef& slotSpec,
        int32 panelId,
        const BattleWorldItemJsonObjectMap& objectsById,
        BattleWorldItemCardDef::Layer& outLayer)
    {
        if (object.text.empty())
            return false;

        if (!Ensure_JsonFontLoaded(object.fontTag, object.fontPath))
            return false;

        const Vec2 worldScale = Resolve_JsonWorldScale(object, panelId, objectsById);
        const Vec2 panelLocalPosition = Transform_PointToPanelLocal(object.position, object.parentId, panelId, objectsById);

        Vec4 color = object.color;
        color.w *= Resolve_JsonAlphaMultiplier(object, panelId, objectsById);

        const BattleWorldItemCardTextSlot textSlot = Resolve_ItemJsonTextSlot(object.name, slotSpec.textPrefix);

        outLayer.kind = BattleWorldItemCardLayerKind::Text;
        outLayer.sortOrder = object.sortOrder;
        outLayer.sequence = object.sequence;
        outLayer.text.slot = textSlot;
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
        outLayer.text.useRuntimeText =
            textSlot == BattleWorldItemCardTextSlot::Count ||
            textSlot == BattleWorldItemCardTextSlot::PrimaryValue;
        return true;
    }

    bool Try_LoadJsonItemCardTemplates(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        array<BattleWorldItemCardTemplate, kBattleWorldItemSlotCount>& outTemplates)
    {
        const fs::path jsonPath = (fs::path(GAME->Get_AssetRoot()) / kBattleWorldItemPanelJsonPath).lexically_normal();
        if (!fs::exists(jsonPath))
        {
            LOG_WARN("BattleWorldUI item json template missing. path='{}'", String::ToString(jsonPath.wstring()));
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
            LOG_WARN("BattleWorldUI item json template parse failed. path='{}', error='{}'",
                String::ToString(jsonPath.wstring()),
                exception.what());
            return false;
        }

        const auto objectsIter = root.find("objects");
        if (objectsIter == root.end() || !objectsIter->is_array())
            return false;

        vector<BattleWorldItemJsonObject> objects{};
        BattleWorldItemJsonObjectMap objectsById{};
        objects.reserve(objectsIter->size());

        uint32 sequence = 0;
        for (const auto& node : *objectsIter)
        {
            if (!node.is_object())
                continue;

            BattleWorldItemJsonObject object = Parse_JsonObject(node, sequence++);
            objectsById.emplace(object.id, object);
            objects.push_back(std::move(object));
        }

        for (uint32 slotIndex = 0; slotIndex < kBattleWorldItemSlotCount; ++slotIndex)
        {
            const BattleWorldItemSlotDef& slotSpec = kBattleWorldItemSlots[slotIndex];
            const auto panelIter = find_if(objects.begin(), objects.end(),
                [&](const BattleWorldItemJsonObject& object)
                {
                    return object.name == slotSpec.panelName;
                });

            if (panelIter == objects.end() || !panelIter->visible)
            {
                LOG_WARN("BattleWorldUI item json panel missing. panel='{}'", slotSpec.panelName);
                return false;
            }

            BattleWorldItemCardTemplate cardTemplate{};
            cardTemplate.pixelSize = Vec2(
                max(1.f, panelIter->size.x),
                max(1.f, panelIter->size.y));

            for (const BattleWorldItemJsonObject& object : objects)
            {
                if (!Is_DescendantOf(object, panelIter->id, objectsById))
                    continue;

                if (!Is_VisibleInPanelHierarchy(object, panelIter->id, objectsById))
                    continue;

                BattleWorldItemCardDef::Layer layer{};
                bool extracted = false;

                if (object.type == "Image")
                    extracted = Try_ExtractJsonImageLayer(device, context, object, slotSpec.panelName, panelIter->id, objectsById, layer);
                else if (object.type == "Text")
                    extracted = Try_ExtractJsonTextLayer(object, slotSpec, panelIter->id, objectsById, layer);

                if (extracted)
                    cardTemplate.layers.push_back(std::move(layer));
            }

            if (cardTemplate.layers.empty())
            {
                LOG_WARN("BattleWorldUI item json panel has no extracted layers. panel='{}'", slotSpec.panelName);
                return false;
            }

            sort(cardTemplate.layers.begin(), cardTemplate.layers.end(),
                [](const BattleWorldItemCardDef::Layer& lhs, const BattleWorldItemCardDef::Layer& rhs)
                {
                    if (lhs.sortOrder != rhs.sortOrder)
                        return lhs.sortOrder < rhs.sortOrder;

                    return lhs.sequence < rhs.sequence;
                });

            outTemplates[slotIndex] = std::move(cardTemplate);
        }

        return true;
    }

    fs::path Resolve_ItemWorldPlacementJsonPath()
    {
        return (fs::path(GAME->Get_AssetRoot()) / kBattleWorldItemPlacementJsonPath).lexically_normal();
    }

    BattleWorldItemWorldPlacementTuning Read_ItemWorldPlacementTuningNode(
        const json& root,
        const BattleWorldItemWorldPlacementTuning& fallback)
    {
        BattleWorldItemWorldPlacementTuning tuning = fallback;
        tuning.screenPosition = Read_JsonVec3(root, "screenPosition", tuning.screenPosition);
        tuning.baseOffset = Read_JsonVec3(root, "baseOffset", tuning.baseOffset);
        tuning.worldSize = Read_JsonVec2(root, "worldSize", tuning.worldSize);
        tuning.visualScale = Read_JsonFloat(root, "visualScale", tuning.visualScale);
        tuning.pitchRadians =
            Read_JsonFloat(root, "pitchDegrees", tuning.pitchRadians * kBattleWorldItemRadiansToDegrees) *
            kBattleWorldItemDegreesToRadians;
        tuning.rollRadians =
            Read_JsonFloat(root, "rollDegrees", tuning.rollRadians * kBattleWorldItemRadiansToDegrees) *
            kBattleWorldItemDegreesToRadians;
        tuning.yawOffsetRadians =
            Read_JsonFloat(root, "yawOffsetDegrees", tuning.yawOffsetRadians * kBattleWorldItemRadiansToDegrees) *
            kBattleWorldItemDegreesToRadians;

        if (const auto offsetsIter = root.find("slotOffsets");
            offsetsIter != root.end() && offsetsIter->is_object())
        {
            tuning.slotOffsets[0] = Read_JsonVec3(*offsetsIter, "Q", tuning.slotOffsets[0]);
            tuning.slotOffsets[1] = Read_JsonVec3(*offsetsIter, "W", tuning.slotOffsets[1]);
            tuning.slotOffsets[2] = Read_JsonVec3(*offsetsIter, "E", tuning.slotOffsets[2]);
        }

        return tuning;
    }

    bool Try_LoadItemWorldPlacementTunings(
        const fs::path& jsonPath,
        array<BattleWorldItemWorldPlacementTuning, kBattleWorldItemPlacementHeroCount>& outTunings)
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
            LOG_WARN("BattleWorldUI item placement json parse failed. path='{}', error='{}'",
                String::ToString(jsonPath.wstring()),
                e.what());
            return false;
        }

        auto tunings = Make_DefaultItemWorldPlacementTunings();
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
                    LOG_WARN("BattleWorldUI item placement json has unknown character. character='{}'", heroName);
                    continue;
                }

                const uint32 heroIndex = Resolve_PlacementHeroIndex(heroId);
                tunings[heroIndex] = Read_ItemWorldPlacementTuningNode(heroNode, tunings[heroIndex]);
            }
        }
        else
        {
            // Backward compatibility for the first single-profile tuning file.
            const BattleWorldItemWorldPlacementTuning flatTuning =
                Read_ItemWorldPlacementTuningNode(root, BattleWorldItemWorldPlacementTuning{});
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

    json To_ItemWorldPlacementTuningJson(const BattleWorldItemWorldPlacementTuning& tuning)
    {
        json root = json::object();
        root["screenPosition"] = To_JsonVec3(tuning.screenPosition);
        root["baseOffset"] = To_JsonVec3(tuning.baseOffset);
        root["slotOffsets"] = json::object(
            {
                { "Q", To_JsonVec3(tuning.slotOffsets[0]) },
                { "W", To_JsonVec3(tuning.slotOffsets[1]) },
                { "E", To_JsonVec3(tuning.slotOffsets[2]) },
            });
        root["worldSize"] = To_JsonVec2(tuning.worldSize);
        root["visualScale"] = tuning.visualScale;
        root["pitchDegrees"] = tuning.pitchRadians * kBattleWorldItemRadiansToDegrees;
        root["rollDegrees"] = tuning.rollRadians * kBattleWorldItemRadiansToDegrees;
        root["yawOffsetDegrees"] = tuning.yawOffsetRadians * kBattleWorldItemRadiansToDegrees;
        return root;
    }

    bool Write_ItemWorldPlacementTuningsToFile(
        const array<BattleWorldItemWorldPlacementTuning, kBattleWorldItemPlacementHeroCount>& tunings,
        fs::file_time_type* outWriteTime)
    {
        const fs::path jsonPath = Resolve_ItemWorldPlacementJsonPath();

        json root = json::object();
        root["characters"] = json::object();
        for (uint32 heroIndex = 0; heroIndex < kBattleWorldItemPlacementHeroCount; ++heroIndex)
        {
            const HeroId heroId = static_cast<HeroId>(heroIndex);
            root["characters"][To_PlacementHeroJsonName(heroId)] =
                To_ItemWorldPlacementTuningJson(tunings[heroIndex]);
        }

        error_code errorCode{};
        fs::create_directories(jsonPath.parent_path(), errorCode);
        if (errorCode)
        {
            LOG_WARN("BattleWorldUI item placement json directory create failed. path='{}'",
                String::ToString(jsonPath.parent_path().wstring()));
            return false;
        }

        try
        {
            ofstream file(jsonPath);
            if (!file.is_open())
            {
                LOG_WARN("BattleWorldUI item placement json open failed. path='{}'",
                    String::ToString(jsonPath.wstring()));
                return false;
            }

            file << root.dump(2) << '\n';
        }
        catch (const exception& e)
        {
            LOG_WARN("BattleWorldUI item placement json write failed. path='{}', error='{}'",
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

        LOG_INFO("BattleWorldUI item placement json saved. path='{}'",
            String::ToString(jsonPath.wstring()));
        return true;
    }

    RuntimeWorldUIPlacementTuning To_EditorWorldPlacementTuning(
        const BattleWorldItemWorldPlacementTuning& tuning)
    {
        RuntimeWorldUIPlacementTuning editorTuning{};
        editorTuning.screenPosition = tuning.screenPosition;
        editorTuning.baseOffset = tuning.baseOffset;
        editorTuning.qOffset = tuning.slotOffsets[0];
        editorTuning.wOffset = tuning.slotOffsets[1];
        editorTuning.eOffset = tuning.slotOffsets[2];
        editorTuning.worldSize = tuning.worldSize;
        editorTuning.visualScale = tuning.visualScale;
        editorTuning.pitchDegrees = tuning.pitchRadians * kBattleWorldItemRadiansToDegrees;
        editorTuning.rollDegrees = tuning.rollRadians * kBattleWorldItemRadiansToDegrees;
        editorTuning.yawOffsetDegrees = tuning.yawOffsetRadians * kBattleWorldItemRadiansToDegrees;
        return editorTuning;
    }

    BattleWorldItemWorldPlacementTuning From_EditorWorldPlacementTuning(
        const RuntimeWorldUIPlacementTuning& editorTuning)
    {
        BattleWorldItemWorldPlacementTuning tuning{};
        tuning.screenPosition = editorTuning.screenPosition;
        tuning.baseOffset = editorTuning.baseOffset;
        tuning.slotOffsets[0] = editorTuning.qOffset;
        tuning.slotOffsets[1] = editorTuning.wOffset;
        tuning.slotOffsets[2] = editorTuning.eOffset;
        tuning.worldSize = Vec2(
            max(0.01f, editorTuning.worldSize.x),
            max(0.01f, editorTuning.worldSize.y));
        tuning.visualScale = max(0.01f, editorTuning.visualScale);
        tuning.pitchRadians = editorTuning.pitchDegrees * kBattleWorldItemDegreesToRadians;
        tuning.rollRadians = editorTuning.rollDegrees * kBattleWorldItemDegreesToRadians;
        tuning.yawOffsetRadians = editorTuning.yawOffsetDegrees * kBattleWorldItemDegreesToRadians;
        return tuning;
    }

    void Reload_ItemWorldPlacementTuningIfNeeded(
        array<BattleWorldItemWorldPlacementTuning, kBattleWorldItemPlacementHeroCount>& tunings,
        fs::file_time_type& knownWriteTime,
        bool& hasKnownFile)
    {
        const fs::path jsonPath = Resolve_ItemWorldPlacementJsonPath();
        if (!fs::exists(jsonPath))
        {
            if (hasKnownFile)
            {
                tunings = Make_DefaultItemWorldPlacementTunings();
                knownWriteTime = {};
                hasKnownFile = false;
                LOG_INFO("BattleWorldUI item placement json removed. using defaults.");
            }
            return;
        }

        error_code errorCode{};
        const fs::file_time_type writeTime = fs::last_write_time(jsonPath, errorCode);
        if (errorCode)
            return;

        if (hasKnownFile && writeTime == knownWriteTime)
            return;

        array<BattleWorldItemWorldPlacementTuning, kBattleWorldItemPlacementHeroCount> loadedTunings{};
        knownWriteTime = writeTime;
        hasKnownFile = true;

        if (!Try_LoadItemWorldPlacementTunings(jsonPath, loadedTunings))
            return;

        tunings = loadedTunings;
        LOG_INFO("BattleWorldUI item placement json reloaded. path='{}'",
            String::ToString(jsonPath.wstring()));
    }

    BattleWorldItemCardPresentation Resolve_ItemPresentation(
        uint32 slotIndex,
        const BattleWorldItemCardDef& def,
        const BattleWorldItemWorldPlacementTuning& placementTuning,
        BattleTurnState state,
        BattleItemType selectedItemType)
    {
        UNREFERENCED_PARAMETER(selectedItemType);

        BattleWorldItemCardPresentation presentation{};

        if (!Should_ShowBattleWorldItemCards(state))
            return presentation;

        if (slotIndex >= kBattleWorldItemSlotCount)
            return presentation;

        presentation.visible = true;
        presentation.dimmed = def.content.count <= 0;
        presentation.screenPosition = placementTuning.screenPosition;
        presentation.worldOffset = placementTuning.baseOffset + placementTuning.slotOffsets[slotIndex];
        presentation.worldSize = placementTuning.worldSize;
        presentation.visualScale = placementTuning.visualScale;
        presentation.pitchRadians = placementTuning.pitchRadians;
        presentation.rollRadians = placementTuning.rollRadians;
        presentation.yawOffsetRadians = placementTuning.yawOffsetRadians;
        return presentation;
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
        // VIBuffer_Rect's front face is the local -Z side, so aim -Z toward the camera.
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

    class IWorldUIItemCardSurface
    {
    public:
        virtual ~IWorldUIItemCardSurface() = default;

        virtual HRESULT Compose(
            const BattleWorldItemCardDef& def,
            bool selected,
            bool dimmed) = 0;
        virtual HRESULT Bind_ShaderResource(ShaderCom* shader, const char* name) const = 0;
    };

    class RenderTargetComposedBattleWorldItemCardSurface final : public IWorldUIItemCardSurface
    {
    public:
        RenderTargetComposedBattleWorldItemCardSurface(
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
                L"BattleWorldItemCardSurface_" + to_wstring(slotIndex),
                _device,
                _context,
                static_cast<uint32>(max(1.f, roundf(_pixelSize.x * kBattleWorldItemCompositionRenderScale))),
                static_cast<uint32>(max(1.f, roundf(_pixelSize.y * kBattleWorldItemCompositionRenderScale))),
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
            const BattleWorldItemCardDef& def,
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
                if (layer.kind == BattleWorldItemCardLayerKind::Image)
                    CHECK_FAILED(Draw_CompositionImage(layer.image, selected, dimmed), E_FAIL);
            }

            for (const auto& layer : def.layers)
            {
                if (layer.kind == BattleWorldItemCardLayerKind::Text)
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

        static Unique<IWorldUIItemCardSurface> Create(
            uint32 slotIndex,
            const ComPtr<Device>& device,
            const ComPtr<Context>& context,
            const Shared<ShaderCom>& shader,
            const Shared<VIBuffer_Rect>& viBuffer,
            const Vec2& pixelSize)
        {
            auto surface = make_unique<RenderTargetComposedBattleWorldItemCardSurface>(
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
        HRESULT Draw_CompositionImage(const BattleWorldItemCardImageDef& image, bool selected, bool dimmed)
        {
            CHECK_NULL(image.texture, E_FAIL);
            CHECK_NULL(_shader, E_FAIL);
            CHECK_NULL(_viBuffer, E_FAIL);

            const auto& srvs = image.texture->Get_SRVs();
            if (srvs.empty() || srvs[0] == nullptr)
                return E_FAIL;

            uint32 passIndex = 0u;
            if (image.imageRenderMode == BattleWorldItemImageRenderMode::PatternOverlay)
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
                passIndex = (image.maskTextureMode == BattleWorldItemMaskTextureMode::BlackWhite) ? 5u : 1u;
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
                image.imageRenderMode == BattleWorldItemImageRenderMode::PatternOverlay ? image.patternUvOffset : defaultPatternOffset;
            const Vec2 patternUvScale =
                image.imageRenderMode == BattleWorldItemImageRenderMode::PatternOverlay ? image.patternUvScale : defaultPatternScale;

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

        void Draw_CompositionText(const BattleWorldItemCardTextDef& text, bool selected, bool dimmed)
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
            if (text.align == BattleWorldItemTextAlign::Center)
                offsetX = (boxWidth - textWidth) * 0.5f;
            else if (text.align == BattleWorldItemTextAlign::Right)
                offsetX = boxWidth - textWidth;

            const Vec2 alignedTopLeft(boxTopLeft.x + offsetX, boxTopLeft.y);
            const Vec2 pivot(
                (text.position.x - alignedTopLeft.x) / textScale,
                (text.position.y - alignedTopLeft.y) / textScale);

            const Vec4 drawColor = Resolve_TextColor(text.color, selected, dimmed);

            GAME->Draw_Font(
                text.fontTag,
                text.text.c_str(),
                text.position * kBattleWorldItemCompositionRenderScale,
                XMVectorSet(drawColor.x, drawColor.y, drawColor.z, drawColor.w),
                text.rotationRadians,
                pivot,
                textScale * kBattleWorldItemCompositionRenderScale);
        }

        ComPtr<Device> _device{};
        ComPtr<Context> _context{};
        Shared<ShaderCom> _shader{};
        Shared<VIBuffer_Rect> _viBuffer{};
        Shared<RenderTarget> _renderTarget{};
        ComPtr<ID3D11BlendState> _compositionBlendState{};
        Vec2 _pixelSize = Vec2(
            static_cast<float>(kBattleWorldItemFallbackPixelWidth),
            static_cast<float>(kBattleWorldItemFallbackPixelHeight));
    };

}

struct BattleWorldUI_ItemCards::Impl
{
    Shared<ShaderCom> shader{};
    Shared<VIBuffer_Rect> viBuffer{};
    array<BattleWorldItemCardTemplate, kBattleWorldItemSlotCount> templates{};
    array<BattleWorldItemCardInstance, kBattleWorldItemSlotCount> cards{};
    array<BattleWorldItemWorldPlacementTuning, kBattleWorldItemPlacementHeroCount> placementTunings =
        Make_DefaultItemWorldPlacementTunings();
    fs::file_time_type placementTuningWriteTime{};
    bool hasPlacementTuningFile = false;
    HeroId activePlacementHeroId = HeroId::END;
    HeroId editorPlacementHeroId = HeroId::END;
};

BattleWorldUI_ItemCards::BattleWorldUI_ItemCards(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

BattleWorldUI_ItemCards::~BattleWorldUI_ItemCards() = default;

HRESULT BattleWorldUI_ItemCards::Initialize(void* arg)
{
    CHECK_FAILED(GameObject::Initialize(arg), E_FAIL);

    _impl = make_unique<Impl>();
    CHECK_NULL(_impl, E_FAIL);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Shader_UI"), _impl->shader), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("VIBuffer_Rect"), _impl->viBuffer), E_FAIL);

    Shared<Texture> fallbackCardTexture = BattleWorldUI_JsonTextureResolver::Load_TextureFromUIImage(
        _device,
        _context,
        kBattleWorldItemPanelJsonPath,
        "Image_Healing_Tint_Frame",
        kBattleWorldItemCardTextureGuid);
    CHECK_NULL(fallbackCardTexture, E_FAIL);

    Shared<Texture> fallbackKeyFrameTexture = BattleWorldUI_JsonTextureResolver::Load_TextureFromUIImage(
        _device,
        _context,
        kBattleWorldItemPanelJsonPath,
        "Image_KeyFrame_Q",
        kBattleWorldKeyFrameTextureGuid);
    CHECK_NULL(fallbackKeyFrameTexture, E_FAIL);

    const BattleWorldItemCardTemplate fallbackTemplate =
        Build_FallbackItemCardTemplate(fallbackCardTexture, fallbackKeyFrameTexture);
    for (auto& cardTemplate : _impl->templates)
        cardTemplate = fallbackTemplate;

    array<BattleWorldItemCardTemplate, kBattleWorldItemSlotCount> jsonTemplates{};
    if (Try_LoadJsonItemCardTemplates(_device, _context, jsonTemplates))
        _impl->templates = std::move(jsonTemplates);

    Reload_ItemWorldPlacementTuningIfNeeded(
        _impl->placementTunings,
        _impl->placementTuningWriteTime,
        _impl->hasPlacementTuningFile);

    for (uint32 slotIndex = 0; slotIndex < kBattleWorldItemSlotCount; ++slotIndex)
    {
        _impl->cards[slotIndex].surface = RenderTargetComposedBattleWorldItemCardSurface::Create(
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

void BattleWorldUI_ItemCards::Show_ForBattleState(
    const Vec3& anchorWorldPos,
    BattleTurnState currentState,
    BattleItemType selectedItemType,
    HeroId actorHeroId)
{
    if (_impl == nullptr || !Should_ShowBattleWorldItemCards(currentState))
    {
        Hide();
        return;
    }

    Reload_ItemWorldPlacementTuningIfNeeded(
        _impl->placementTunings,
        _impl->placementTuningWriteTime,
        _impl->hasPlacementTuningFile);

    const HeroId placementHeroId = Normalize_PlacementHeroId(actorHeroId);
    const uint32 placementHeroIndex = Resolve_PlacementHeroIndex(placementHeroId);
    _impl->activePlacementHeroId = placementHeroId;
    if (_impl->editorPlacementHeroId == HeroId::END)
        _impl->editorPlacementHeroId = placementHeroId;

    bool anyVisible = false;
    for (uint32 slotIndex = 0; slotIndex < kBattleWorldItemSlotCount; ++slotIndex)
    {
        auto& card = _impl->cards[slotIndex];
        card.visible = false;

        const BattleWorldItemSlotDef& slot = kBattleWorldItemSlots[slotIndex];
        const Item_Data* item = CLIENT->Get_Item(slot.itemType);
        if (item == nullptr || !item->IsValid())
        {
            card.hasContent = false;
            continue;
        }

        const BattleWorldItemCardContent content = Build_ItemCardContent(slot, *item);
        if (!card.hasContent || !Is_SameContent(card.def.content, content))
        {
            card.def = Build_ItemCardDef(content, _impl->templates[slotIndex]);
            card.hasContent = true;
            card.dirty = true;
        }

        const BattleWorldItemCardPresentation presentation =
            Resolve_ItemPresentation(
                slotIndex,
                card.def,
                _impl->placementTunings[placementHeroIndex],
                currentState,
                selectedItemType);

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

        const Vec3 position =
            anchorWorldPos +
            Resolve_CameraScreenPositionDelta(presentation.screenPosition) +
            presentation.worldOffset;
        const Vec2 aspectWorldSize = Resolve_AspectWorldSize(presentation.worldSize, card.def.pixelSize);
        card.worldMatrix =
            Matrix::CreateScale(
                aspectWorldSize.x * presentation.visualScale,
                aspectWorldSize.y * presentation.visualScale,
                1.f) *
            Matrix::CreateRotationZ(presentation.rollRadians) *
            Matrix::CreateRotationX(presentation.pitchRadians) *
            Matrix::CreateRotationY(presentation.yawOffsetRadians) *
            Resolve_CameraFacingYawRotation(position) *
            Matrix::CreateTranslation(position.x, position.y, position.z);
        card.visible = true;
        anyVisible = true;
    }

    Set_Visible(anyVisible);
}

void BattleWorldUI_ItemCards::Hide()
{
    Set_Visible(false);

    if (_impl == nullptr)
        return;

    for (auto& card : _impl->cards)
        card.visible = false;
}

HeroId BattleWorldUI_ItemCards::Get_EditorWorldPlacementHeroId() const
{
    if (_impl == nullptr)
        return HeroId::Gustave;

    if (_impl->editorPlacementHeroId != HeroId::END)
        return Normalize_PlacementHeroId(_impl->editorPlacementHeroId);

    if (_impl->activePlacementHeroId != HeroId::END)
        return Normalize_PlacementHeroId(_impl->activePlacementHeroId);

    return HeroId::Gustave;
}

void BattleWorldUI_ItemCards::Set_EditorWorldPlacementHeroId(HeroId heroId)
{
    if (_impl == nullptr)
        return;

    _impl->editorPlacementHeroId = Normalize_PlacementHeroId(heroId);
}

RuntimeWorldUIPlacementTuning BattleWorldUI_ItemCards::Get_EditorWorldPlacementTuning() const
{
    if (_impl == nullptr)
        return To_EditorWorldPlacementTuning(Make_DefaultItemWorldPlacementTuning(HeroId::Gustave));

    const uint32 heroIndex = Resolve_PlacementHeroIndex(Get_EditorWorldPlacementHeroId());
    return To_EditorWorldPlacementTuning(_impl->placementTunings[heroIndex]);
}

void BattleWorldUI_ItemCards::Set_EditorWorldPlacementTuning(
    const RuntimeWorldUIPlacementTuning& tuning,
    bool saveImmediately)
{
    if (_impl == nullptr)
        return;

    const uint32 heroIndex = Resolve_PlacementHeroIndex(Get_EditorWorldPlacementHeroId());
    _impl->placementTunings[heroIndex] = From_EditorWorldPlacementTuning(tuning);

    if (!saveImmediately)
        return;

    if (Write_ItemWorldPlacementTuningsToFile(
        _impl->placementTunings,
        &_impl->placementTuningWriteTime))
    {
        _impl->hasPlacementTuningFile = true;
    }
}

void BattleWorldUI_ItemCards::Reload_EditorWorldPlacementTuning()
{
    if (_impl == nullptr)
        return;

    const fs::path jsonPath = Resolve_ItemWorldPlacementJsonPath();
    if (!fs::exists(jsonPath))
    {
        _impl->placementTunings = Make_DefaultItemWorldPlacementTunings();
        _impl->placementTuningWriteTime = {};
        _impl->hasPlacementTuningFile = false;
        LOG_INFO("BattleWorldUI item placement json reload requested, but file is missing. using defaults.");
        return;
    }

    array<BattleWorldItemWorldPlacementTuning, kBattleWorldItemPlacementHeroCount> loadedTunings{};
    if (!Try_LoadItemWorldPlacementTunings(jsonPath, loadedTunings))
        return;

    error_code errorCode{};
    const fs::file_time_type writeTime = fs::last_write_time(jsonPath, errorCode);
    if (!errorCode)
        _impl->placementTuningWriteTime = writeTime;

    _impl->placementTunings = loadedTunings;
    _impl->hasPlacementTuningFile = true;
}

void BattleWorldUI_ItemCards::Reset_EditorWorldPlacementTuning()
{
    if (_impl == nullptr)
        return;

    const HeroId heroId = Get_EditorWorldPlacementHeroId();
    _impl->placementTunings[Resolve_PlacementHeroIndex(heroId)] =
        Make_DefaultItemWorldPlacementTuning(heroId);
    if (Write_ItemWorldPlacementTuningsToFile(
        _impl->placementTunings,
        &_impl->placementTuningWriteTime))
    {
        _impl->hasPlacementTuningFile = true;
    }
}

void BattleWorldUI_ItemCards::Late_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!Is_Visible())
        return;

    GAME->Add_RenderGroup(RenderGroup::UIEffect, GetSharedPtr<GameObject>());
}

HRESULT BattleWorldUI_ItemCards::Render()
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

Shared<GameObject> BattleWorldUI_ItemCards::Clone(void* arg)
{
    UNREFERENCED_PARAMETER(arg);
    return nullptr;
}

void BattleWorldUI_ItemCards::Free()
{
    __super::Free();

    _impl.reset();
}

Shared<BattleWorldUI_ItemCards> BattleWorldUI_ItemCards::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context)
{
    auto instance = make_shared<BattleWorldUI_ItemCards>(device, context);
    if (FAILED(instance->Initialize(nullptr)))
    {
        MSG_BOX("Failed to create BattleWorldUI_ItemCards");
        return nullptr;
    }

    return instance;
}

NS_END
