#include "BattleWorldUI_MaterialVfxBackground.h"

#include "BattleWorldUI_JsonTextureResolver.h"
#include "BattleWorldUI_LevelPlacement.h"
#include "BattleWorldUI_PlacementUtils.h"
#include "BattleWorldUI_SurfaceAnim.h"
#include "Camera.h"
#include "Debug_Profile.h"
#include "RenderTarget.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "TransformCom.h"
#include "VIBuffer_Rect.h"

NS_BEGIN(Client)

namespace
{
    constexpr wchar_t kBattleWorldVfxBackgroundJsonPath[] =
        L"UI/UI_Json/BattleSystem/UI_BattleSystem_Player_BattleCommand_Background.json";
    constexpr wchar_t kBattleWorldVfxBackgroundPlacementJsonPath[] =
        L"UI/UI_Json/BattleSystem/UI_BattleSystem_Player_BattleCommand_Background_WorldPlacement.json";
    constexpr uint32 kBattleWorldVfxBackgroundPlacementHeroCount = static_cast<uint32>(HeroId::END);
    constexpr uint32 kBattleWorldVfxBackgroundStateCount = 4;
    constexpr float kBattleWorldVfxBackgroundCompositionRenderScale = 2.f;
    constexpr float kBattleWorldVfxBackgroundDegreesToRadians = 0.017453292519943295f;
    constexpr float kBattleWorldVfxBackgroundRadiansToDegrees = 57.29577951308232f;
    constexpr float kBattleWorldVfxBackgroundItemScreenXOffset = -0.55f;
    constexpr float kBattleWorldVfxBackgroundSkillScreenXOffset = -0.35f;
    constexpr bool kBattleWorldVfxBackgroundIgnoreWicSrgb = true;
    const Vec2 kBattleWorldVfxBackgroundDefaultWorldSize = Vec2(1.f, 1.f);
    const Vec4 kBattleWorldVfxBackgroundWorldColor = Vec4(1.24f, 1.14f, 0.93f, 1.f);

    enum class BattleWorldVfxBackgroundState : uint32
    {
        Command,
        Item,
        Skill,
        Gradient,
    };

    struct BattleWorldVfxBackgroundStateDef
    {
        BattleWorldVfxBackgroundState state = BattleWorldVfxBackgroundState::Command;
        BattleTurnState turnState = BattleTurnState::None;
        const char* jsonName = "";
        const char* editorLabel = "";
    };

    const BattleWorldVfxBackgroundStateDef kBattleWorldVfxBackgroundStateDefs[kBattleWorldVfxBackgroundStateCount] =
    {
        { BattleWorldVfxBackgroundState::Command, BattleTurnState::ChooseAction, "Command", "Command" },
        { BattleWorldVfxBackgroundState::Item, BattleTurnState::ChooseItem, "Item", "Item" },
        { BattleWorldVfxBackgroundState::Skill, BattleTurnState::ChooseSkill, "Skill", "Skill" },
        { BattleWorldVfxBackgroundState::Gradient, BattleTurnState::ChooseGradient, "Gradient", "Gradient" },
    };

    struct BattleWorldVfxBackgroundDef
    {
        Shared<Texture> baseTexture{};
        Shared<Texture> maskTexture{};
        Shared<Texture> noiseTexture{};
        Vec2 pixelSize = Vec2(500.f, 500.f);
        Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);
        Vec4 materialColor = Vec4(1.f, 0.12f, 0.95f, 1.f);
        Vec4 materialCoreColor = Vec4(1.f, 0.92f, 1.f, 1.f);
        Vec4 materialFlowColor = Vec4(1.f, 1.f, 1.f, 0.f);
        Vec2 materialUvScroll = Vec2(0.08f, -0.18f);
        uint32 materialMode = 0u;
        float materialGlow = 4.f;
        float materialAlpha = 1.f;
        float materialCoreStrength = 2.2f;
        float materialCoreWidth = 0.13f;
        float materialCoreSoftness = 0.05f;
        float materialNoiseStrength = 0.35f;
        float materialFlowSpeed = 1.f;
        float materialPreserveColor = 0.f;
        float materialEdgeWidth = 0.18f;
        float materialEdgeSoftness = 0.08f;
        float materialErosion = 0.18f;
        float materialFlicker = 0.22f;
        float materialDistortion = 0.015f;
        float materialEdgeWobbleSpeed = 1.f;
        bool valid = false;
    };

    struct BattleWorldVfxBackgroundPlacementTuning
    {
        Vec3 screenPosition = Vec3::Zero;
        Vec3 baseOffset = Vec3::Zero;
        Vec2 worldSize = kBattleWorldVfxBackgroundDefaultWorldSize;
        float visualScale = 1.f;
        float pitchRadians = 0.f;
        float rollRadians = 0.f;
        float yawOffsetRadians = 0.f;
    };

    using BattleWorldVfxBackgroundStatePlacements =
        array<BattleWorldVfxBackgroundPlacementTuning, kBattleWorldVfxBackgroundStateCount>;
    using BattleWorldVfxBackgroundPlacementTunings =
        array<BattleWorldVfxBackgroundStatePlacements, kBattleWorldVfxBackgroundPlacementHeroCount>;

    HeroId Normalize_PlacementHeroId(HeroId heroId)
    {
        const uint32 heroIndex = static_cast<uint32>(heroId);
        if (heroIndex < kBattleWorldVfxBackgroundPlacementHeroCount)
            return heroId;

        return HeroId::Gustave;
    }

    uint32 Resolve_PlacementHeroIndex(HeroId heroId)
    {
        return static_cast<uint32>(Normalize_PlacementHeroId(heroId));
    }

    uint32 Resolve_BackgroundStateIndex(BattleWorldVfxBackgroundState state)
    {
        return static_cast<uint32>(state);
    }

    uint32 Normalize_BackgroundStateIndex(uint32 stateIndex)
    {
        return stateIndex < kBattleWorldVfxBackgroundStateCount
            ? stateIndex
            : Resolve_BackgroundStateIndex(BattleWorldVfxBackgroundState::Command);
    }

    bool Try_ResolveBackgroundState(BattleTurnState turnState, BattleWorldVfxBackgroundState& outState)
    {
        for (const auto& stateDef : kBattleWorldVfxBackgroundStateDefs)
        {
            if (stateDef.turnState != turnState)
                continue;

            outState = stateDef.state;
            return true;
        }

        return false;
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

    bool Try_ParseBackgroundStateJsonName(const string& stateName, BattleWorldVfxBackgroundState& outState)
    {
        const string lowerName = String::ToLowerCopy(stateName);
        for (const auto& stateDef : kBattleWorldVfxBackgroundStateDefs)
        {
            const string lowerJsonName = String::ToLowerCopy(stateDef.jsonName);
            if (lowerName == lowerJsonName)
            {
                outState = stateDef.state;
                return true;
            }
        }

        return false;
    }

    const char* To_BackgroundStateJsonName(uint32 stateIndex)
    {
        return kBattleWorldVfxBackgroundStateDefs[Normalize_BackgroundStateIndex(stateIndex)].jsonName;
    }

    const char* To_BackgroundStateEditorLabel(uint32 stateIndex)
    {
        return kBattleWorldVfxBackgroundStateDefs[Normalize_BackgroundStateIndex(stateIndex)].editorLabel;
    }

    BattleWorldVfxBackgroundPlacementTuning Make_DefaultBackgroundPlacementTuning(
        HeroId heroId,
        BattleWorldVfxBackgroundState state)
    {
        BattleWorldVfxBackgroundPlacementTuning tuning{};
        tuning.worldSize = kBattleWorldVfxBackgroundDefaultWorldSize;

        switch (Normalize_PlacementHeroId(heroId))
        {
        case HeroId::Gustave:
            tuning.screenPosition = Vec3(0.03f, -0.59f, 0.f);
            tuning.baseOffset = Vec3(-0.28f, 0.f, 0.f);
            tuning.visualScale = 1.5f;
            break;
        case HeroId::Lune:
            tuning.screenPosition = Vec3(-0.12f, -0.42f, 0.f);
            tuning.baseOffset = Vec3(0.f, 0.f, -0.25f);
            tuning.visualScale = 1.5f;
            break;
        case HeroId::Maelle:
            tuning.screenPosition = Vec3(-0.12f, -0.44f, 0.f);
            tuning.baseOffset = Vec3(0.f, 0.f, -0.25f);
            tuning.visualScale = 1.5f;
            break;
        case HeroId::END:
        default:
            break;
        }

        switch (state)
        {
        case BattleWorldVfxBackgroundState::Item:
            tuning.screenPosition.x += kBattleWorldVfxBackgroundItemScreenXOffset;
            break;
        case BattleWorldVfxBackgroundState::Skill:
        case BattleWorldVfxBackgroundState::Gradient:
            tuning.screenPosition.x += kBattleWorldVfxBackgroundSkillScreenXOffset;
            break;
        case BattleWorldVfxBackgroundState::Command:
        default:
            break;
        }

        return tuning;
    }

    BattleWorldVfxBackgroundPlacementTunings Make_DefaultBackgroundPlacementTunings()
    {
        BattleWorldVfxBackgroundPlacementTunings tunings{};
        for (uint32 heroIndex = 0; heroIndex < kBattleWorldVfxBackgroundPlacementHeroCount; ++heroIndex)
        {
            for (uint32 stateIndex = 0; stateIndex < kBattleWorldVfxBackgroundStateCount; ++stateIndex)
            {
                tunings[heroIndex][stateIndex] = Make_DefaultBackgroundPlacementTuning(
                    static_cast<HeroId>(heroIndex),
                    kBattleWorldVfxBackgroundStateDefs[stateIndex].state);
            }
        }

        return tunings;
    }

    using BattleWorldVfxBackgroundLevelPlacementTunings =
        array<BattleWorldVfxBackgroundPlacementTunings, BattleWorldUILevelPlacement::kLevelCount>;

    BattleWorldVfxBackgroundLevelPlacementTunings Make_DefaultBackgroundLevelPlacementTunings()
    {
        BattleWorldVfxBackgroundLevelPlacementTunings levelTunings{};
        const BattleWorldVfxBackgroundPlacementTunings defaultTunings =
            Make_DefaultBackgroundPlacementTunings();
        for (auto& levelTuning : levelTunings)
            levelTuning = defaultTunings;

        return levelTunings;
    }

    string Read_JsonString(const json& node, const char* key, const string& fallback = {})
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_string())
            return fallback;

        return iter->get<string>();
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

    bool Has_TextureRef(const string& textureGuid, const string& texturePath)
    {
        return !textureGuid.empty() || !texturePath.empty();
    }

    uint32 Parse_MaterialVfxMode(const string& modeName)
    {
        const string mode = String::ToLowerCopy(modeName);
        if (mode == "brushflow" || mode == "brush_flow" || mode == "brush flow")
            return 1u;

        return 0u;
    }

    bool Try_LoadBackgroundDef(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        BattleWorldVfxBackgroundDef& outDef)
    {
        const fs::path jsonPath =
            (fs::path(GAME->Get_AssetRoot()) / kBattleWorldVfxBackgroundJsonPath).lexically_normal();
        if (!fs::exists(jsonPath))
        {
            LOG_WARN("BattleWorldUI material vfx background json missing. path='{}'",
                String::ToString(jsonPath.wstring()));
            return false;
        }

        json root{};
        try
        {
            ifstream file(jsonPath);
            file >> root;
        }
        catch (const exception& e)
        {
            LOG_WARN("BattleWorldUI material vfx background json parse failed. path='{}', error='{}'",
                String::ToString(jsonPath.wstring()),
                e.what());
            return false;
        }

        const auto objectsIter = root.find("objects");
        if (objectsIter == root.end() || !objectsIter->is_array())
            return false;

        const json* imageNode = nullptr;
        for (const json& objectNode : *objectsIter)
        {
            if (!objectNode.is_object())
                continue;

            if (Read_JsonString(objectNode, "type") != "Image")
                continue;

            if (!Read_JsonBool(objectNode, "visible", true))
                continue;

            if (String::ToLowerCopy(Read_JsonString(objectNode, "imageRenderMode")) != "materialvfx")
                continue;

            imageNode = &objectNode;
            break;
        }

        if (imageNode == nullptr)
        {
            LOG_WARN("BattleWorldUI material vfx background json has no visible MaterialVfx image.");
            return false;
        }

        BattleWorldVfxBackgroundDef def{};
        const string textureGuid = Read_JsonString(*imageNode, "textureGuid");
        const string texturePath = Read_JsonString(*imageNode, "texturePath");
        def.baseTexture = BattleWorldUI_JsonTextureResolver::Load_TextureFromUITextureRef(
            device,
            context,
            textureGuid,
            texturePath,
            Read_JsonString(*imageNode, "name", "MaterialVfxBackground"),
            kBattleWorldVfxBackgroundIgnoreWicSrgb);
        if (!def.baseTexture)
            return false;

        const string maskTextureGuid = Read_JsonString(*imageNode, "maskTextureGuid");
        const string maskTexturePath = Read_JsonString(*imageNode, "maskTexturePath");
        if (Has_TextureRef(maskTextureGuid, maskTexturePath))
        {
            def.maskTexture = BattleWorldUI_JsonTextureResolver::Load_TextureFromUITextureRef(
                device,
                context,
                maskTextureGuid,
                maskTexturePath,
                "MaterialVfxBackgroundMask",
                kBattleWorldVfxBackgroundIgnoreWicSrgb);
        }

        def.pixelSize = Read_JsonVec2(*imageNode, "size", def.pixelSize);
        const Vec2 imageScale = Read_JsonVec2(*imageNode, "scale", Vec2(1.f, 1.f));
        def.pixelSize.x = max(1.f, def.pixelSize.x * max(0.01f, fabsf(imageScale.x)));
        def.pixelSize.y = max(1.f, def.pixelSize.y * max(0.01f, fabsf(imageScale.y)));
        def.color = Read_JsonColor(*imageNode, "color", def.color);
        def.color.w *= min(max(Read_JsonFloat(*imageNode, "alphaMultiplier", 1.f), 0.f), 1.f);

        if (const auto materialIter = imageNode->find("materialVfx");
            materialIter != imageNode->end() && materialIter->is_object())
        {
            const json& materialNode = *materialIter;
            const string noiseTextureGuid = Read_JsonString(materialNode, "noiseTextureGuid");
            const string noiseTexturePath = Read_JsonString(materialNode, "noiseTexturePath");
            if (Has_TextureRef(noiseTextureGuid, noiseTexturePath))
            {
                def.noiseTexture = BattleWorldUI_JsonTextureResolver::Load_TextureFromUITextureRef(
                    device,
                    context,
                    noiseTextureGuid,
                    noiseTexturePath,
                    "MaterialVfxBackgroundNoise",
                    kBattleWorldVfxBackgroundIgnoreWicSrgb);
            }

            def.materialMode = Parse_MaterialVfxMode(Read_JsonString(materialNode, "mode", "Energy"));
            def.materialColor = Read_JsonColor(materialNode, "color", def.materialColor);
            def.materialCoreColor = Read_JsonColor(materialNode, "coreColor", def.materialCoreColor);
            def.materialFlowColor = Read_JsonColor(materialNode, "flowColor", def.materialFlowColor);
            def.materialGlow = max(0.f, Read_JsonFloat(materialNode, "glow", def.materialGlow));
            def.materialAlpha = max(0.f, Read_JsonFloat(materialNode, "alpha", def.materialAlpha));
            def.materialCoreStrength = max(0.f, Read_JsonFloat(materialNode, "coreStrength", def.materialCoreStrength));
            def.materialCoreWidth = max(0.001f, Read_JsonFloat(materialNode, "coreWidth", def.materialCoreWidth));
            def.materialCoreSoftness = max(0.001f, Read_JsonFloat(materialNode, "coreSoftness", def.materialCoreSoftness));
            def.materialNoiseStrength = min(max(Read_JsonFloat(materialNode, "noiseStrength", def.materialNoiseStrength), 0.f), 1.f);
            def.materialUvScroll = Read_JsonVec2(materialNode, "uvScroll", def.materialUvScroll);
            def.materialFlowSpeed = min(max(Read_JsonFloat(materialNode, "flowSpeed", def.materialFlowSpeed), 0.f), 4.f);
            def.materialPreserveColor = min(max(Read_JsonFloat(materialNode, "preserveColor", def.materialPreserveColor), 0.f), 1.f);
            def.materialEdgeWidth = max(0.001f, Read_JsonFloat(materialNode, "edgeWidth", def.materialEdgeWidth));
            def.materialEdgeSoftness = max(0.001f, Read_JsonFloat(materialNode, "edgeSoftness", def.materialEdgeSoftness));
            def.materialErosion = min(max(Read_JsonFloat(materialNode, "erosion", def.materialErosion), 0.f), 1.f);
            def.materialFlicker = min(max(Read_JsonFloat(materialNode, "flicker", def.materialFlicker), 0.f), 1.f);
            def.materialDistortion = min(max(Read_JsonFloat(materialNode, "distortion", def.materialDistortion), 0.f), 0.1f);
            def.materialEdgeWobbleSpeed = min(max(Read_JsonFloat(materialNode, "edgeWobbleSpeed", def.materialEdgeWobbleSpeed), 0.f), 4.f);
        }

        def.valid = true;
        outDef = std::move(def);
        return true;
    }

    fs::path Resolve_BackgroundPlacementJsonPath()
    {
        return (fs::path(GAME->Get_AssetRoot()) / kBattleWorldVfxBackgroundPlacementJsonPath).lexically_normal();
    }

    BattleWorldVfxBackgroundPlacementTuning Read_BackgroundPlacementTuningNode(
        const json& root,
        const BattleWorldVfxBackgroundPlacementTuning& fallback)
    {
        BattleWorldVfxBackgroundPlacementTuning tuning = fallback;
        tuning.screenPosition = Read_JsonVec3(root, "screenPosition", tuning.screenPosition);
        tuning.baseOffset = Read_JsonVec3(root, "baseOffset", tuning.baseOffset);
        tuning.worldSize = Read_JsonVec2(root, "worldSize", tuning.worldSize);
        tuning.visualScale = Read_JsonFloat(root, "visualScale", tuning.visualScale);
        tuning.pitchRadians = 0.f;
        tuning.rollRadians =
            Read_JsonFloat(root, "rollDegrees", tuning.rollRadians * kBattleWorldVfxBackgroundRadiansToDegrees) *
            kBattleWorldVfxBackgroundDegreesToRadians;
        tuning.yawOffsetRadians = 0.f;
        return tuning;
    }

    bool Try_ReadBackgroundPlacementCharacters(
        const json& root,
        BattleWorldVfxBackgroundPlacementTunings& tunings)
    {
        const auto charactersIter = root.find("characters");
        if (charactersIter == root.end() || !charactersIter->is_object())
            return false;

        for (const auto& [heroName, heroNode] : charactersIter->items())
        {
            if (!heroNode.is_object())
                continue;

            HeroId heroId = HeroId::END;
            if (!Try_ParsePlacementHeroJsonName(heroName, heroId))
            {
                LOG_WARN("BattleWorldUI material vfx background placement json has unknown character. character='{}'", heroName);
                continue;
            }

            const uint32 heroIndex = Resolve_PlacementHeroIndex(heroId);
            if (const auto statesIter = heroNode.find("states");
                statesIter != heroNode.end() && statesIter->is_object())
            {
                for (const auto& [stateName, stateNode] : statesIter->items())
                {
                    if (!stateNode.is_object())
                        continue;

                    BattleWorldVfxBackgroundState state{};
                    if (!Try_ParseBackgroundStateJsonName(stateName, state))
                    {
                        LOG_WARN("BattleWorldUI material vfx background placement json has unknown state. state='{}'", stateName);
                        continue;
                    }

                    const uint32 stateIndex = Resolve_BackgroundStateIndex(state);
                    tunings[heroIndex][stateIndex] =
                        Read_BackgroundPlacementTuningNode(stateNode, tunings[heroIndex][stateIndex]);
                }
            }
        }

        return true;
    }

    bool Try_LoadBackgroundPlacementTunings(
        const fs::path& jsonPath,
        BattleWorldVfxBackgroundLevelPlacementTunings& outTunings)
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
            LOG_WARN("BattleWorldUI material vfx background placement json parse failed. path='{}', error='{}'",
                String::ToString(jsonPath.wstring()),
                e.what());
            return false;
        }

        auto tunings = Make_DefaultBackgroundLevelPlacementTunings();
        if (const auto levelsIter = root.find("levels");
            levelsIter != root.end() && levelsIter->is_object())
        {
            for (const auto& [levelName, levelNode] : levelsIter->items())
            {
                if (!levelNode.is_object())
                    continue;

                LevelType levelType = LevelType::END;
                if (!BattleWorldUILevelPlacement::Try_ParseLevelJsonName(levelName, levelType))
                {
                    LOG_WARN("BattleWorldUI material vfx background placement json has unknown level. level='{}'", levelName);
                    continue;
                }

                const uint32 levelIndex = BattleWorldUILevelPlacement::Resolve_LevelIndex(levelType);
                Try_ReadBackgroundPlacementCharacters(levelNode, tunings[levelIndex]);
            }
        }
        else if (const auto charactersIter = root.find("characters");
            charactersIter != root.end() && charactersIter->is_object())
        {
            BattleWorldVfxBackgroundPlacementTunings legacyTunings =
                Make_DefaultBackgroundPlacementTunings();
            Try_ReadBackgroundPlacementCharacters(root, legacyTunings);
            for (auto& levelTuning : tunings)
                levelTuning = legacyTunings;
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

    json To_BackgroundPlacementTuningJson(const BattleWorldVfxBackgroundPlacementTuning& tuning)
    {
        json root = json::object();
        root["screenPosition"] = To_JsonVec3(tuning.screenPosition);
        root["baseOffset"] = To_JsonVec3(tuning.baseOffset);
        root["worldSize"] = To_JsonVec2(tuning.worldSize);
        root["visualScale"] = tuning.visualScale;
        root["pitchDegrees"] = tuning.pitchRadians * kBattleWorldVfxBackgroundRadiansToDegrees;
        root["rollDegrees"] = tuning.rollRadians * kBattleWorldVfxBackgroundRadiansToDegrees;
        root["yawOffsetDegrees"] = tuning.yawOffsetRadians * kBattleWorldVfxBackgroundRadiansToDegrees;
        return root;
    }

    bool Write_BackgroundPlacementTuningsToFile(
        const BattleWorldVfxBackgroundLevelPlacementTunings& tunings,
        fs::file_time_type* outWriteTime)
    {
        const fs::path jsonPath = Resolve_BackgroundPlacementJsonPath();

        json root = json::object();
        root["levels"] = json::object();
        for (uint32 levelIndex = 0; levelIndex < BattleWorldUILevelPlacement::kLevelCount; ++levelIndex)
        {
            const LevelType levelType = BattleWorldUILevelPlacement::Get_LevelByIndex(levelIndex);
            json levelNode = json::object();
            levelNode["characters"] = json::object();
            for (uint32 heroIndex = 0; heroIndex < kBattleWorldVfxBackgroundPlacementHeroCount; ++heroIndex)
            {
                const HeroId heroId = static_cast<HeroId>(heroIndex);
                json heroNode = json::object();
                heroNode["states"] = json::object();
                for (uint32 stateIndex = 0; stateIndex < kBattleWorldVfxBackgroundStateCount; ++stateIndex)
                {
                    heroNode["states"][To_BackgroundStateJsonName(stateIndex)] =
                        To_BackgroundPlacementTuningJson(tunings[levelIndex][heroIndex][stateIndex]);
                }

                levelNode["characters"][To_PlacementHeroJsonName(heroId)] = std::move(heroNode);
            }

            root["levels"][BattleWorldUILevelPlacement::To_LevelJsonName(levelType)] = std::move(levelNode);
        }

        error_code errorCode{};
        fs::create_directories(jsonPath.parent_path(), errorCode);
        if (errorCode)
        {
            LOG_WARN("BattleWorldUI material vfx background placement json directory create failed. path='{}'",
                String::ToString(jsonPath.parent_path().wstring()));
            return false;
        }

        try
        {
            ofstream file(jsonPath);
            if (!file.is_open())
            {
                LOG_WARN("BattleWorldUI material vfx background placement json open failed. path='{}'",
                    String::ToString(jsonPath.wstring()));
                return false;
            }

            file << root.dump(2) << '\n';
        }
        catch (const exception& e)
        {
            LOG_WARN("BattleWorldUI material vfx background placement json write failed. path='{}', error='{}'",
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

        LOG_INFO("BattleWorldUI material vfx background placement json saved. path='{}'",
            String::ToString(jsonPath.wstring()));
        return true;
    }

    RuntimeWorldUIPlacementTuning To_EditorWorldPlacementTuning(
        const BattleWorldVfxBackgroundPlacementTuning& tuning)
    {
        RuntimeWorldUIPlacementTuning editorTuning{};
        editorTuning.screenPosition = tuning.screenPosition;
        editorTuning.baseOffset = tuning.baseOffset;
        editorTuning.showKeyOffsets = false;
        editorTuning.worldSize = tuning.worldSize;
        editorTuning.visualScale = tuning.visualScale;
        editorTuning.pitchDegrees = 0.f;
        editorTuning.rollDegrees = tuning.rollRadians * kBattleWorldVfxBackgroundRadiansToDegrees;
        editorTuning.yawOffsetDegrees = 0.f;
        return editorTuning;
    }

    BattleWorldVfxBackgroundPlacementTuning From_EditorWorldPlacementTuning(
        const RuntimeWorldUIPlacementTuning& editorTuning)
    {
        BattleWorldVfxBackgroundPlacementTuning tuning{};
        tuning.screenPosition = editorTuning.screenPosition;
        tuning.baseOffset = editorTuning.baseOffset;
        tuning.worldSize = Vec2(
            max(0.01f, editorTuning.worldSize.x),
            max(0.01f, editorTuning.worldSize.y));
        tuning.visualScale = max(0.01f, editorTuning.visualScale);
        tuning.pitchRadians = 0.f;
        tuning.rollRadians = editorTuning.rollDegrees * kBattleWorldVfxBackgroundDegreesToRadians;
        tuning.yawOffsetRadians = 0.f;
        return tuning;
    }

    void Reload_BackgroundPlacementTuningIfNeeded(
        BattleWorldVfxBackgroundLevelPlacementTunings& tunings,
        fs::file_time_type& knownWriteTime,
        bool& hasKnownFile)
    {
        const fs::path jsonPath = Resolve_BackgroundPlacementJsonPath();
        if (!fs::exists(jsonPath))
        {
            if (hasKnownFile)
            {
                tunings = Make_DefaultBackgroundLevelPlacementTunings();
                knownWriteTime = {};
                hasKnownFile = false;
                LOG_INFO("BattleWorldUI material vfx background placement json removed. using defaults.");
            }
            return;
        }

        error_code errorCode{};
        const fs::file_time_type writeTime = fs::last_write_time(jsonPath, errorCode);
        if (errorCode)
            return;

        if (hasKnownFile && writeTime == knownWriteTime)
            return;

        BattleWorldVfxBackgroundLevelPlacementTunings loadedTunings{};
        knownWriteTime = writeTime;
        hasKnownFile = true;

        if (!Try_LoadBackgroundPlacementTunings(jsonPath, loadedTunings))
            return;

        tunings = loadedTunings;
        LOG_INFO("BattleWorldUI material vfx background placement json reloaded. path='{}'",
            String::ToString(jsonPath.wstring()));
    }

    Vec2 Resolve_AspectWorldSize(const Vec2& baseWorldSize, const Vec2& pixelSize)
    {
        if (pixelSize.x <= 0.f || pixelSize.y <= 0.f)
            return baseWorldSize;

        return Vec2(baseWorldSize.x, baseWorldSize.x * pixelSize.y / pixelSize.x);
    }

    Vec3 Normalize_BillboardAxis(Vec3 axis, const Vec3& fallback)
    {
        if (axis.LengthSquared() <= 0.000001f)
            return fallback;

        axis.Normalize();
        return axis;
    }

    Matrix Resolve_CameraBillboardMatrix(const Vec3& position)
    {
        const Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
        if (activeCamera == nullptr || activeCamera->Get_Transform() == nullptr)
            return Matrix::CreateTranslation(position.x, position.y, position.z);

        Shared<TransformCom> cameraTransform = activeCamera->Get_Transform();
        const Vec3 right = Normalize_BillboardAxis(cameraTransform->Get_WorldRight(), Vec3::Right);
        const Vec3 up = Normalize_BillboardAxis(cameraTransform->Get_WorldUp(), Vec3::Up);
        const Vec3 forward = Normalize_BillboardAxis(cameraTransform->Get_WorldForward(), Vec3::Forward);

        Matrix billboardMatrix = Matrix::Identity;
        billboardMatrix._11 = right.x;
        billboardMatrix._12 = right.y;
        billboardMatrix._13 = right.z;
        billboardMatrix._21 = up.x;
        billboardMatrix._22 = up.y;
        billboardMatrix._23 = up.z;
        billboardMatrix._31 = forward.x;
        billboardMatrix._32 = forward.y;
        billboardMatrix._33 = forward.z;
        billboardMatrix._41 = position.x;
        billboardMatrix._42 = position.y;
        billboardMatrix._43 = position.z;
        return billboardMatrix;
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

    class RenderTargetComposedMaterialVfxBackground final
    {
    public:
        RenderTargetComposedMaterialVfxBackground(
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

        HRESULT Initialize()
        {
            _renderTarget = RenderTarget::Create(
                L"BattleWorldMaterialVfxBackground",
                _device,
                _context,
                static_cast<uint32>(max(1.f, roundf(_pixelSize.x * kBattleWorldVfxBackgroundCompositionRenderScale))),
                static_cast<uint32>(max(1.f, roundf(_pixelSize.y * kBattleWorldVfxBackgroundCompositionRenderScale))),
                DXGI_FORMAT_R8G8B8A8_UNORM,
                Vec4(0.f, 0.f, 0.f, 0.f),
                false);
            CHECK_NULL(_renderTarget, E_FAIL);

            D3D11_BLEND_DESC blendDesc{};
            blendDesc.RenderTarget[0].BlendEnable = FALSE;
            blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
            blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
            blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
            blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
            blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            CHECK_FAILED(_device->CreateBlendState(&blendDesc, _compositionBlendState.GetAddressOf()), E_FAIL);

            return S_OK;
        }

        HRESULT Compose(const BattleWorldVfxBackgroundDef& backgroundDef, float timeDelta)
        {
            ENGINE_PROFILE_SCOPE_BASIC("BattleWorldUI.MaterialVfx.Compose");

            CHECK_NULL(_renderTarget, E_FAIL);
            CHECK_TRUE(backgroundDef.valid, E_FAIL);

            _materialVfxTime = fmodf(_materialVfxTime + max(0.f, timeDelta), 32.f);

            const auto& baseSrvs = backgroundDef.baseTexture->Get_SRVs();
            if (baseSrvs.empty() || baseSrvs[0] == nullptr)
                return E_FAIL;

            const vector<ComPtr<ID3D11ShaderResourceView>>* maskSrvs = &baseSrvs;
            if (backgroundDef.maskTexture)
                maskSrvs = &backgroundDef.maskTexture->Get_SRVs();

            const vector<ComPtr<ID3D11ShaderResourceView>>* noiseSrvs = &baseSrvs;
            if (backgroundDef.noiseTexture)
                noiseSrvs = &backgroundDef.noiseTexture->Get_SRVs();

            if (maskSrvs->empty() || maskSrvs->at(0) == nullptr ||
                noiseSrvs->empty() || noiseSrvs->at(0) == nullptr)
            {
                return E_FAIL;
            }

            ScopedRenderTargetState restoreState{ _context };

            ID3D11ShaderResourceView* nullSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
            _context->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);
            _context->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);

            CHECK_FAILED(_renderTarget->Begin(true, false), E_FAIL);

            const float viewportWidth = max(1.f, _pixelSize.x);
            const float viewportHeight = max(1.f, _pixelSize.y);
            const Vec2 center = Vec2(viewportWidth * 0.5f, viewportHeight * 0.5f);

            Matrix worldMatrix =
                Matrix::CreateScale(viewportWidth, viewportHeight, 1.f) *
                Matrix::CreateTranslation(
                    center.x - viewportWidth * 0.5f,
                    -center.y + viewportHeight * 0.5f,
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
            const float materialUseBaseMask = backgroundDef.maskTexture ? 0.f : 1.f;
            const float materialHasNoise = backgroundDef.noiseTexture ? 1.f : 0.f;

            CHECK_FAILED(_shader->Bind_Matrix("g_WorldMatrix", &worldMatrix), E_FAIL);
            CHECK_FAILED(_shader->Bind_Matrix("g_ViewMatrix", &viewMatrix), E_FAIL);
            CHECK_FAILED(_shader->Bind_Matrix("g_ProjMatrix", &projMatrix), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_Color", &backgroundDef.color, sizeof(Vec4)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_PatternUvOffset", &defaultPatternOffset, sizeof(Vec2)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_PatternUvScale", &defaultPatternScale, sizeof(Vec2)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_UseGrayScale", &disabled, sizeof(uint32)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_GrayAmount", &defaultGrayAmount, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_IsOverlay", &disabled, sizeof(uint32)), E_FAIL);
            CHECK_FAILED(_shader->Bind_SRV("g_Texture", baseSrvs[0].Get()), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_HoleCenterUV", &defaultHoleCenter, sizeof(Vec2)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_HoleRadius", &defaultHoleRadius, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_HoleFeather", &defaultHoleFeather, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_ViewportAspect", &defaultViewportAspect, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_UseFillRatio", &disabled, sizeof(uint32)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_FillRatio", &defaultFillRatio, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_FillOrigin", &fillOrigin, sizeof(uint32)), E_FAIL);
            const uint32 passIndex = 7u;
            CHECK_FAILED(_shader->Bind_SRV("g_MaskTexture", maskSrvs->at(0).Get()), E_FAIL);
            CHECK_FAILED(_shader->Bind_SRV("g_PatternMaskTexture", noiseSrvs->at(0).Get()), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxMode", &backgroundDef.materialMode, sizeof(uint32)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxColor", &backgroundDef.materialColor, sizeof(Vec4)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxCoreColor", &backgroundDef.materialCoreColor, sizeof(Vec4)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxFlowColor", &backgroundDef.materialFlowColor, sizeof(Vec4)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxGlow", &backgroundDef.materialGlow, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxAlpha", &backgroundDef.materialAlpha, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxCoreStrength", &backgroundDef.materialCoreStrength, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxCoreWidth", &backgroundDef.materialCoreWidth, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxCoreSoftness", &backgroundDef.materialCoreSoftness, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxNoiseStrength", &backgroundDef.materialNoiseStrength, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxUvScroll", &backgroundDef.materialUvScroll, sizeof(Vec2)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxFlowSpeed", &backgroundDef.materialFlowSpeed, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxPreserveColor", &backgroundDef.materialPreserveColor, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxEdgeWidth", &backgroundDef.materialEdgeWidth, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxEdgeSoftness", &backgroundDef.materialEdgeSoftness, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxErosion", &backgroundDef.materialErosion, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxFlicker", &backgroundDef.materialFlicker, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxDistortion", &backgroundDef.materialDistortion, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxEdgeWobbleSpeed", &backgroundDef.materialEdgeWobbleSpeed, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxTime", &_materialVfxTime, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxUseBaseMask", &materialUseBaseMask, sizeof(float)), E_FAIL);
            CHECK_FAILED(_shader->Bind_RawValue("g_MaterialVfxHasNoise", &materialHasNoise, sizeof(float)), E_FAIL);

            CHECK_FAILED(_shader->Begin(passIndex), E_FAIL);
            const float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
            _context->OMSetBlendState(_compositionBlendState.Get(), blendFactor, 0xffffffff);
            CHECK_FAILED(_viBuffer->Bind_Resources(), E_FAIL);
            CHECK_FAILED(_viBuffer->Render(), E_FAIL);

            _context->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);
            _context->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);

            return S_OK;
        }

        HRESULT Bind_ShaderResource(ShaderCom* shader, const char* name) const
        {
            CHECK_NULL(_renderTarget, E_FAIL);
            return _renderTarget->Bind_ShaderResource(shader, name);
        }

        static Unique<RenderTargetComposedMaterialVfxBackground> Create(
            const ComPtr<Device>& device,
            const ComPtr<Context>& context,
            const Shared<ShaderCom>& shader,
            const Shared<VIBuffer_Rect>& viBuffer,
            const Vec2& pixelSize)
        {
            auto surface = make_unique<RenderTargetComposedMaterialVfxBackground>(
                device,
                context,
                shader,
                viBuffer,
                pixelSize);
            if (FAILED(surface->Initialize()))
                return nullptr;

            return surface;
        }

    private:
        ComPtr<Device> _device{};
        ComPtr<Context> _context{};
        Shared<ShaderCom> _shader{};
        Shared<VIBuffer_Rect> _viBuffer{};
        Shared<RenderTarget> _renderTarget{};
        ComPtr<ID3D11BlendState> _compositionBlendState{};
        Vec2 _pixelSize = Vec2(500.f, 500.f);
        float _materialVfxTime = 0.f;
    };
}

struct BattleWorldUI_MaterialVfxBackground::Impl
{
    Shared<ShaderCom> shader{};
    Shared<VIBuffer_Rect> viBuffer{};
    BattleWorldVfxBackgroundDef backgroundDef{};
    Unique<RenderTargetComposedMaterialVfxBackground> surface{};
    BattleWorldVfxBackgroundLevelPlacementTunings placementTunings =
        Make_DefaultBackgroundLevelPlacementTunings();
    fs::file_time_type placementTuningWriteTime{};
    bool hasPlacementTuningFile = false;
    LevelType activePlacementLevelType = LevelType::END;
    LevelType editorPlacementLevelType = LevelType::END;
    HeroId activePlacementHeroId = HeroId::END;
    HeroId editorPlacementHeroId = HeroId::END;
    uint32 activeStateIndex = 0;
    uint32 editorStateIndex = 0;
    BattleTurnState introState = BattleTurnState::None;
    HeroId introHeroId = HeroId::END;
    bool hasIntroKey = false;
    BattleWorldSurfaceAnimState introAnim{};
    Vec3 introStartOffset = Vec3::Zero;
    Matrix worldMatrix = Matrix::Identity;
    bool visible = false;
};

BattleWorldUI_MaterialVfxBackground::BattleWorldUI_MaterialVfxBackground(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

BattleWorldUI_MaterialVfxBackground::~BattleWorldUI_MaterialVfxBackground() = default;

HRESULT BattleWorldUI_MaterialVfxBackground::Initialize(void* arg)
{
    CHECK_FAILED(GameObject::Initialize(arg), E_FAIL);

    _impl = make_unique<Impl>();
    CHECK_NULL(_impl, E_FAIL);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Shader_UI"), _impl->shader), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("VIBuffer_Rect"), _impl->viBuffer), E_FAIL);

    CHECK_FAILED(Try_LoadBackgroundDef(_device, _context, _impl->backgroundDef) ? S_OK : E_FAIL, E_FAIL);

    _impl->surface = RenderTargetComposedMaterialVfxBackground::Create(
        _device,
        _context,
        _impl->shader,
        _impl->viBuffer,
        _impl->backgroundDef.pixelSize);
    CHECK_NULL(_impl->surface, E_FAIL);

    Reload_BackgroundPlacementTuningIfNeeded(
        _impl->placementTunings,
        _impl->placementTuningWriteTime,
        _impl->hasPlacementTuningFile);

    Hide();
    return S_OK;
}

HRESULT BattleWorldUI_MaterialVfxBackground::Warmup_ForBattleState(HeroId actorHeroId)
{
    ENGINE_PROFILE_SCOPE_BASIC("BattleWorldUI.MaterialVfx.Warmup");

    if (_impl == nullptr || _impl->surface == nullptr || !_impl->backgroundDef.valid)
        return E_FAIL;

    const LevelType placementLevelType =
        BattleWorldUILevelPlacement::Normalize_LevelType(static_cast<LevelType>(GAME->Current_LevelIndex()));
    const HeroId placementHeroId = Normalize_PlacementHeroId(actorHeroId);
    _impl->activePlacementLevelType = placementLevelType;
    if (_impl->editorPlacementLevelType == LevelType::END)
        _impl->editorPlacementLevelType = placementLevelType;
    _impl->activePlacementHeroId = placementHeroId;
    if (_impl->editorPlacementHeroId == HeroId::END)
        _impl->editorPlacementHeroId = placementHeroId;

    return _impl->surface->Compose(_impl->backgroundDef, 0.f);
}

void BattleWorldUI_MaterialVfxBackground::Show_ForBattleState(
    const Vec3& anchorWorldPos,
    BattleTurnState currentState,
    HeroId actorHeroId,
    float timeDelta)
{
    ENGINE_PROFILE_SCOPE_DETAIL("BattleWorldUI.MaterialVfx.Show");

    BattleWorldVfxBackgroundState backgroundState{};
    if (_impl == nullptr || _impl->surface == nullptr || !_impl->backgroundDef.valid ||
        !Try_ResolveBackgroundState(currentState, backgroundState))
    {
        Hide();
        return;
    }

    const LevelType placementLevelType =
        BattleWorldUILevelPlacement::Normalize_LevelType(static_cast<LevelType>(GAME->Current_LevelIndex()));
    const uint32 placementLevelIndex = BattleWorldUILevelPlacement::Resolve_LevelIndex(placementLevelType);
    const HeroId placementHeroId = Normalize_PlacementHeroId(actorHeroId);
    const uint32 placementHeroIndex = Resolve_PlacementHeroIndex(placementHeroId);
    const uint32 stateIndex = Resolve_BackgroundStateIndex(backgroundState);
    _impl->activePlacementLevelType = placementLevelType;
    if (_impl->editorPlacementLevelType == LevelType::END)
        _impl->editorPlacementLevelType = placementLevelType;
    _impl->activePlacementHeroId = placementHeroId;
    _impl->activeStateIndex = stateIndex;
    if (_impl->editorPlacementHeroId == HeroId::END)
        _impl->editorPlacementHeroId = placementHeroId;

    const bool restartIntro =
        !_impl->hasIntroKey ||
        _impl->introHeroId != placementHeroId;
    if (restartIntro)
    {
        _impl->introState = currentState;
        _impl->introHeroId = placementHeroId;
        _impl->hasIntroKey = true;
        Restart_BattleWorldSurfaceAnim(_impl->introAnim, 0);
    }

    Advance_BattleWorldSurfaceAnim(_impl->introAnim, timeDelta);

    if (FAILED(_impl->surface->Compose(_impl->backgroundDef, GAME->Get_UnscaledTimeDelta())))
    {
        Set_Visible(false);
        _impl->visible = false;
        return;
    }

    const BattleWorldVfxBackgroundPlacementTuning& placementTuning =
        _impl->placementTunings[placementLevelIndex][placementHeroIndex][stateIndex];
    const Vec3 position =
        anchorWorldPos +
        Resolve_CameraScreenPositionDelta(placementTuning.screenPosition) +
        BattleWorldUIPlacement::Resolve_CameraHorizontalPlacementDelta(placementTuning.baseOffset);
    _impl->introStartOffset = Resolve_CameraScreenPositionDelta(Vec3(0.f, -0.06f, 0.f));
    const Vec2 aspectWorldSize = Resolve_AspectWorldSize(
        placementTuning.worldSize,
        _impl->backgroundDef.pixelSize);

    _impl->worldMatrix =
        Matrix::CreateScale(
            aspectWorldSize.x * placementTuning.visualScale,
            aspectWorldSize.y * placementTuning.visualScale,
            1.f) *
        Matrix::CreateRotationZ(placementTuning.rollRadians) *
        Resolve_CameraBillboardMatrix(position);
    _impl->visible = true;

    Set_Visible(true);
}

void BattleWorldUI_MaterialVfxBackground::Hide()
{
    Set_Visible(false);

    if (_impl == nullptr)
        return;

    _impl->hasIntroKey = false;
    _impl->visible = false;
}

LevelType BattleWorldUI_MaterialVfxBackground::Get_EditorWorldPlacementLevelType() const
{
    if (_impl == nullptr)
        return LevelType::Stage01;

    if (_impl->editorPlacementLevelType != LevelType::END)
        return BattleWorldUILevelPlacement::Normalize_LevelType(_impl->editorPlacementLevelType);

    if (_impl->activePlacementLevelType != LevelType::END)
        return BattleWorldUILevelPlacement::Normalize_LevelType(_impl->activePlacementLevelType);

    return LevelType::Stage01;
}

void BattleWorldUI_MaterialVfxBackground::Set_EditorWorldPlacementLevelType(LevelType levelType)
{
    if (_impl == nullptr)
        return;

    _impl->editorPlacementLevelType = BattleWorldUILevelPlacement::Normalize_LevelType(levelType);
}

HeroId BattleWorldUI_MaterialVfxBackground::Get_EditorWorldPlacementHeroId() const
{
    if (_impl == nullptr)
        return HeroId::Gustave;

    if (_impl->editorPlacementHeroId != HeroId::END)
        return Normalize_PlacementHeroId(_impl->editorPlacementHeroId);

    if (_impl->activePlacementHeroId != HeroId::END)
        return Normalize_PlacementHeroId(_impl->activePlacementHeroId);

    return HeroId::Gustave;
}

void BattleWorldUI_MaterialVfxBackground::Set_EditorWorldPlacementHeroId(HeroId heroId)
{
    if (_impl == nullptr)
        return;

    _impl->editorPlacementHeroId = Normalize_PlacementHeroId(heroId);
}

uint32 BattleWorldUI_MaterialVfxBackground::Get_EditorWorldPlacementVariantCount() const
{
    return kBattleWorldVfxBackgroundStateCount;
}

uint32 BattleWorldUI_MaterialVfxBackground::Get_EditorWorldPlacementVariantIndex() const
{
    if (_impl == nullptr)
        return 0;

    return Normalize_BackgroundStateIndex(_impl->editorStateIndex);
}

const char* BattleWorldUI_MaterialVfxBackground::Get_EditorWorldPlacementVariantLabel(uint32 index) const
{
    return To_BackgroundStateEditorLabel(index);
}

void BattleWorldUI_MaterialVfxBackground::Set_EditorWorldPlacementVariantIndex(uint32 index)
{
    if (_impl == nullptr)
        return;

    _impl->editorStateIndex = Normalize_BackgroundStateIndex(index);
}

RuntimeWorldUIPlacementTuning BattleWorldUI_MaterialVfxBackground::Get_EditorWorldPlacementTuning() const
{
    if (_impl == nullptr)
    {
        return To_EditorWorldPlacementTuning(Make_DefaultBackgroundPlacementTuning(
            HeroId::Gustave,
            BattleWorldVfxBackgroundState::Command));
    }

    const uint32 levelIndex = BattleWorldUILevelPlacement::Resolve_LevelIndex(Get_EditorWorldPlacementLevelType());
    const uint32 heroIndex = Resolve_PlacementHeroIndex(Get_EditorWorldPlacementHeroId());
    const uint32 stateIndex = Normalize_BackgroundStateIndex(_impl->editorStateIndex);
    return To_EditorWorldPlacementTuning(_impl->placementTunings[levelIndex][heroIndex][stateIndex]);
}

void BattleWorldUI_MaterialVfxBackground::Set_EditorWorldPlacementTuning(
    const RuntimeWorldUIPlacementTuning& tuning,
    bool saveImmediately)
{
    if (_impl == nullptr)
        return;

    const uint32 levelIndex = BattleWorldUILevelPlacement::Resolve_LevelIndex(Get_EditorWorldPlacementLevelType());
    const uint32 heroIndex = Resolve_PlacementHeroIndex(Get_EditorWorldPlacementHeroId());
    const uint32 stateIndex = Normalize_BackgroundStateIndex(_impl->editorStateIndex);
    _impl->placementTunings[levelIndex][heroIndex][stateIndex] = From_EditorWorldPlacementTuning(tuning);

    if (!saveImmediately)
        return;

    if (Write_BackgroundPlacementTuningsToFile(
        _impl->placementTunings,
        &_impl->placementTuningWriteTime))
    {
        _impl->hasPlacementTuningFile = true;
    }
}

void BattleWorldUI_MaterialVfxBackground::Reload_EditorWorldPlacementTuning()
{
    if (_impl == nullptr)
        return;

    const fs::path jsonPath = Resolve_BackgroundPlacementJsonPath();
    if (!fs::exists(jsonPath))
    {
        _impl->placementTunings = Make_DefaultBackgroundLevelPlacementTunings();
        _impl->placementTuningWriteTime = {};
        _impl->hasPlacementTuningFile = false;
        LOG_INFO("BattleWorldUI material vfx background placement json reload requested, but file is missing. using defaults.");
        return;
    }

    BattleWorldVfxBackgroundLevelPlacementTunings loadedTunings{};
    if (!Try_LoadBackgroundPlacementTunings(jsonPath, loadedTunings))
        return;

    error_code errorCode{};
    const fs::file_time_type writeTime = fs::last_write_time(jsonPath, errorCode);
    if (!errorCode)
        _impl->placementTuningWriteTime = writeTime;

    _impl->placementTunings = loadedTunings;
    _impl->hasPlacementTuningFile = true;
}

void BattleWorldUI_MaterialVfxBackground::Reset_EditorWorldPlacementTuning()
{
    if (_impl == nullptr)
        return;

    const HeroId heroId = Get_EditorWorldPlacementHeroId();
    const uint32 levelIndex = BattleWorldUILevelPlacement::Resolve_LevelIndex(Get_EditorWorldPlacementLevelType());
    const uint32 stateIndex = Normalize_BackgroundStateIndex(_impl->editorStateIndex);
    _impl->placementTunings[levelIndex][Resolve_PlacementHeroIndex(heroId)][stateIndex] =
        Make_DefaultBackgroundPlacementTuning(
            heroId,
            kBattleWorldVfxBackgroundStateDefs[stateIndex].state);
    if (Write_BackgroundPlacementTuningsToFile(
        _impl->placementTunings,
        &_impl->placementTuningWriteTime))
    {
        _impl->hasPlacementTuningFile = true;
    }
}

void BattleWorldUI_MaterialVfxBackground::Late_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!Is_Visible())
        return;

    GAME->Add_RenderGroup(RenderGroup::UIEffect, GetSharedPtr<GameObject>());
}

HRESULT BattleWorldUI_MaterialVfxBackground::Render()
{
    if (!Is_Visible() || _impl == nullptr || !_impl->visible ||
        _impl->shader == nullptr || _impl->viBuffer == nullptr || _impl->surface == nullptr)
    {
        return S_OK;
    }

    const BattleWorldSurfaceAnimRenderState renderState =
        Apply_BattleWorldSurfaceAnim(
            _impl->introAnim,
            _impl->worldMatrix,
            kBattleWorldVfxBackgroundWorldColor,
            _impl->introStartOffset);
    CHECK_FAILED(Bind_WorldUIPlaneShaderState(_impl->shader.get(), renderState.worldMatrix, renderState.color), E_FAIL);
    CHECK_FAILED(_impl->surface->Bind_ShaderResource(_impl->shader.get(), "g_Texture"), E_FAIL);
    CHECK_FAILED(_impl->shader->Begin(0), E_FAIL);
    CHECK_FAILED(GAME->Bind_BackgroundUIBehindPlayerStencilState(), E_FAIL);
    CHECK_FAILED(_impl->viBuffer->Bind_Resources(), E_FAIL);
    CHECK_FAILED(_impl->viBuffer->Render(), E_FAIL);
    CHECK_FAILED(GAME->Bind_UINoDepthStencilState(), E_FAIL);

    return S_OK;
}

Shared<GameObject> BattleWorldUI_MaterialVfxBackground::Clone(void* arg)
{
    UNREFERENCED_PARAMETER(arg);
    return nullptr;
}

void BattleWorldUI_MaterialVfxBackground::Free()
{
    __super::Free();

    _impl.reset();
}

Shared<BattleWorldUI_MaterialVfxBackground> BattleWorldUI_MaterialVfxBackground::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context)
{
    auto instance = make_shared<BattleWorldUI_MaterialVfxBackground>(device, context);
    if (FAILED(instance->Initialize(nullptr)))
    {
        MSG_BOX("Failed to create BattleWorldUI_MaterialVfxBackground");
        return nullptr;
    }

    return instance;
}

NS_END
