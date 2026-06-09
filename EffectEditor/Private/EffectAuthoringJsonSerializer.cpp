#include "EffectAuthoringJsonSerializer.h"

#include "GameInstance.h"
#include "Helper_EffectAuthoring.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    json To_Json(const EffectMaterialInstanceData& data);
    void From_Json(const json& root, EffectMaterialInstanceData& outData);

    //## Common Codec Helpers

    template <typename EnumType>
    string To_EnumToken(EnumType value)
    {
        return string(magic_enum::enum_name(value));
    }

    template <typename EnumType>
    bool Try_ReadEnumToken(const json& node, EnumType& outValue)
    {
        if (!node.is_string())
            return false;

        const optional<EnumType> parsed = magic_enum::enum_cast<EnumType>(node.get<string>());
        if (!parsed.has_value())
            return false;

        outValue = parsed.value();
        return true;
    }

    template <typename T>
    void Read_NumberField(const json& node, const char* key, T& outValue)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_number())
            return;

        outValue = iter->get<T>();
    }

    EmitterSortPolicy Resolve_SortPolicyFromLegacyMode(EmitterSortMode sortMode)
    {
        switch (sortMode)
        {
        case EmitterSortMode::ViewProjDepth:
        case EmitterSortMode::DistanceToView:
            return EmitterSortPolicy::EmitterDepth;
        case EmitterSortMode::None:
        case EmitterSortMode::AgeOldestFirst:
        case EmitterSortMode::AgeNewestFirst:
        default:
            return EmitterSortPolicy::None;
        }
    }

    void Read_BoolField(const json& node, const char* key, bool& outValue)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_boolean())
            return;

        outValue = iter->get<bool>();
    }

    json To_Json(const DistributionRandomSeedData& data)
    {
        json root = json::object();
        root["mode"] = To_EnumToken(data.mode);
        root["manualSeed"] = data.manualSeed;
        root["useInstanceSeed"] = data.useInstanceSeed;
        return root;
    }

    void From_Json(const json& root, DistributionRandomSeedData& outData)
    {
        outData = DistributionRandomSeedData{};
        if (!root.is_object())
            return;

        const auto modeIter = root.find("mode");
        if (modeIter != root.end())
            Try_ReadEnumToken(*modeIter, outData.mode);
        Read_NumberField(root, "manualSeed", outData.manualSeed);
        Read_BoolField(root, "useInstanceSeed", outData.useInstanceSeed);
    }

    void Write_RandomSeedField(json& root, const DistributionRandomSeedData& data)
    {
        root["randomSeed"] = To_Json(data);
    }

    void Read_RandomSeedField(const json& root, DistributionRandomSeedData& outData)
    {
        outData = DistributionRandomSeedData{};
        const auto iter = root.find("randomSeed");
        if (iter != root.end())
            From_Json(*iter, outData);
    }

    bool Has_RandomSeedField(const json& root)
    {
        const auto iter = root.find("randomSeed");
        return iter != root.end() && iter->is_object();
    }

    bool Is_DefaultRandomSeed(const DistributionRandomSeedData& data)
    {
        return data.mode == DistributionRandomSeedMode::Default &&
               data.manualSeed == 0u &&
               !data.useInstanceSeed;
    }

    void Apply_LegacyRandomSeedToUniform(FloatDistributionData& data, const DistributionRandomSeedData& legacySeed)
    {
        if (auto* uniform = get_if<UniformFloatDistributionData>(&data.payload);
            data.mode == DistributionMode::Uniform && uniform != nullptr && Is_DefaultRandomSeed(uniform->randomSeed))
            uniform->randomSeed = legacySeed;
    }

    void Apply_LegacyRandomSeedToUniform(Vector2DistributionData& data, const DistributionRandomSeedData& legacySeed)
    {
        if (auto* uniform = get_if<UniformVector2DistributionData>(&data.payload);
            data.mode == DistributionMode::Uniform && uniform != nullptr && Is_DefaultRandomSeed(uniform->randomSeed))
            uniform->randomSeed = legacySeed;
    }

    void Apply_LegacyRandomSeedToUniform(Vector3DistributionData& data, const DistributionRandomSeedData& legacySeed)
    {
        if (auto* uniform = get_if<UniformVector3DistributionData>(&data.payload);
            data.mode == DistributionMode::Uniform && uniform != nullptr && Is_DefaultRandomSeed(uniform->randomSeed))
            uniform->randomSeed = legacySeed;
    }

    void Apply_LegacyRandomSeedToUniform(ColorDistributionData& data, const DistributionRandomSeedData& legacySeed)
    {
        if (auto* uniform = get_if<UniformColorDistributionData>(&data.payload);
            data.mode == DistributionMode::Uniform && uniform != nullptr && Is_DefaultRandomSeed(uniform->randomSeed))
            uniform->randomSeed = legacySeed;
    }

    void Read_StringField(const json& node, const char* key, string& outValue)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_string())
            return;

        outValue = iter->get<string>();
    }

    bool Try_MigrateLegacyTextureId(const string& textureId, string& outGuid, string& outPath)
    {
        if (textureId == "Texture_Effect_DefaultTexture")
        {
            outGuid.clear();
            outPath = "Effects/Textures/Shared/DefaultTexture.dds";
            return true;
        }

        if (textureId == "Texture_Effect_Clouds4")
        {
            outGuid.clear();
            outPath = "Effects/Textures/Shared/clouds4_T.dds";
            return true;
        }

        return false;
    }

    wstring Resolve_TexturePathByGuid(const string& textureGuid)
    {
        if (GAME == nullptr || textureGuid.empty())
            return {};

        const AssetMeta* assetMeta = GAME->Find_AssetByGUID(textureGuid);
        if (assetMeta == nullptr || assetMeta->type != "Texture")
            return {};

        const wstring resolvedPath = GAME->Resolve_AssetPath(textureGuid);
        return !resolvedPath.empty() && fs::exists(resolvedPath) ? resolvedPath : wstring{};
    }

    wstring Resolve_TexturePathByPath(const string& texturePath)
    {
        if (texturePath.empty())
            return {};

        fs::path candidatePath = String::ToWString(texturePath);
        if (candidatePath.is_relative() && GAME != nullptr)
            candidatePath = fs::path(GAME->Get_AssetRoot()) / candidatePath;

        candidatePath = candidatePath.lexically_normal();
        return fs::exists(candidatePath) ? candidatePath.wstring() : wstring{};
    }

    string Make_TextureGuidForSave(const string& textureGuid, const string& texturePath)
    {
        if (!textureGuid.empty())
            return textureGuid;

        if (GAME == nullptr)
            return {};

        const wstring resolvedPath = Resolve_TexturePathByPath(texturePath);
        return resolvedPath.empty() ? string{} : GAME->Ensure_AssetGUID(resolvedPath, "Texture");
    }

    void Fill_TextureGuidFromPath(string& textureGuid, const string& texturePath)
    {
        if (!textureGuid.empty() || GAME == nullptr)
            return;

        const wstring resolvedPath = Resolve_TexturePathByPath(texturePath);
        if (resolvedPath.empty())
            return;

        textureGuid = GAME->Ensure_AssetGUID(resolvedPath, "Texture");
    }

    void Fill_TexturePathHintFromGuid(const string& textureGuid, string& texturePath)
    {
        if (!texturePath.empty())
            return;

        const wstring resolvedPath = Resolve_TexturePathByGuid(textureGuid);
        if (resolvedPath.empty() || GAME == nullptr)
            return;

        const fs::path assetRoot = fs::path(GAME->Get_AssetRoot()).lexically_normal();
        const fs::path relativePath = fs::path(resolvedPath).lexically_normal().lexically_relative(assetRoot);
        if (relativePath.empty() || relativePath.native().starts_with(L".."))
            return;

        texturePath = String::ToString(relativePath.wstring());
        ranges::replace(texturePath, '\\', '/');
    }

    //## Primitive Value Codec

    json To_Json(const Vec2& value)
    {
        return json::array({ value.x, value.y });
    }

    json To_Json(const Vec3& value)
    {
        return json::array({ value.x, value.y, value.z });
    }

    json To_Json(const Color& value)
    {
        return json::array({ value.x, value.y, value.z, value.w });
    }

    void Read_Vec2(const json& node, Vec2& outValue)
    {
        if (!node.is_array() || node.size() < 2 || !node[0].is_number() || !node[1].is_number())
            return;

        outValue.x = node[0].get<float>();
        outValue.y = node[1].get<float>();
    }

    void Read_Vec3(const json& node, Vec3& outValue)
    {
        if (!node.is_array() || node.size() < 3 || !node[0].is_number() || !node[1].is_number() || !node[2].is_number())
            return;

        outValue.x = node[0].get<float>();
        outValue.y = node[1].get<float>();
        outValue.z = node[2].get<float>();
    }

    void Read_Color(const json& node, Color& outValue)
    {
        if (!node.is_array() || node.size() < 4 ||
            !node[0].is_number() || !node[1].is_number() || !node[2].is_number() || !node[3].is_number())
            return;

        outValue.x = node[0].get<float>();
        outValue.y = node[1].get<float>();
        outValue.z = node[2].get<float>();
        outValue.w = node[3].get<float>();
    }

    bool Is_SameColor(const Color& lhs, const Color& rhs)
    {
        return
            lhs.x == rhs.x &&
            lhs.y == rhs.y &&
            lhs.z == rhs.z &&
            lhs.w == rhs.w;
    }

    bool Is_DefaultAdditiveContribution(const EffectMaterialAdditiveContributionData& data)
    {
        const EffectMaterialAdditiveContributionData defaultData{};
        return
            data.colorSource == defaultData.colorSource &&
            data.amountSource == defaultData.amountSource &&
            data.coveragePolicy == defaultData.coveragePolicy &&
            data.intensityScale == defaultData.intensityScale &&
            data.blackNeutral == defaultData.blackNeutral &&
            Is_SameColor(data.emissiveColor, defaultData.emissiveColor) &&
            Is_SameColor(data.constantColor, defaultData.constantColor);
    }

    bool Is_DefaultCoreEmissive(const EffectMaterialCoreEmissiveData& data)
    {
        const EffectMaterialCoreEmissiveData defaultData{};
        return
            data.enabled == defaultData.enabled &&
            Is_SameColor(data.coreColor, defaultData.coreColor) &&
            data.corePower == defaultData.corePower &&
            data.coreIntensity == defaultData.coreIntensity &&
            data.outerPower == defaultData.outerPower &&
            data.outerIntensity == defaultData.outerIntensity;
    }

    json To_Json(const EffectMaterialCoreEmissiveData& data)
    {
        json root = json::object();
        root["enabled"] = data.enabled;
        root["coreColor"] = To_Json(data.coreColor);
        root["corePower"] = data.corePower;
        root["coreIntensity"] = data.coreIntensity;
        root["outerPower"] = data.outerPower;
        root["outerIntensity"] = data.outerIntensity;
        return root;
    }

    void From_Json(const json& root, EffectMaterialCoreEmissiveData& outData)
    {
        if (!root.is_object())
            return;

        Read_BoolField(root, "enabled", outData.enabled);
        if (const auto coreColorIter = root.find("coreColor"); coreColorIter != root.end())
            Read_Color(*coreColorIter, outData.coreColor);
        Read_NumberField(root, "corePower", outData.corePower);
        Read_NumberField(root, "coreIntensity", outData.coreIntensity);
        Read_NumberField(root, "outerPower", outData.outerPower);
        Read_NumberField(root, "outerIntensity", outData.outerIntensity);
    }

    json To_Json(const EffectMaterialAdditiveContributionData& data)
    {
        json root = json::object();
        root["colorSource"] = To_EnumToken(data.colorSource);
        root["amountSource"] = To_EnumToken(data.amountSource);
        root["coveragePolicy"] = To_EnumToken(data.coveragePolicy);
        root["intensityScale"] = data.intensityScale;
        root["blackNeutral"] = data.blackNeutral;
        root["emissiveColor"] = To_Json(data.emissiveColor);
        root["constantColor"] = To_Json(data.constantColor);
        return root;
    }

    void From_Json(const json& root, EffectMaterialAdditiveContributionData& outData)
    {
        if (!root.is_object())
            return;

        if (const auto colorSourceIter = root.find("colorSource"); colorSourceIter != root.end())
            Try_ReadEnumToken(*colorSourceIter, outData.colorSource);
        if (const auto amountSourceIter = root.find("amountSource"); amountSourceIter != root.end())
            Try_ReadEnumToken(*amountSourceIter, outData.amountSource);
        if (const auto coveragePolicyIter = root.find("coveragePolicy"); coveragePolicyIter != root.end())
            Try_ReadEnumToken(*coveragePolicyIter, outData.coveragePolicy);

        Read_NumberField(root, "intensityScale", outData.intensityScale);
        Read_BoolField(root, "blackNeutral", outData.blackNeutral);

        if (const auto emissiveColorIter = root.find("emissiveColor"); emissiveColorIter != root.end())
            Read_Color(*emissiveColorIter, outData.emissiveColor);
        if (const auto constantColorIter = root.find("constantColor"); constantColorIter != root.end())
            Read_Color(*constantColorIter, outData.constantColor);
    }

    json To_Json(const ParticleSystemAuthoringData& data)
    {
        json root = json::object();
        root["autoDeactivate"] = data.autoDeactivate;
        return root;
    }

    void From_Json(const json& root, ParticleSystemAuthoringData& outData)
    {
        if (!root.is_object())
            return;

        Read_BoolField(root, "autoDeactivate", outData.autoDeactivate);
    }

    json To_Json(const TrailTypeData& data)
    {
        json root = json::object();
        root["width"] = data.width;
        root["segmentLifetime"] = data.segmentLifetime;
        root["historyCount"] = data.historyCount;
        root["uvTiling"] = data.uvTiling;
        root["maxTrailLength"] = data.maxTrailLength;
        root["tailFadeLength"] = data.tailFadeLength;
        root["curveQuality"] = To_EnumToken(data.curveQuality);
        root["sampleSpacing"] = data.sampleSpacing;
        root["curveSubdivision"] = data.curveSubdivision;
        root["smoothTangent"] = data.smoothTangent;
        root["sideFade"] = data.sideFade;
        return root;
    }

    void From_Json(const json& root, TrailTypeData& outData)
    {
        if (!root.is_object())
            return;

        Read_NumberField(root, "width", outData.width);
        Read_NumberField(root, "segmentLifetime", outData.segmentLifetime);
        Read_NumberField(root, "historyCount", outData.historyCount);
        Read_NumberField(root, "uvTiling", outData.uvTiling);
        Read_NumberField(root, "maxTrailLength", outData.maxTrailLength);
        Read_NumberField(root, "tailFadeLength", outData.tailFadeLength);
        const auto curveQualityIter = root.find("curveQuality");
        if (curveQualityIter != root.end())
            Try_ReadEnumToken(*curveQualityIter, outData.curveQuality);
        Read_NumberField(root, "sampleSpacing", outData.sampleSpacing);
        Read_NumberField(root, "curveSubdivision", outData.curveSubdivision);
        Read_BoolField(root, "smoothTangent", outData.smoothTangent);
        Read_NumberField(root, "sideFade", outData.sideFade);
    }

    json To_Json(const RibbonTypeData& data)
    {
        json root = json::object();
        root["maxParticleInTrailCount"] = data.maxParticleInTrailCount;
        root["spawnInitialParticle"] = data.spawnInitialParticle;
        root["renderAxis"] = To_EnumToken(data.renderAxis);
        root["tilingDistance"] = data.tilingDistance;
        root["distanceTessellationStepSize"] = data.distanceTessellationStepSize;
        root["enablePreviousTangentRecalculation"] = data.enablePreviousTangentRecalculation;
        root["linkOrder"] = To_EnumToken(data.linkOrder);
        return root;
    }

    void From_Json(const json& root, RibbonTypeData& outData)
    {
        if (!root.is_object())
            return;

        Read_NumberField(root, "maxParticleInTrailCount", outData.maxParticleInTrailCount);
        Read_BoolField(root, "spawnInitialParticle", outData.spawnInitialParticle);
        if (const auto renderAxisIter = root.find("renderAxis"); renderAxisIter != root.end())
            Try_ReadEnumToken(*renderAxisIter, outData.renderAxis);
        Read_NumberField(root, "tilingDistance", outData.tilingDistance);
        Read_NumberField(root, "distanceTessellationStepSize", outData.distanceTessellationStepSize);
        Read_BoolField(root, "enablePreviousTangentRecalculation", outData.enablePreviousTangentRecalculation);
        if (const auto linkOrderIter = root.find("linkOrder"); linkOrderIter != root.end())
            Try_ReadEnumToken(*linkOrderIter, outData.linkOrder);
    }

    json To_Json(const SourceHistoryRibbonTypeData& data)
    {
        json root = json::object();
        root["maxSampleCount"] = data.maxSampleCount;
        root["sampleSpacing"] = data.sampleSpacing;
        root["sampleInterval"] = data.sampleInterval;
        root["maxLength"] = data.maxLength;
        root["tailFadeLength"] = data.tailFadeLength;
        root["baseWidth"] = data.baseWidth;
        root["tilingDistance"] = data.tilingDistance;
        root["previewMotionPreset"] = To_EnumToken(data.previewMotionPreset);
        root["previewMotionAxis"] = To_EnumToken(data.previewMotionAxis);
        root["previewMotionAmplitude"] = data.previewMotionAmplitude;
        root["previewMotionSpeed"] = data.previewMotionSpeed;
        return root;
    }

    void From_Json(const json& root, SourceHistoryRibbonTypeData& outData)
    {
        if (!root.is_object())
            return;

        Read_NumberField(root, "maxSampleCount", outData.maxSampleCount);
        Read_NumberField(root, "sampleSpacing", outData.sampleSpacing);
        Read_NumberField(root, "sampleInterval", outData.sampleInterval);
        Read_NumberField(root, "maxLength", outData.maxLength);
        Read_NumberField(root, "tailFadeLength", outData.tailFadeLength);
        Read_NumberField(root, "baseWidth", outData.baseWidth);
        Read_NumberField(root, "tilingDistance", outData.tilingDistance);
        if (const auto presetIter = root.find("previewMotionPreset"); presetIter != root.end())
            Try_ReadEnumToken(*presetIter, outData.previewMotionPreset);
        if (const auto axisIter = root.find("previewMotionAxis"); axisIter != root.end())
            Try_ReadEnumToken(*axisIter, outData.previewMotionAxis);
        Read_NumberField(root, "previewMotionAmplitude", outData.previewMotionAmplitude);
        Read_NumberField(root, "previewMotionSpeed", outData.previewMotionSpeed);
    }

    json To_Json(const MeshTypeData& data)
    {
        json root = json::object();
        root["modelGuid"] = data.modelGuid;
        root["modelPath"] = data.modelPath;
        root["assignedEffectMaterialGuid"] = data.assignedEffectMaterialGuid;
        root["assignedEffectMaterialPath"] = data.assignedEffectMaterialPath;
        root["hasAssignedMaterialInstance"] = data.hasAssignedMaterialInstance;
        if (data.hasAssignedMaterialInstance)
            root["assignedMaterial"] = To_Json(data.assignedMaterial);
        root["previewScale"] = To_Json(data.previewScale);
        root["useModelMaterials"] = data.useModelMaterials;
        return root;
    }

    void From_Json(const json& root, MeshTypeData& outData)
    {
        if (!root.is_object())
            return;

        Read_StringField(root, "modelGuid", outData.modelGuid);
        Read_StringField(root, "modelPath", outData.modelPath);
        Read_StringField(root, "assignedEffectMaterialGuid", outData.assignedEffectMaterialGuid);
        Read_StringField(root, "assignedEffectMaterialPath", outData.assignedEffectMaterialPath);
        Read_BoolField(root, "hasAssignedMaterialInstance", outData.hasAssignedMaterialInstance);
        if (const auto assignedMaterialIter = root.find("assignedMaterial");
            assignedMaterialIter != root.end() && assignedMaterialIter->is_object())
        {
            From_Json(*assignedMaterialIter, outData.assignedMaterial);
            outData.hasAssignedMaterialInstance = true;
        }
        if (const auto previewScaleIter = root.find("previewScale"); previewScaleIter != root.end())
            Read_Vec3(*previewScaleIter, outData.previewScale);
        Read_BoolField(root, "useModelMaterials", outData.useModelMaterials);
    }

    json To_Json(const AuthoringTypeData& data)
    {
        json root = json::object();
        root["kind"] = To_EnumToken(data.kind);

        switch (data.kind)
        {
        case AuthoringTypeDataKind::Trail:
        {
            const TrailTypeData* trailData = get_if<TrailTypeData>(&data.payload);
            root["data"] = To_Json(trailData != nullptr ? *trailData : TrailTypeData{});
            break;
        }
        case AuthoringTypeDataKind::Mesh:
        {
            const MeshTypeData* meshData = get_if<MeshTypeData>(&data.payload);
            root["data"] = To_Json(meshData != nullptr ? *meshData : MeshTypeData{});
            break;
        }
        case AuthoringTypeDataKind::Ribbon:
        {
            const RibbonTypeData* ribbonData = get_if<RibbonTypeData>(&data.payload);
            root["data"] = To_Json(ribbonData != nullptr ? *ribbonData : RibbonTypeData{});
            break;
        }
        case AuthoringTypeDataKind::SourceHistoryRibbon:
        {
            const SourceHistoryRibbonTypeData* sourceHistoryRibbonData = get_if<SourceHistoryRibbonTypeData>(&data.payload);
            root["data"] = To_Json(sourceHistoryRibbonData != nullptr ? *sourceHistoryRibbonData : SourceHistoryRibbonTypeData{});
            break;
        }
        case AuthoringTypeDataKind::None:
        default:
            break;
        }

        return root;
    }

    void From_Json(const json& root, AuthoringTypeData& outData)
    {
        if (!root.is_object())
            return;

        AuthoringTypeDataKind kind = AuthoringTypeDataKind::None;
        const auto kindIter = root.find("kind");
        if (kindIter == root.end() || !Try_ReadEnumToken(*kindIter, kind))
            return;

        switch (kind)
        {
        case AuthoringTypeDataKind::Trail:
        {
            TrailTypeData data{};
            const auto dataIter = root.find("data");
            if (dataIter != root.end())
                From_Json(*dataIter, data);

            outData.kind = AuthoringTypeDataKind::Trail;
            outData.payload = data;
            break;
        }
        case AuthoringTypeDataKind::Mesh:
        {
            MeshTypeData data{};
            const auto dataIter = root.find("data");
            if (dataIter != root.end())
                From_Json(*dataIter, data);

            outData.kind = AuthoringTypeDataKind::Mesh;
            outData.payload = data;
            break;
        }
        case AuthoringTypeDataKind::Ribbon:
        {
            RibbonTypeData data{};
            const auto dataIter = root.find("data");
            if (dataIter != root.end())
                From_Json(*dataIter, data);

            outData.kind = AuthoringTypeDataKind::Ribbon;
            outData.payload = data;
            break;
        }
        case AuthoringTypeDataKind::SourceHistoryRibbon:
        {
            SourceHistoryRibbonTypeData data{};
            const auto dataIter = root.find("data");
            if (dataIter != root.end())
                From_Json(*dataIter, data);

            outData.kind = AuthoringTypeDataKind::SourceHistoryRibbon;
            outData.payload = data;
            break;
        }
        case AuthoringTypeDataKind::None:
        default:
            outData.kind = AuthoringTypeDataKind::None;
            outData.payload = EmptyTypeData{};
            break;
        }
    }

    json To_Json(const EffectMaterialInstanceData& data)
    {
        json root = json::object();
        root["sourcePresetName"] = data.sourcePresetName;
        root["materialFamily"] = To_EnumToken(data.materialFamily);
        root["mainTextureGuid"] = Make_TextureGuidForSave(data.mainTextureGuid, data.mainTexturePath);
        root["mainTexturePath"] = data.mainTexturePath;
        root["noiseTextureGuid"] = Make_TextureGuidForSave(data.noiseTextureGuid, data.noiseTexturePath);
        root["noiseTexturePath"] = data.noiseTexturePath;
        root["maskTextureGuid"] = Make_TextureGuidForSave(data.maskTextureGuid, data.maskTexturePath);
        root["maskTexturePath"] = data.maskTexturePath;
        root["flowTextureGuid"] = Make_TextureGuidForSave(data.flowTextureGuid, data.flowTexturePath);
        root["flowTexturePath"] = data.flowTexturePath;
        root["tint"] = To_Json(data.tint);
        root["intensity"] = data.intensity;
        root["opacityPower"] = data.opacityPower;
        root["noiseStrength"] = data.noiseStrength;
        root["alphaCutoff"] = data.alphaCutoff;
        root["alphaErosion"] = data.alphaErosion;
        root["noiseSource"] = data.noiseSource;
        root["maskSource"] = data.maskSource;
        root["noiseInvert"] = data.noiseInvert;
        root["maskInvert"] = data.maskInvert;
        root["mainUVScale"] = To_Json(data.mainUVScale);
        root["mainUVOffset"] = To_Json(data.mainUVOffset);
        root["mainUVScrollSpeed"] = To_Json(data.mainUVScrollSpeed);
        root["mainUVTilingMode"] = To_EnumToken(data.mainUVTilingMode);
        root["noiseUVScale"] = To_Json(data.noiseUVScale);
        root["noiseUVOffset"] = To_Json(data.noiseUVOffset);
        root["noiseUVScrollSpeed"] = To_Json(data.noiseUVScrollSpeed);
        root["noiseUVTilingMode"] = To_EnumToken(data.noiseUVTilingMode);
        root["maskUVScale"] = To_Json(data.maskUVScale);
        root["maskUVOffset"] = To_Json(data.maskUVOffset);
        root["maskUVScrollSpeed"] = To_Json(data.maskUVScrollSpeed);
        root["maskUVTilingMode"] = To_EnumToken(data.maskUVTilingMode);
        root["flowUVScale"] = To_Json(data.flowUVScale);
        root["flowUVOffset"] = To_Json(data.flowUVOffset);
        root["flowUVScrollSpeed"] = To_Json(data.flowUVScrollSpeed);
        root["flowUVTilingMode"] = To_EnumToken(data.flowUVTilingMode);
        root["blendMode"] = To_EnumToken(data.blendMode);
        root["opacitySource"] = data.opacitySource;
        if (data.blendMode == EffectMaterialBlendMode::Additive || !Is_DefaultAdditiveContribution(data.additive))
            root["additive"] = To_Json(data.additive);
        if (!Is_DefaultCoreEmissive(data.coreEmissive))
            root["coreEmissive"] = To_Json(data.coreEmissive);
        root["refractionIntensity"] = data.refractionIntensity;
        root["refractionPresence"] = data.refractionPresence;
        root["distortionShapeMode"] = To_EnumToken(data.distortionShapeMode);
        root["distortionShapeRadius"] = data.distortionShapeRadius;
        root["distortionShapeThickness"] = data.distortionShapeThickness;
        root["distortionShapeSoftness"] = data.distortionShapeSoftness;
        root["twoSided"] = data.twoSided;
        root["subUVRows"] = data.subUVRows;
        root["subUVCols"] = data.subUVCols;
        return root;
    }

    void From_Json(const json& root, EffectMaterialInstanceData& outData)
    {
        if (!root.is_object())
            return;

        Read_StringField(root, "sourcePresetName", outData.sourcePresetName);
        if (const auto materialFamilyIter = root.find("materialFamily"); materialFamilyIter != root.end())
            Try_ReadEnumToken(*materialFamilyIter, outData.materialFamily);
        Read_StringField(root, "mainTextureGuid", outData.mainTextureGuid);
        Read_StringField(root, "mainTexturePath", outData.mainTexturePath);
        Read_StringField(root, "noiseTextureGuid", outData.noiseTextureGuid);
        Read_StringField(root, "noiseTexturePath", outData.noiseTexturePath);
        Read_StringField(root, "maskTextureGuid", outData.maskTextureGuid);
        Read_StringField(root, "maskTexturePath", outData.maskTexturePath);
        Read_StringField(root, "flowTextureGuid", outData.flowTextureGuid);
        Read_StringField(root, "flowTexturePath", outData.flowTexturePath);
        if (outData.mainTextureGuid.empty() && outData.mainTexturePath.empty())
        {
            string legacyMainTextureId{};
            Read_StringField(root, "mainTextureId", legacyMainTextureId);
            Try_MigrateLegacyTextureId(legacyMainTextureId, outData.mainTextureGuid, outData.mainTexturePath);
        }
        if (outData.noiseTextureGuid.empty() && outData.noiseTexturePath.empty())
        {
            string legacyNoiseTextureId{};
            Read_StringField(root, "noiseTextureId", legacyNoiseTextureId);
            Try_MigrateLegacyTextureId(legacyNoiseTextureId, outData.noiseTextureGuid, outData.noiseTexturePath);
        }
        Fill_TextureGuidFromPath(outData.mainTextureGuid, outData.mainTexturePath);
        Fill_TextureGuidFromPath(outData.noiseTextureGuid, outData.noiseTexturePath);
        Fill_TextureGuidFromPath(outData.maskTextureGuid, outData.maskTexturePath);
        Fill_TextureGuidFromPath(outData.flowTextureGuid, outData.flowTexturePath);
        Fill_TexturePathHintFromGuid(outData.mainTextureGuid, outData.mainTexturePath);
        Fill_TexturePathHintFromGuid(outData.noiseTextureGuid, outData.noiseTexturePath);
        Fill_TexturePathHintFromGuid(outData.maskTextureGuid, outData.maskTexturePath);
        Fill_TexturePathHintFromGuid(outData.flowTextureGuid, outData.flowTexturePath);
        Read_NumberField(root, "intensity", outData.intensity);
        Read_NumberField(root, "opacityPower", outData.opacityPower);
        Read_NumberField(root, "noiseStrength", outData.noiseStrength);
        Read_NumberField(root, "alphaCutoff", outData.alphaCutoff);
        Read_NumberField(root, "alphaErosion", outData.alphaErosion);
        Read_StringField(root, "noiseSource", outData.noiseSource);
        Read_StringField(root, "maskSource", outData.maskSource);
        Read_BoolField(root, "noiseInvert", outData.noiseInvert);
        Read_BoolField(root, "maskInvert", outData.maskInvert);
        if (const auto blendModeIter = root.find("blendMode"); blendModeIter != root.end())
            Try_ReadEnumToken(*blendModeIter, outData.blendMode);
        Read_StringField(root, "opacitySource", outData.opacitySource);
        if (const auto additiveIter = root.find("additive"); additiveIter != root.end())
            From_Json(*additiveIter, outData.additive);
        if (const auto coreEmissiveIter = root.find("coreEmissive"); coreEmissiveIter != root.end())
            From_Json(*coreEmissiveIter, outData.coreEmissive);
        Read_BoolField(root, "twoSided", outData.twoSided);
        Read_NumberField(root, "subUVRows", outData.subUVRows);
        Read_NumberField(root, "subUVCols", outData.subUVCols);

        if (const auto tintIter = root.find("tint"); tintIter != root.end())
            Read_Color(*tintIter, outData.tint);

        if (const auto mainUVScaleIter = root.find("mainUVScale"); mainUVScaleIter != root.end())
            Read_Vec2(*mainUVScaleIter, outData.mainUVScale);

        if (const auto mainUVOffsetIter = root.find("mainUVOffset"); mainUVOffsetIter != root.end())
            Read_Vec2(*mainUVOffsetIter, outData.mainUVOffset);

        if (const auto mainUVScrollSpeedIter = root.find("mainUVScrollSpeed"); mainUVScrollSpeedIter != root.end())
            Read_Vec2(*mainUVScrollSpeedIter, outData.mainUVScrollSpeed);

        if (const auto mainUVTilingModeIter = root.find("mainUVTilingMode"); mainUVTilingModeIter != root.end())
            Try_ReadEnumToken(*mainUVTilingModeIter, outData.mainUVTilingMode);

        if (const auto noiseUVScaleIter = root.find("noiseUVScale"); noiseUVScaleIter != root.end())
            Read_Vec2(*noiseUVScaleIter, outData.noiseUVScale);

        if (const auto noiseUVOffsetIter = root.find("noiseUVOffset"); noiseUVOffsetIter != root.end())
            Read_Vec2(*noiseUVOffsetIter, outData.noiseUVOffset);

        if (const auto noiseUVScrollSpeedIter = root.find("noiseUVScrollSpeed"); noiseUVScrollSpeedIter != root.end())
            Read_Vec2(*noiseUVScrollSpeedIter, outData.noiseUVScrollSpeed);

        if (const auto noiseUVTilingModeIter = root.find("noiseUVTilingMode"); noiseUVTilingModeIter != root.end())
            Try_ReadEnumToken(*noiseUVTilingModeIter, outData.noiseUVTilingMode);

        if (const auto maskUVScaleIter = root.find("maskUVScale"); maskUVScaleIter != root.end())
            Read_Vec2(*maskUVScaleIter, outData.maskUVScale);

        if (const auto maskUVOffsetIter = root.find("maskUVOffset"); maskUVOffsetIter != root.end())
            Read_Vec2(*maskUVOffsetIter, outData.maskUVOffset);

        if (const auto maskUVScrollSpeedIter = root.find("maskUVScrollSpeed"); maskUVScrollSpeedIter != root.end())
            Read_Vec2(*maskUVScrollSpeedIter, outData.maskUVScrollSpeed);

        if (const auto maskUVTilingModeIter = root.find("maskUVTilingMode"); maskUVTilingModeIter != root.end())
            Try_ReadEnumToken(*maskUVTilingModeIter, outData.maskUVTilingMode);

        if (const auto flowUVScaleIter = root.find("flowUVScale"); flowUVScaleIter != root.end())
            Read_Vec2(*flowUVScaleIter, outData.flowUVScale);

        if (const auto flowUVOffsetIter = root.find("flowUVOffset"); flowUVOffsetIter != root.end())
            Read_Vec2(*flowUVOffsetIter, outData.flowUVOffset);

        if (const auto flowUVScrollSpeedIter = root.find("flowUVScrollSpeed"); flowUVScrollSpeedIter != root.end())
            Read_Vec2(*flowUVScrollSpeedIter, outData.flowUVScrollSpeed);

        if (const auto flowUVTilingModeIter = root.find("flowUVTilingMode"); flowUVTilingModeIter != root.end())
            Try_ReadEnumToken(*flowUVTilingModeIter, outData.flowUVTilingMode);

        Read_NumberField(root, "refractionIntensity", outData.refractionIntensity);
        Read_NumberField(root, "refractionPresence", outData.refractionPresence);
        if (const auto shapeModeIter = root.find("distortionShapeMode"); shapeModeIter != root.end())
            Try_ReadEnumToken(*shapeModeIter, outData.distortionShapeMode);
        Read_NumberField(root, "distortionShapeRadius", outData.distortionShapeRadius);
        Read_NumberField(root, "distortionShapeThickness", outData.distortionShapeThickness);
        Read_NumberField(root, "distortionShapeSoftness", outData.distortionShapeSoftness);
    }

    json To_Json(const FloatCurveKeyData& data)
    {
        json root = json::object();
        root["time"] = data.time;
        root["value"] = data.value;
        root["arriveTangent"] = data.arriveTangent;
        root["leaveTangent"] = data.leaveTangent;
        root["interpolationMode"] = To_EnumToken(data.interpolationMode);
        return root;
    }

    void From_Json(const json& root, FloatCurveKeyData& outData)
    {
        if (!root.is_object())
            return;

        Read_NumberField(root, "time", outData.time);
        Read_NumberField(root, "value", outData.value);
        Read_NumberField(root, "arriveTangent", outData.arriveTangent);
        Read_NumberField(root, "leaveTangent", outData.leaveTangent);

        const auto interpolationIter = root.find("interpolationMode");
        if (interpolationIter != root.end())
            Try_ReadEnumToken(*interpolationIter, outData.interpolationMode);
    }

    json To_Json(const Vector2CurveKeyData& data)
    {
        json root = json::object();
        root["time"] = data.time;
        root["value"] = To_Json(data.value);
        root["arriveTangent"] = To_Json(data.arriveTangent);
        root["leaveTangent"] = To_Json(data.leaveTangent);
        root["interpolationMode"] = To_EnumToken(data.interpolationMode);
        return root;
    }

    void From_Json(const json& root, Vector2CurveKeyData& outData)
    {
        if (!root.is_object())
            return;

        Read_NumberField(root, "time", outData.time);

        const auto valueIter = root.find("value");
        if (valueIter != root.end())
            Read_Vec2(*valueIter, outData.value);

        const auto arriveTangentIter = root.find("arriveTangent");
        if (arriveTangentIter != root.end())
            Read_Vec2(*arriveTangentIter, outData.arriveTangent);

        const auto leaveTangentIter = root.find("leaveTangent");
        if (leaveTangentIter != root.end())
            Read_Vec2(*leaveTangentIter, outData.leaveTangent);

        const auto interpolationIter = root.find("interpolationMode");
        if (interpolationIter != root.end())
            Try_ReadEnumToken(*interpolationIter, outData.interpolationMode);
    }

    json To_Json(const Vector3CurveKeyData& data)
    {
        json root = json::object();
        root["time"] = data.time;
        root["value"] = To_Json(data.value);
        root["arriveTangent"] = To_Json(data.arriveTangent);
        root["leaveTangent"] = To_Json(data.leaveTangent);
        root["interpolationMode"] = To_EnumToken(data.interpolationMode);
        return root;
    }

    void From_Json(const json& root, Vector3CurveKeyData& outData)
    {
        if (!root.is_object())
            return;

        Read_NumberField(root, "time", outData.time);

        const auto valueIter = root.find("value");
        if (valueIter != root.end())
            Read_Vec3(*valueIter, outData.value);

        const auto arriveTangentIter = root.find("arriveTangent");
        if (arriveTangentIter != root.end())
            Read_Vec3(*arriveTangentIter, outData.arriveTangent);

        const auto leaveTangentIter = root.find("leaveTangent");
        if (leaveTangentIter != root.end())
            Read_Vec3(*leaveTangentIter, outData.leaveTangent);

        const auto interpolationIter = root.find("interpolationMode");
        if (interpolationIter != root.end())
            Try_ReadEnumToken(*interpolationIter, outData.interpolationMode);
    }

    json To_ColorRgbJson(const Color& value)
    {
        return json::array({ value.x, value.y, value.z });
    }

    void Read_ColorRgb(const json& node, Color& outValue)
    {
        if (!node.is_array() || node.size() < 3 || !node[0].is_number() || !node[1].is_number() || !node[2].is_number())
            return;

        outValue.x = node[0].get<float>();
        outValue.y = node[1].get<float>();
        outValue.z = node[2].get<float>();
    }

    json To_Json(const ColorCurveKeyData& data)
    {
        json root = json::object();
        root["time"] = data.time;
        root["value"] = To_ColorRgbJson(data.value);
        root["arriveTangent"] = To_ColorRgbJson(data.arriveTangent);
        root["leaveTangent"] = To_ColorRgbJson(data.leaveTangent);
        root["interpolationMode"] = To_EnumToken(data.interpolationMode);
        return root;
    }

    void From_Json(const json& root, ColorCurveKeyData& outData)
    {
        if (!root.is_object())
            return;

        Read_NumberField(root, "time", outData.time);

        const auto valueIter = root.find("value");
        if (valueIter != root.end())
            Read_ColorRgb(*valueIter, outData.value);

        const auto arriveTangentIter = root.find("arriveTangent");
        if (arriveTangentIter != root.end())
            Read_ColorRgb(*arriveTangentIter, outData.arriveTangent);

        const auto leaveTangentIter = root.find("leaveTangent");
        if (leaveTangentIter != root.end())
            Read_ColorRgb(*leaveTangentIter, outData.leaveTangent);

        const auto interpolationIter = root.find("interpolationMode");
        if (interpolationIter != root.end())
            Try_ReadEnumToken(*interpolationIter, outData.interpolationMode);
    }

    static float Evaluate_FloatCurveSegmentLinear(const FloatCurveKeyData& leftKey, const FloatCurveKeyData& rightKey, float x)
    {
        const float width = max(0.0001f, rightKey.time - leftKey.time);
        const float t = clamp((x - leftKey.time) / width, 0.f, 1.f);
        return lerp(leftKey.value, rightKey.value, t);
    }

    static float Evaluate_FloatCurveSegmentAutoClamped(const FloatCurveKeyData& leftKey, const FloatCurveKeyData& rightKey, float x)
    {
        const float width = max(0.0001f, rightKey.time - leftKey.time);
        const float t = clamp((x - leftKey.time) / width, 0.f, 1.f);
        const float m0 = leftKey.leaveTangent * width;
        const float m1 = rightKey.arriveTangent * width;
        const float t2 = t * t;
        const float t3 = t2 * t;

        return (2.f * t3 - 3.f * t2 + 1.f) * leftKey.value +
               (t3 - 2.f * t2 + t) * m0 +
               (-2.f * t3 + 3.f * t2) * rightKey.value +
               (t3 - t2) * m1;
    }

    static float Evaluate_FloatConstantCurve(const ConstantCurveFloatDistributionData& curve, float x, float fallbackValue)
    {
        if (curve.keys.empty())
            return fallbackValue;

        if (curve.keys.size() == 1)
            return curve.keys.front().value;

        const float clampedX = clamp(x, 0.f, 1.f);
        for (size_t index = 1; index < curve.keys.size(); ++index)
        {
            const FloatCurveKeyData& leftKey = curve.keys[index - 1];
            const FloatCurveKeyData& rightKey = curve.keys[index];
            if (clampedX > rightKey.time)
                continue;

            if (leftKey.interpolationMode == FloatCurveInterpolationMode::Constant)
                return leftKey.value;

            if (leftKey.interpolationMode == FloatCurveInterpolationMode::CurveAutoClamped)
                return Evaluate_FloatCurveSegmentAutoClamped(leftKey, rightKey, clampedX);

            return Evaluate_FloatCurveSegmentLinear(leftKey, rightKey, clampedX);
        }

        return curve.keys.back().value;
    }

    static Vec2 Lerp_Vec2(const Vec2& left, const Vec2& right, float t)
    {
        return Vec2{ lerp(left.x, right.x, t), lerp(left.y, right.y, t) };
    }

    //## Distribution Codec

    json To_Json(const FloatDistributionData& data)
    {
        json root = json::object();
        root["mode"] = To_EnumToken(data.mode);

        json payload = json::object();
        if (const auto* constantData = get_if<ConstantFloatDistributionData>(&data.payload))
            payload["value"] = constantData->value;
        else if (const auto* uniformData = get_if<UniformFloatDistributionData>(&data.payload))
        {
            payload["minValue"] = uniformData->minValue;
            payload["maxValue"] = uniformData->maxValue;
            Write_RandomSeedField(payload, uniformData->randomSeed);
        }
        else if (const auto* constantCurveData = get_if<ConstantCurveFloatDistributionData>(&data.payload))
        {
            payload["keys"] = json::array();
            for (const FloatCurveKeyData& key : constantCurveData->keys)
                payload["keys"].push_back(To_Json(key));
        }

        root["payload"] = move(payload);
        return root;
    }

    void From_Json(const json& root, FloatDistributionData& outData)
    {
        if (!root.is_object())
            return;

        DistributionMode parsedMode = outData.mode;
        const auto modeIter = root.find("mode");
        if (modeIter != root.end() && Try_ReadEnumToken(*modeIter, parsedMode))
        {
            if (parsedMode == DistributionMode::Constant ||
                parsedMode == DistributionMode::Uniform ||
                parsedMode == DistributionMode::ConstantCurve)
                outData.mode = parsedMode;
        }

        const auto payloadIter = root.find("payload");
        if (payloadIter == root.end() || !payloadIter->is_object())
            return;

        switch (outData.mode)
        {
        case DistributionMode::Constant:
        {
            ConstantFloatDistributionData payload{};
            if (const auto* existingPayload = get_if<ConstantFloatDistributionData>(&outData.payload))
                payload = *existingPayload;
            Read_NumberField(*payloadIter, "value", payload.value);
            outData.payload = payload;
            break;
        }
        case DistributionMode::Uniform:
        {
            UniformFloatDistributionData payload{};
            if (const auto* existingPayload = get_if<UniformFloatDistributionData>(&outData.payload))
                payload = *existingPayload;
            Read_NumberField(*payloadIter, "minValue", payload.minValue);
            Read_NumberField(*payloadIter, "maxValue", payload.maxValue);
            Read_RandomSeedField(*payloadIter, payload.randomSeed);
            outData.payload = payload;
            break;
        }
        case DistributionMode::ConstantCurve:
        {
            ConstantCurveFloatDistributionData payload{};
            const auto keysIter = payloadIter->find("keys");
            if (keysIter != payloadIter->end() && keysIter->is_array())
            {
                payload.keys.clear();
                for (const json& keyNode : *keysIter)
                {
                    FloatCurveKeyData key{};
                    From_Json(keyNode, key);
                    payload.keys.push_back(key);
                }
            }

            if (payload.keys.empty())
            {
                if (const auto* existingPayload = get_if<ConstantCurveFloatDistributionData>(&outData.payload))
                    payload.keys = existingPayload->keys;
            }

            outData.payload = payload;
            break;
        }
        default:
            break;
        }
    }

    json To_Json(const Vector2DistributionData& data)
    {
        json root = json::object();
        root["mode"] = To_EnumToken(data.mode);

        json payload = json::object();
        if (const auto* constantData = get_if<ConstantVector2DistributionData>(&data.payload))
            payload["value"] = To_Json(constantData->value);
        else if (const auto* uniformData = get_if<UniformVector2DistributionData>(&data.payload))
        {
            payload["minValue"] = To_Json(uniformData->minValue);
            payload["maxValue"] = To_Json(uniformData->maxValue);
            Write_RandomSeedField(payload, uniformData->randomSeed);
        }
        else if (const auto* constantCurveData = get_if<ConstantCurveVector2DistributionData>(&data.payload))
        {
            payload["keys"] = json::array();
            for (const Vector2CurveKeyData& key : constantCurveData->keys)
                payload["keys"].push_back(To_Json(key));
        }

        root["payload"] = move(payload);
        return root;
    }

    void From_Json(const json& root, Vector2DistributionData& outData)
    {
        if (!root.is_object())
            return;

        DistributionMode parsedMode = outData.mode;
        const auto modeIter = root.find("mode");
        if (modeIter != root.end() && Try_ReadEnumToken(*modeIter, parsedMode))
        {
            if (parsedMode == DistributionMode::Constant ||
                parsedMode == DistributionMode::Uniform ||
                parsedMode == DistributionMode::ConstantCurve)
                outData.mode = parsedMode;
        }

        const auto payloadIter = root.find("payload");
        if (payloadIter == root.end() || !payloadIter->is_object())
            return;

        switch (outData.mode)
        {
        case DistributionMode::Constant:
        {
            ConstantVector2DistributionData payload{};
            if (const auto* existingPayload = get_if<ConstantVector2DistributionData>(&outData.payload))
                payload = *existingPayload;
            const auto valueIter = payloadIter->find("value");
            if (valueIter != payloadIter->end())
                Read_Vec2(*valueIter, payload.value);
            outData.payload = payload;
            break;
        }
        case DistributionMode::Uniform:
        {
            UniformVector2DistributionData payload{};
            if (const auto* existingPayload = get_if<UniformVector2DistributionData>(&outData.payload))
                payload = *existingPayload;
            const auto minIter = payloadIter->find("minValue");
            if (minIter != payloadIter->end())
                Read_Vec2(*minIter, payload.minValue);
            const auto maxIter = payloadIter->find("maxValue");
            if (maxIter != payloadIter->end())
                Read_Vec2(*maxIter, payload.maxValue);
            Read_RandomSeedField(*payloadIter, payload.randomSeed);
            outData.payload = payload;
            break;
        }
        case DistributionMode::ConstantCurve:
        {
            ConstantCurveVector2DistributionData payload{};
            const auto keysIter = payloadIter->find("keys");
            if (keysIter != payloadIter->end() && keysIter->is_array())
            {
                payload.keys.clear();
                for (const json& keyNode : *keysIter)
                {
                    Vector2CurveKeyData key{};
                    From_Json(keyNode, key);
                    payload.keys.push_back(key);
                }
            }

            if (payload.keys.empty())
            {
                if (const auto* existingPayload = get_if<ConstantCurveVector2DistributionData>(&outData.payload))
                    payload.keys = existingPayload->keys;
            }

            outData.payload = payload;
            break;
        }
        default:
            break;
        }
    }

    json To_Json(const Vector3DistributionData& data)
    {
        json root = json::object();
        root["mode"] = To_EnumToken(data.mode);

        json payload = json::object();
        if (const auto* constantData = get_if<ConstantVector3DistributionData>(&data.payload))
            payload["value"] = To_Json(constantData->value);
        else if (const auto* uniformData = get_if<UniformVector3DistributionData>(&data.payload))
        {
            payload["minValue"] = To_Json(uniformData->minValue);
            payload["maxValue"] = To_Json(uniformData->maxValue);
            Write_RandomSeedField(payload, uniformData->randomSeed);
        }
        else if (const auto* constantCurveData = get_if<ConstantCurveVector3DistributionData>(&data.payload))
        {
            payload["keys"] = json::array();
            for (const Vector3CurveKeyData& key : constantCurveData->keys)
                payload["keys"].push_back(To_Json(key));
        }

        root["payload"] = move(payload);
        return root;
    }

    void From_Json(const json& root, Vector3DistributionData& outData)
    {
        if (!root.is_object())
            return;

        DistributionMode parsedMode = outData.mode;
        const auto modeIter = root.find("mode");
        if (modeIter != root.end() && Try_ReadEnumToken(*modeIter, parsedMode))
        {
            if (parsedMode == DistributionMode::Constant ||
                parsedMode == DistributionMode::Uniform ||
                parsedMode == DistributionMode::ConstantCurve)
                outData.mode = parsedMode;
        }

        const auto payloadIter = root.find("payload");
        if (payloadIter == root.end() || !payloadIter->is_object())
            return;

        switch (outData.mode)
        {
        case DistributionMode::Constant:
        {
            ConstantVector3DistributionData payload{};
            if (const auto* existingPayload = get_if<ConstantVector3DistributionData>(&outData.payload))
                payload = *existingPayload;
            const auto valueIter = payloadIter->find("value");
            if (valueIter != payloadIter->end())
                Read_Vec3(*valueIter, payload.value);
            outData.payload = payload;
            break;
        }
        case DistributionMode::Uniform:
        {
            UniformVector3DistributionData payload{};
            if (const auto* existingPayload = get_if<UniformVector3DistributionData>(&outData.payload))
                payload = *existingPayload;
            const auto minIter = payloadIter->find("minValue");
            if (minIter != payloadIter->end())
                Read_Vec3(*minIter, payload.minValue);
            const auto maxIter = payloadIter->find("maxValue");
            if (maxIter != payloadIter->end())
                Read_Vec3(*maxIter, payload.maxValue);
            Read_RandomSeedField(*payloadIter, payload.randomSeed);
            outData.payload = payload;
            break;
        }
        case DistributionMode::ConstantCurve:
        {
            ConstantCurveVector3DistributionData payload{};
            const auto keysIter = payloadIter->find("keys");
            if (keysIter != payloadIter->end() && keysIter->is_array())
            {
                payload.keys.clear();
                for (const json& keyNode : *keysIter)
                {
                    Vector3CurveKeyData key{};
                    From_Json(keyNode, key);
                    payload.keys.push_back(key);
                }
            }

            if (payload.keys.empty())
            {
                if (const auto* existingPayload = get_if<ConstantCurveVector3DistributionData>(&outData.payload))
                    payload.keys = existingPayload->keys;
            }

            outData.payload = payload;
            break;
        }
        default:
            break;
        }
    }

    json To_Json(const ColorDistributionData& data)
    {
        json root = json::object();
        root["mode"] = To_EnumToken(data.mode);

        json payload = json::object();
        if (const auto* constantData = get_if<ConstantColorDistributionData>(&data.payload))
            payload["value"] = To_Json(constantData->value);
        else if (const auto* uniformData = get_if<UniformColorDistributionData>(&data.payload))
        {
            payload["minValue"] = To_Json(uniformData->minValue);
            payload["maxValue"] = To_Json(uniformData->maxValue);
            Write_RandomSeedField(payload, uniformData->randomSeed);
        }
        else if (const auto* constantCurveData = get_if<ConstantCurveColorDistributionData>(&data.payload))
        {
            payload["keys"] = json::array();
            for (const ColorCurveKeyData& key : constantCurveData->keys)
                payload["keys"].push_back(To_Json(key));
        }

        root["payload"] = move(payload);
        return root;
    }

    void From_Json(const json& root, ColorDistributionData& outData)
    {
        if (!root.is_object())
            return;

        DistributionMode parsedMode = outData.mode;
        const auto modeIter = root.find("mode");
        if (modeIter != root.end() && Try_ReadEnumToken(*modeIter, parsedMode))
        {
            if (parsedMode == DistributionMode::Constant ||
                parsedMode == DistributionMode::Uniform ||
                parsedMode == DistributionMode::ConstantCurve)
                outData.mode = parsedMode;
        }

        const auto payloadIter = root.find("payload");
        if (payloadIter == root.end() || !payloadIter->is_object())
            return;

        switch (outData.mode)
        {
        case DistributionMode::Constant:
        {
            ConstantColorDistributionData payload{};
            if (const auto* existingPayload = get_if<ConstantColorDistributionData>(&outData.payload))
                payload = *existingPayload;
            const auto valueIter = payloadIter->find("value");
            if (valueIter != payloadIter->end())
                Read_Color(*valueIter, payload.value);
            outData.payload = payload;
            break;
        }
        case DistributionMode::Uniform:
        {
            UniformColorDistributionData payload{};
            if (const auto* existingPayload = get_if<UniformColorDistributionData>(&outData.payload))
                payload = *existingPayload;
            const auto minIter = payloadIter->find("minValue");
            if (minIter != payloadIter->end())
                Read_Color(*minIter, payload.minValue);
            const auto maxIter = payloadIter->find("maxValue");
            if (maxIter != payloadIter->end())
                Read_Color(*maxIter, payload.maxValue);
            Read_RandomSeedField(*payloadIter, payload.randomSeed);
            outData.payload = payload;
            break;
        }
        case DistributionMode::ConstantCurve:
        {
            ConstantCurveColorDistributionData payload{};
            const auto keysIter = payloadIter->find("keys");
            if (keysIter != payloadIter->end() && keysIter->is_array())
            {
                payload.keys.clear();
                for (const json& keyNode : *keysIter)
                {
                    ColorCurveKeyData key{};
                    From_Json(keyNode, key);
                    payload.keys.push_back(key);
                }
            }

            if (payload.keys.empty())
            {
                if (const auto* existingPayload = get_if<ConstantCurveColorDistributionData>(&outData.payload))
                    payload.keys = existingPayload->keys;
            }

            outData.payload = payload;
            break;
        }
        default:
            break;
        }
    }

    json To_Json(const RequiredModuleData& data)
    {
        json root = json::object();
        root["rendererType"] = data.rendererType;
        root["material"] = To_Json(data.material);
        root["emitterOrigin"] = To_Json(data.emitterOrigin);
        root["emitterRotationDegrees"] = To_Json(data.emitterRotationDegrees);
        root["screenAlignment"] = To_EnumToken(data.screenAlignment);
        root["directionalAlignmentMode"] = To_EnumToken(data.directionalAlignmentMode);
        root["spriteTextureAxis"] = To_EnumToken(data.spriteTextureAxis);
        root["spriteRollOffsetDegrees"] = data.spriteRollOffsetDegrees;
        root["minCameraBlendDistance"] = data.minCameraBlendDistance;
        root["maxCameraBlendDistance"] = data.maxCameraBlendDistance;
        root["useLocalSpace"] = data.useLocalSpace;
        root["sortPolicy"] = To_EnumToken(data.sortPolicy);
        root["sortLayer"] = data.sortLayer;
        root["sortBias"] = data.sortBias;
        root["cameraMotionBlurAmount"] = data.cameraMotionBlurAmount;
        root["clearExistingParticlesOnInit"] = data.clearExistingParticlesOnInit;
        root["loopCount"] = data.loopCount;
        root["duration"] = data.duration;
        root["durationLow"] = data.durationLow;
        root["useDurationRange"] = data.useDurationRange;
        root["recalculateDurationEachLoop"] = data.recalculateDurationEachLoop;
        root["delay"] = data.delay;
        root["delayLow"] = data.delayLow;
        root["useDelayRange"] = data.useDelayRange;
        root["delayFirstLoopOnly"] = data.delayFirstLoopOnly;
        root["killOnDeactivate"] = data.killOnDeactivate;
        root["killOnCompleted"] = data.killOnCompleted;
        root["useMaxDrawCount"] = data.useMaxDrawCount;
        root["maxDrawCount"] = data.maxDrawCount;
        return root;
    }

    void From_Json(const json& root, RequiredModuleData& outData)
    {
        if (!root.is_object())
            return;

        Read_StringField(root, "rendererType", outData.rendererType);
        const auto materialIter = root.find("material");
        if (materialIter != root.end())
            From_Json(*materialIter, outData.material);
        const auto originIter = root.find("emitterOrigin");
        if (originIter != root.end())
            Read_Vec3(*originIter, outData.emitterOrigin);
        const auto rotationIter = root.find("emitterRotationDegrees");
        if (rotationIter != root.end())
            Read_Vec3(*rotationIter, outData.emitterRotationDegrees);

        const auto screenAlignmentIter = root.find("screenAlignment");
        if (screenAlignmentIter != root.end())
            Try_ReadEnumToken(*screenAlignmentIter, outData.screenAlignment);
        const auto directionalAlignmentIter = root.find("directionalAlignmentMode");
        if (directionalAlignmentIter != root.end())
            Try_ReadEnumToken(*directionalAlignmentIter, outData.directionalAlignmentMode);
        const auto spriteTextureAxisIter = root.find("spriteTextureAxis");
        if (spriteTextureAxisIter != root.end())
            Try_ReadEnumToken(*spriteTextureAxisIter, outData.spriteTextureAxis);
        const auto sortPolicyIter = root.find("sortPolicy");
        if (sortPolicyIter != root.end())
            Try_ReadEnumToken(*sortPolicyIter, outData.sortPolicy);
        else
        {
            const auto sortModeIter = root.find("sortMode");
            if (sortModeIter != root.end() && Try_ReadEnumToken(*sortModeIter, outData.sortMode))
                outData.sortPolicy = Resolve_SortPolicyFromLegacyMode(outData.sortMode);
        }

        Read_NumberField(root, "spriteRollOffsetDegrees", outData.spriteRollOffsetDegrees);
        Read_NumberField(root, "minCameraBlendDistance", outData.minCameraBlendDistance);
        Read_NumberField(root, "maxCameraBlendDistance", outData.maxCameraBlendDistance);
        Read_BoolField(root, "useLocalSpace", outData.useLocalSpace);
        Read_NumberField(root, "sortLayer", outData.sortLayer);
        Read_NumberField(root, "sortBias", outData.sortBias);
        Read_NumberField(root, "cameraMotionBlurAmount", outData.cameraMotionBlurAmount);
        Read_BoolField(root, "clearExistingParticlesOnInit", outData.clearExistingParticlesOnInit);
        Read_NumberField(root, "loopCount", outData.loopCount);
        Read_NumberField(root, "duration", outData.duration);
        Read_NumberField(root, "durationLow", outData.durationLow);
        Read_BoolField(root, "useDurationRange", outData.useDurationRange);
        Read_BoolField(root, "recalculateDurationEachLoop", outData.recalculateDurationEachLoop);
        Read_NumberField(root, "delay", outData.delay);
        Read_NumberField(root, "delayLow", outData.delayLow);
        Read_BoolField(root, "useDelayRange", outData.useDelayRange);
        Read_BoolField(root, "delayFirstLoopOnly", outData.delayFirstLoopOnly);
        Read_BoolField(root, "killOnDeactivate", outData.killOnDeactivate);
        Read_BoolField(root, "killOnCompleted", outData.killOnCompleted);
        Read_BoolField(root, "useMaxDrawCount", outData.useMaxDrawCount);
        Read_NumberField(root, "maxDrawCount", outData.maxDrawCount);
    }

    json To_Json(const SpawnModuleData& data)
    {
        json root = json::object();
        root["processSpawnRate"] = data.processSpawnRate;
        root["spawnRate"] = To_Json(data.spawnRate);
        root["spawnRateScale"] = To_Json(data.spawnRateScale);
        root["processBurstList"] = data.processBurstList;
        root["burstList"] = json::array();
        for (const SpawnModuleData::ParticleBurstData& burst : data.burstList)
        {
            json burstNode = json::object();
            burstNode["time"] = burst.time;
            burstNode["count"] = burst.count;
            root["burstList"].push_back(move(burstNode));
        }
        root["burstScale"] = To_Json(data.burstScale);
        root["maxParticleCount"] = data.maxParticleCount;
        return root;
    }

    void From_Json(const json& root, SpawnModuleData& outData)
    {
        if (!root.is_object())
            return;

        Read_BoolField(root, "processSpawnRate", outData.processSpawnRate);
        const auto spawnRateIter = root.find("spawnRate");
        if (spawnRateIter != root.end())
            From_Json(*spawnRateIter, outData.spawnRate);
        const auto spawnRateScaleIter = root.find("spawnRateScale");
        if (spawnRateScaleIter != root.end())
            From_Json(*spawnRateScaleIter, outData.spawnRateScale);
        Read_BoolField(root, "processBurstList", outData.processBurstList);
        const auto burstScaleIter = root.find("burstScale");
        if (burstScaleIter != root.end())
            From_Json(*burstScaleIter, outData.burstScale);
        Read_NumberField(root, "maxParticleCount", outData.maxParticleCount);

        const auto burstListIter = root.find("burstList");
        if (burstListIter != root.end() && burstListIter->is_array())
        {
            outData.burstList.clear();
            for (const json& burstNode : *burstListIter)
            {
                if (!burstNode.is_object())
                    continue;

                SpawnModuleData::ParticleBurstData burst{};
                Read_NumberField(burstNode, "time", burst.time);
                Read_NumberField(burstNode, "count", burst.count);
                outData.burstList.push_back(burst);
            }
        }
    }

    json To_Json(const LifetimeModuleData& data)
    {
        json root = json::object();
        root["lifeTime"] = To_Json(data.lifeTime);
        return root;
    }

    void From_Json(const json& root, LifetimeModuleData& outData)
    {
        const auto iter = root.find("lifeTime");
        if (root.is_object() && iter != root.end())
            From_Json(*iter, outData.lifeTime);
        if (root.is_object())
        {
            DistributionRandomSeedData legacySeed{};
            Read_RandomSeedField(root, legacySeed);
            Apply_LegacyRandomSeedToUniform(outData.lifeTime, legacySeed);
        }
    }

    json To_Json(const InitialLocationModuleData& data)
    {
        json root = json::object();
        root["location"] = To_Json(data.location);
        return root;
    }

    void From_Json(const json& root, InitialLocationModuleData& outData)
    {
        const auto iter = root.find("location");
        if (root.is_object() && iter != root.end())
            From_Json(*iter, outData.location);
        if (root.is_object())
        {
            DistributionRandomSeedData legacySeed{};
            Read_RandomSeedField(root, legacySeed);
            Apply_LegacyRandomSeedToUniform(outData.location, legacySeed);
        }
    }

    json To_Json(const SphereLocationModuleData& data)
    {
        json root = json::object();
        root["offset"] = To_Json(data.offset);
        root["radius"] = data.radius;
        root["spawnMode"] = To_EnumToken(data.spawnMode);
        root["placementMode"] = To_EnumToken(data.placementMode);
        Write_RandomSeedField(root, data.randomSeed);
        return root;
    }

    void From_Json(const json& root, SphereLocationModuleData& outData)
    {
        if (!root.is_object())
            return;

        const auto offsetIter = root.find("offset");
        if (offsetIter != root.end())
            Read_Vec3(*offsetIter, outData.offset);
        Read_NumberField(root, "radius", outData.radius);
        const auto spawnModeIter = root.find("spawnMode");
        if (spawnModeIter != root.end())
            Try_ReadEnumToken(*spawnModeIter, outData.spawnMode);
        const auto placementModeIter = root.find("placementMode");
        if (placementModeIter != root.end())
            Try_ReadEnumToken(*placementModeIter, outData.placementMode);
        Read_RandomSeedField(root, outData.randomSeed);
    }

    void Read_FloatDistributionOrLegacyRangeField(
        const json& root,
        const char* distributionKey,
        const char* legacyRangeKey,
        FloatDistributionData& outData)
    {
        if (const auto distributionIter = root.find(distributionKey);
            distributionIter != root.end() && distributionIter->is_object())
        {
            From_Json(*distributionIter, outData);
            return;
        }

        if (const auto legacyIter = root.find(legacyRangeKey); legacyIter != root.end())
        {
            Vec2 legacyRange{};
            Read_Vec2(*legacyIter, legacyRange);
            outData = legacyRange.x == legacyRange.y
                      ? FloatDistributionData::Make_Constant(legacyRange.x)
                      : FloatDistributionData::Make_Uniform(legacyRange.x, legacyRange.y);
        }
    }

    json To_Json(const PlaneRadialLocationModuleData& data)
    {
        json root = json::object();
        root["enabled"] = data.enabled;
        root["plane"] = To_EnumToken(data.plane);
        root["shape"] = To_EnumToken(data.shape);
        root["placementMode"] = To_EnumToken(data.placementMode);
        root["offset"] = To_Json(data.offset);
        root["uDistribution"] = To_Json(data.uDistribution);
        root["vDistribution"] = To_Json(data.vDistribution);
        root["radiusDistribution"] = To_Json(data.radiusDistribution);
        root["angleDegreesDistribution"] = To_Json(data.angleDegreesDistribution);
        root["thickness"] = data.thickness;
        return root;
    }

    void From_Json(const json& root, PlaneRadialLocationModuleData& outData)
    {
        if (!root.is_object())
            return;

        Read_BoolField(root, "enabled", outData.enabled);
        const auto planeIter = root.find("plane");
        if (planeIter != root.end())
            Try_ReadEnumToken(*planeIter, outData.plane);
        const auto shapeIter = root.find("shape");
        if (shapeIter != root.end())
            Try_ReadEnumToken(*shapeIter, outData.shape);
        const auto placementModeIter = root.find("placementMode");
        if (placementModeIter != root.end())
            Try_ReadEnumToken(*placementModeIter, outData.placementMode);

        const auto offsetIter = root.find("offset");
        if (offsetIter != root.end())
            Read_Vec3(*offsetIter, outData.offset);
        Read_FloatDistributionOrLegacyRangeField(root, "uDistribution", "rangeU", outData.uDistribution);
        Read_FloatDistributionOrLegacyRangeField(root, "vDistribution", "rangeV", outData.vDistribution);
        Read_FloatDistributionOrLegacyRangeField(root, "radiusDistribution", "radiusRange", outData.radiusDistribution);
        Read_FloatDistributionOrLegacyRangeField(root, "angleDegreesDistribution", "angleDegreesRange", outData.angleDegreesDistribution);
        Read_NumberField(root, "thickness", outData.thickness);
        DistributionRandomSeedData legacySeed{};
        Read_RandomSeedField(root, legacySeed);
        Apply_LegacyRandomSeedToUniform(outData.uDistribution, legacySeed);
        Apply_LegacyRandomSeedToUniform(outData.vDistribution, legacySeed);
        Apply_LegacyRandomSeedToUniform(outData.radiusDistribution, legacySeed);
        Apply_LegacyRandomSeedToUniform(outData.angleDegreesDistribution, legacySeed);
    }

    json To_Json(const PlaneRadialOrientationModuleData& data)
    {
        json root = json::object();
        root["enabled"] = data.enabled;
        root["targetKind"] = To_EnumToken(data.targetKind);
        root["orientationMode"] = To_EnumToken(data.orientationMode);
        root["meshForwardAxis"] = To_EnumToken(data.meshForwardAxis);
        root["meshUpAxis"] = To_EnumToken(data.meshUpAxis);
        root["tiltDegrees"] = data.tiltDegrees;
        root["rollOffsetDegrees"] = data.rollOffsetDegrees;
        return root;
    }

    void From_Json(const json& root, PlaneRadialOrientationModuleData& outData)
    {
        if (!root.is_object())
            return;

        Read_BoolField(root, "enabled", outData.enabled);
        const auto targetKindIter = root.find("targetKind");
        if (targetKindIter != root.end())
            Try_ReadEnumToken(*targetKindIter, outData.targetKind);
        const auto orientationModeIter = root.find("orientationMode");
        if (orientationModeIter != root.end())
            Try_ReadEnumToken(*orientationModeIter, outData.orientationMode);
        const auto meshForwardAxisIter = root.find("meshForwardAxis");
        if (meshForwardAxisIter != root.end())
            Try_ReadEnumToken(*meshForwardAxisIter, outData.meshForwardAxis);
        const auto meshUpAxisIter = root.find("meshUpAxis");
        if (meshUpAxisIter != root.end())
            Try_ReadEnumToken(*meshUpAxisIter, outData.meshUpAxis);
        Read_NumberField(root, "tiltDegrees", outData.tiltDegrees);
        Read_NumberField(root, "rollOffsetDegrees", outData.rollOffsetDegrees);
    }

    json To_Json(const SphereRadialOrientationModuleData& data)
    {
        json root = json::object();
        root["enabled"] = data.enabled;
        root["orientationMode"] = To_EnumToken(data.orientationMode);
        root["meshForwardAxis"] = To_EnumToken(data.meshForwardAxis);
        root["meshUpAxis"] = To_EnumToken(data.meshUpAxis);
        root["tiltDegrees"] = data.tiltDegrees;
        root["rollOffsetDegrees"] = data.rollOffsetDegrees;
        return root;
    }

    void From_Json(const json& root, SphereRadialOrientationModuleData& outData)
    {
        if (!root.is_object())
            return;

        Read_BoolField(root, "enabled", outData.enabled);
        const auto orientationModeIter = root.find("orientationMode");
        if (orientationModeIter != root.end())
            Try_ReadEnumToken(*orientationModeIter, outData.orientationMode);
        const auto meshForwardAxisIter = root.find("meshForwardAxis");
        if (meshForwardAxisIter != root.end())
            Try_ReadEnumToken(*meshForwardAxisIter, outData.meshForwardAxis);
        const auto meshUpAxisIter = root.find("meshUpAxis");
        if (meshUpAxisIter != root.end())
            Try_ReadEnumToken(*meshUpAxisIter, outData.meshUpAxis);
        Read_NumberField(root, "tiltDegrees", outData.tiltDegrees);
        Read_NumberField(root, "rollOffsetDegrees", outData.rollOffsetDegrees);
    }

    json To_Json(const InitialSizeModuleData& data)
    {
        json root = json::object();
        root["size"] = To_Json(data.size);
        return root;
    }

    void From_Json(const json& root, InitialSizeModuleData& outData)
    {
        const auto iter = root.find("size");
        if (root.is_object() && iter != root.end())
            From_Json(*iter, outData.size);
    }

    json To_Json(const InitialMeshSizeModuleData& data)
    {
        json root = json::object();
        root["size"] = To_Json(data.size);
        return root;
    }

    void From_Json(const json& root, InitialMeshSizeModuleData& outData)
    {
        const auto iter = root.find("size");
        if (root.is_object() && iter != root.end())
            From_Json(*iter, outData.size);
    }

    json To_Json(const InitialVelocityModuleData& data)
    {
        json root = json::object();
        root["velocity"] = To_Json(data.velocity);
        root["inWorldSpace"] = data.inWorldSpace;
        return root;
    }

    void From_Json(const json& root, InitialVelocityModuleData& outData)
    {
        if (!root.is_object())
            return;

        const auto velocityIter = root.find("velocity");
        if (velocityIter != root.end())
            From_Json(*velocityIter, outData.velocity);
        Read_BoolField(root, "inWorldSpace", outData.inWorldSpace);
        DistributionRandomSeedData legacySeed{};
        Read_RandomSeedField(root, legacySeed);
        Apply_LegacyRandomSeedToUniform(outData.velocity, legacySeed);
    }

    json To_Json(const InitialRadialVelocityModuleData& data)
    {
        json root = json::object();
        root["speed"] = To_Json(data.speed);
        root["radialPivot"] = To_Json(data.radialPivot);
        root["centerDirectionMode"] = To_EnumToken(data.centerDirectionMode);
        root["inWorldSpace"] = data.inWorldSpace;
        return root;
    }

    void From_Json(const json& root, InitialRadialVelocityModuleData& outData)
    {
        if (!root.is_object())
            return;

        const auto speedIter = root.find("speed");
        if (speedIter != root.end())
            From_Json(*speedIter, outData.speed);
        const auto radialPivotIter = root.find("radialPivot");
        if (radialPivotIter != root.end())
            Read_Vec3(*radialPivotIter, outData.radialPivot);
        const auto centerDirectionModeIter = root.find("centerDirectionMode");
        if (centerDirectionModeIter != root.end())
            Try_ReadEnumToken(*centerDirectionModeIter, outData.centerDirectionMode);
        Read_BoolField(root, "inWorldSpace", outData.inWorldSpace);
        DistributionRandomSeedData legacySeed{};
        Read_RandomSeedField(root, legacySeed);
        Apply_LegacyRandomSeedToUniform(outData.speed, legacySeed);
    }

    json To_Json(const VelocityConeModuleData& data)
    {
        json root = json::object();
        root["axis"] = To_Json(data.axis);
        root["angleDegrees"] = data.angleDegrees;
        root["speed"] = To_Json(data.speed);
        root["inWorldSpace"] = data.inWorldSpace;
        return root;
    }

    void From_Json(const json& root, VelocityConeModuleData& outData)
    {
        if (!root.is_object())
            return;

        const auto axisIter = root.find("axis");
        if (axisIter != root.end())
            Read_Vec3(*axisIter, outData.axis);
        Read_NumberField(root, "angleDegrees", outData.angleDegrees);
        const auto speedIter = root.find("speed");
        if (speedIter != root.end())
            From_Json(*speedIter, outData.speed);
        Read_BoolField(root, "inWorldSpace", outData.inWorldSpace);
        DistributionRandomSeedData legacySeed{};
        Read_RandomSeedField(root, legacySeed);
        Apply_LegacyRandomSeedToUniform(outData.speed, legacySeed);
    }

    json To_Json(const AccelerationModuleData& data)
    {
        json root = json::object();
        root["acceleration"] = To_Json(data.acceleration);
        return root;
    }

    void From_Json(const json& root, AccelerationModuleData& outData)
    {
        const auto iter = root.find("acceleration");
        if (root.is_object() && iter != root.end())
            From_Json(*iter, outData.acceleration);
        if (root.is_object())
        {
            DistributionRandomSeedData legacySeed{};
            Read_RandomSeedField(root, legacySeed);
            Apply_LegacyRandomSeedToUniform(outData.acceleration, legacySeed);
        }
    }

    json To_Json(const DragModuleData& data)
    {
        json root = json::object();
        root["drag"] = To_Json(data.drag);
        return root;
    }

    void From_Json(const json& root, DragModuleData& outData)
    {
        const auto iter = root.find("drag");
        if (root.is_object() && iter != root.end())
            From_Json(*iter, outData.drag);
        if (root.is_object())
        {
            DistributionRandomSeedData legacySeed{};
            Read_RandomSeedField(root, legacySeed);
            Apply_LegacyRandomSeedToUniform(outData.drag, legacySeed);
        }
    }

    json To_Json(const VelocityOverLifeModuleData& data)
    {
        json root = json::object();
        root["scaleOverLife"] = To_Json(data.scaleOverLife);
        return root;
    }

    void From_Json(const json& root, VelocityOverLifeModuleData& outData)
    {
        if (!root.is_object())
            return;

        const auto scaleOverLifeIter = root.find("scaleOverLife");
        if (scaleOverLifeIter != root.end())
        {
            From_Json(*scaleOverLifeIter, outData.scaleOverLife);
            return;
        }

        float scaleStart{ 1.f };
        float scaleEnd{ 1.f };
        Read_NumberField(root, "scaleStart", scaleStart);
        Read_NumberField(root, "scaleEnd", scaleEnd);
        outData.scaleOverLife = FloatDistributionData::Make_ConstantCurve(scaleStart, scaleEnd);
    }

    json To_Json(const OrbitOverLifeModuleData& data)
    {
        json root = json::object();
        root["enabled"] = data.enabled;
        root["pivotMode"] = To_EnumToken(data.pivotMode);
        root["plane"] = To_EnumToken(data.plane);
        root["angleDegreesOverLife"] = To_Json(data.angleDegreesOverLife);
        root["radiusScaleOverLife"] = To_Json(data.radiusScaleOverLife);
        root["orientationMode"] = To_EnumToken(data.orientationMode);
        return root;
    }

    void From_Json(const json& root, OrbitOverLifeModuleData& outData)
    {
        if (!root.is_object())
            return;

        Read_BoolField(root, "enabled", outData.enabled);
        const auto pivotModeIter = root.find("pivotMode");
        if (pivotModeIter != root.end())
            Try_ReadEnumToken(*pivotModeIter, outData.pivotMode);
        const auto planeIter = root.find("plane");
        if (planeIter != root.end())
            Try_ReadEnumToken(*planeIter, outData.plane);
        const auto angleIter = root.find("angleDegreesOverLife");
        if (angleIter != root.end())
            From_Json(*angleIter, outData.angleDegreesOverLife);
        const auto radiusIter = root.find("radiusScaleOverLife");
        if (radiusIter != root.end())
            From_Json(*radiusIter, outData.radiusScaleOverLife);
        const auto orientationIter = root.find("orientationMode");
        if (orientationIter != root.end())
            Try_ReadEnumToken(*orientationIter, outData.orientationMode);
    }

    json To_Json(const InitialRotationModuleData& data)
    {
        json root = json::object();
        root["rotationDegrees"] = To_Json(data.rotationDegrees);
        return root;
    }

    void From_Json(const json& root, InitialRotationModuleData& outData)
    {
        const auto iter = root.find("rotationDegrees");
        if (root.is_object() && iter != root.end())
            From_Json(*iter, outData.rotationDegrees);
        if (root.is_object())
        {
            DistributionRandomSeedData legacySeed{};
            Read_RandomSeedField(root, legacySeed);
            Apply_LegacyRandomSeedToUniform(outData.rotationDegrees, legacySeed);
        }
    }

    json To_Json(const RotationOverLifeModuleData& data)
    {
        json root = json::object();
        root["rotationOverLife"] = To_Json(data.rotationOverLife);
        return root;
    }

    void From_Json(const json& root, RotationOverLifeModuleData& outData)
    {
        if (!root.is_object())
            return;

        const auto rotationOverLifeIter = root.find("rotationOverLife");
        if (rotationOverLifeIter != root.end())
            From_Json(*rotationOverLifeIter, outData.rotationOverLife);
        else
        {
            float rotationStartDegrees{ 0.f };
            float rotationEndDegrees{ 0.f };
            Read_NumberField(root, "rotationStartDegrees", rotationStartDegrees);
            Read_NumberField(root, "rotationEndDegrees", rotationEndDegrees);
            outData.rotationOverLife = FloatDistributionData::Make_ConstantCurve(rotationStartDegrees, rotationEndDegrees);
        }
    }

    json To_Json(const InitialRotationRateModuleData& data)
    {
        json root = json::object();
        root["rotationRateDegrees"] = To_Json(data.rotationRateDegrees);
        return root;
    }

    void From_Json(const json& root, InitialRotationRateModuleData& outData)
    {
        const auto iter = root.find("rotationRateDegrees");
        if (root.is_object() && iter != root.end())
            From_Json(*iter, outData.rotationRateDegrees);
        if (root.is_object())
        {
            DistributionRandomSeedData legacySeed{};
            Read_RandomSeedField(root, legacySeed);
            Apply_LegacyRandomSeedToUniform(outData.rotationRateDegrees, legacySeed);
        }
    }

    json To_Json(const RotationRateScaleByLifeModuleData& data)
    {
        json root = json::object();
        root["scaleOverLife"] = To_Json(data.scaleOverLife);
        return root;
    }

    void From_Json(const json& root, RotationRateScaleByLifeModuleData& outData)
    {
        if (!root.is_object())
            return;

        const auto scaleOverLifeIter = root.find("scaleOverLife");
        if (scaleOverLifeIter != root.end())
            From_Json(*scaleOverLifeIter, outData.scaleOverLife);
        else
        {
            float scaleStart{ 1.f };
            float scaleEnd{ 1.f };
            Read_NumberField(root, "scaleStart", scaleStart);
            Read_NumberField(root, "scaleEnd", scaleEnd);
            outData.scaleOverLife = FloatDistributionData::Make_ConstantCurve(scaleStart, scaleEnd);
        }
    }

    json To_Json(const InitialMeshRotationModuleData& data)
    {
        json root = json::object();
        root["rotationDegrees"] = To_Json(data.rotationDegrees);
        return root;
    }

    void From_Json(const json& root, InitialMeshRotationModuleData& outData)
    {
        const auto iter = root.find("rotationDegrees");
        if (root.is_object() && iter != root.end())
            From_Json(*iter, outData.rotationDegrees);
    }

    json To_Json(const MeshRotationOverLifeModuleData& data)
    {
        json root = json::object();
        root["rotationOverLife"] = To_Json(data.rotationOverLife);
        return root;
    }

    void From_Json(const json& root, MeshRotationOverLifeModuleData& outData)
    {
        const auto iter = root.find("rotationOverLife");
        if (root.is_object() && iter != root.end())
            From_Json(*iter, outData.rotationOverLife);
    }

    json To_Json(const InitialMeshRotationRateModuleData& data)
    {
        json root = json::object();
        root["rotationRateDegrees"] = To_Json(data.rotationRateDegrees);
        return root;
    }

    void From_Json(const json& root, InitialMeshRotationRateModuleData& outData)
    {
        const auto iter = root.find("rotationRateDegrees");
        if (root.is_object() && iter != root.end())
            From_Json(*iter, outData.rotationRateDegrees);
    }

    json To_Json(const MeshRotationRateScaleByLifeModuleData& data)
    {
        json root = json::object();
        root["scaleOverLife"] = To_Json(data.scaleOverLife);
        return root;
    }

    void From_Json(const json& root, MeshRotationRateScaleByLifeModuleData& outData)
    {
        const auto iter = root.find("scaleOverLife");
        if (root.is_object() && iter != root.end())
            From_Json(*iter, outData.scaleOverLife);
    }

    json To_Json(const InitialColorModuleData& data)
    {
        json root = json::object();
        root["color"] = To_Json(data.color);
        root["alpha"] = To_Json(data.alpha);
        return root;
    }

    void From_Json(const json& root, InitialColorModuleData& outData)
    {
        if (!root.is_object())
            return;

        const auto colorIter = root.find("color");
        if (colorIter != root.end())
            From_Json(*colorIter, outData.color);
        const auto alphaIter = root.find("alpha");
        if (alphaIter != root.end())
            From_Json(*alphaIter, outData.alpha);
    }

    json To_Json(const ColorOverLifeModuleData& data)
    {
        json root = json::object();
        root["colorOverLife"] = To_Json(data.colorOverLife);
        root["alphaOverLife"] = To_Json(data.alphaOverLife);
        return root;
    }

    void From_Json(const json& root, ColorOverLifeModuleData& outData)
    {
        if (!root.is_object())
            return;

        const auto colorIter = root.find("colorOverLife");
        if (colorIter != root.end())
            From_Json(*colorIter, outData.colorOverLife);
        const auto alphaIter = root.find("alphaOverLife");
        if (alphaIter != root.end())
            From_Json(*alphaIter, outData.alphaOverLife);
    }

    json To_Json(const SubUVFrameOverLifeModuleData& data)
    {
        json root = json::object();
        root["frameIndex"] = To_Json(data.frameIndex);
        root["startFrame"] = data.startFrame;
        root["endFrame"] = data.endFrame;
        root["loop"] = data.loop;
        root["playbackMode"] = To_EnumToken(data.playbackMode);
        root["framesPerSecond"] = data.framesPerSecond;
        Write_RandomSeedField(root, data.randomSeed);
        return root;
    }

    void From_Json(const json& root, SubUVFrameOverLifeModuleData& outData)
    {
        if (!root.is_object())
            return;

        const auto frameIndexIter = root.find("frameIndex");
        if (frameIndexIter != root.end())
            From_Json(*frameIndexIter, outData.frameIndex);
        Read_NumberField(root, "startFrame", outData.startFrame);
        Read_NumberField(root, "endFrame", outData.endFrame);
        Read_BoolField(root, "loop", outData.loop);
        const auto playbackModeIter = root.find("playbackMode");
        if (playbackModeIter != root.end())
            Try_ReadEnumToken(*playbackModeIter, outData.playbackMode);
        Read_NumberField(root, "framesPerSecond", outData.framesPerSecond);
        Read_RandomSeedField(root, outData.randomSeed);
    }

    //## Module Data Codec

    json To_Json(const SizeByLifeModuleData& data)
    {
        json root = json::object();
        root["scaleOverLife"] = To_Json(data.scaleOverLife);
        root["multiplyX"] = data.multiplyX;
        root["multiplyY"] = data.multiplyY;
        root["axisLock"] = To_EnumToken(data.axisLock);
        return root;
    }

    void From_Json(const json& root, SizeByLifeModuleData& outData)
    {
        if (!root.is_object())
            return;

        Read_BoolField(root, "multiplyX", outData.multiplyX);
        Read_BoolField(root, "multiplyY", outData.multiplyY);

        const auto axisLockIter = root.find("axisLock");
        if (axisLockIter != root.end())
            Try_ReadEnumToken(*axisLockIter, outData.axisLock);

        const auto scaleOverLifeIter = root.find("scaleOverLife");
        if (scaleOverLifeIter != root.end())
            From_Json(*scaleOverLifeIter, outData.scaleOverLife);
        else
        {
            float multiplyXStart{ 1.f };
            float multiplyXEnd{ 1.f };
            float multiplyYStart{ 1.f };
            float multiplyYEnd{ 1.f };
            Read_NumberField(root, "multiplyXStart", multiplyXStart);
            Read_NumberField(root, "multiplyXEnd", multiplyXEnd);
            Read_NumberField(root, "multiplyYStart", multiplyYStart);
            Read_NumberField(root, "multiplyYEnd", multiplyYEnd);

            const auto responseCurveIter = root.find("responseCurve");
            if (responseCurveIter == root.end())
            {
                outData.scaleOverLife = Vector2DistributionData::Make_ConstantCurve(
                    Vec2{ multiplyXStart, multiplyYStart },
                    Vec2{ multiplyXEnd, multiplyYEnd }
                );
            }
            else
            {
                FloatDistributionData responseCurve{ FloatDistributionData::Make_ConstantCurve(0.f, 1.f) };
                From_Json(*responseCurveIter, responseCurve);

                ConstantCurveVector2DistributionData bakedCurve{};
                bakedCurve.keys.clear();

                const auto* curve = get_if<ConstantCurveFloatDistributionData>(&responseCurve.payload);
                if (responseCurve.mode == DistributionMode::ConstantCurve && curve != nullptr && !curve->keys.empty())
                {
                    for (const FloatCurveKeyData& responseKey : curve->keys)
                    {
                        const float ratio = Evaluate_FloatConstantCurve(*curve, responseKey.time, responseKey.value);
                        bakedCurve.keys.push_back(
                            Vector2CurveKeyData{
                                responseKey.time,
                                Lerp_Vec2(Vec2{ multiplyXStart, multiplyYStart }, Vec2{ multiplyXEnd, multiplyYEnd }, ratio),
                                Vec2{},
                                Vec2{},
                                responseKey.interpolationMode
                            }
                        );
                    }
                }

                if (bakedCurve.keys.empty())
                {
                    bakedCurve.keys = get<ConstantCurveVector2DistributionData>(
                        Vector2DistributionData::Make_ConstantCurve(
                            Vec2{ multiplyXStart, multiplyYStart },
                            Vec2{ multiplyXEnd, multiplyYEnd }
                        ).payload
                    ).keys;
                }

                outData.scaleOverLife.mode = DistributionMode::ConstantCurve;
                outData.scaleOverLife.payload = bakedCurve;
            }
        }
    }

    json To_Json(const MeshSizeByLifeModuleData& data)
    {
        json root = json::object();
        root["scaleOverLife"] = To_Json(data.scaleOverLife);
        root["multiplyX"] = data.multiplyX;
        root["multiplyY"] = data.multiplyY;
        root["multiplyZ"] = data.multiplyZ;
        return root;
    }

    void From_Json(const json& root, MeshSizeByLifeModuleData& outData)
    {
        if (!root.is_object())
            return;

        Read_BoolField(root, "multiplyX", outData.multiplyX);
        Read_BoolField(root, "multiplyY", outData.multiplyY);
        Read_BoolField(root, "multiplyZ", outData.multiplyZ);

        const auto scaleOverLifeIter = root.find("scaleOverLife");
        if (scaleOverLifeIter != root.end())
            From_Json(*scaleOverLifeIter, outData.scaleOverLife);
    }

    json To_Json(const SpawnPerUnitModuleData& data)
    {
        json root = json::object();
        root["enabled"] = data.enabled;
        root["spawnPerUnit"] = To_Json(data.spawnPerUnit);
        root["unitScalar"] = data.unitScalar;
        root["movementTolerance"] = data.movementTolerance;
        root["maxFrameDistance"] = data.maxFrameDistance;
        return root;
    }

    void From_Json(const json& root, SpawnPerUnitModuleData& outData)
    {
        if (!root.is_object())
            return;

        Read_BoolField(root, "enabled", outData.enabled);

        const auto spawnPerUnitIter = root.find("spawnPerUnit");
        if (spawnPerUnitIter != root.end())
            From_Json(*spawnPerUnitIter, outData.spawnPerUnit);

        Read_NumberField(root, "unitScalar", outData.unitScalar);
        Read_NumberField(root, "movementTolerance", outData.movementTolerance);
        Read_NumberField(root, "maxFrameDistance", outData.maxFrameDistance);

        if (spawnPerUnitIter == root.end())
        {
            float legacyUnitDistance = 0.f;
            Read_NumberField(root, "unitDistance", legacyUnitDistance);
            if (legacyUnitDistance > 0.f)
                outData.spawnPerUnit = FloatDistributionData::Make_Constant(max(0.0001f, outData.unitScalar) / legacyUnitDistance);
        }
    }

    json To_Json(const MaterialScalarModulatorData& data)
    {
        json root = json::object();
        root["enabled"] = data.enabled;
        root["targetField"] = To_EnumToken(data.targetField);
        root["operation"] = To_EnumToken(data.operation);
        root["timeSource"] = To_EnumToken(data.timeSource);
        root["distribution"] = To_Json(data.distribution);
        return root;
    }

    void From_Json(const json& root, MaterialScalarModulatorData& outData)
    {
        if (!root.is_object())
            return;

        Read_BoolField(root, "enabled", outData.enabled);

        const auto targetIter = root.find("targetField");
        if (targetIter != root.end())
            Try_ReadEnumToken(*targetIter, outData.targetField);

        const auto operationIter = root.find("operation");
        if (operationIter != root.end())
            Try_ReadEnumToken(*operationIter, outData.operation);

        const auto timeSourceIter = root.find("timeSource");
        if (timeSourceIter != root.end())
            Try_ReadEnumToken(*timeSourceIter, outData.timeSource);

        const auto distributionIter = root.find("distribution");
        if (distributionIter != root.end())
            From_Json(*distributionIter, outData.distribution);
    }

    json To_Json(const MaterialCoreColorRgbModulatorData& data)
    {
        json root = json::object();
        root["enabled"] = data.enabled;
        root["timeSource"] = To_EnumToken(data.timeSource);
        root["distribution"] = To_Json(data.distribution);
        return root;
    }

    void From_Json(const json& root, MaterialCoreColorRgbModulatorData& outData)
    {
        if (!root.is_object())
            return;

        Read_BoolField(root, "enabled", outData.enabled);

        const auto timeSourceIter = root.find("timeSource");
        if (timeSourceIter != root.end())
            Try_ReadEnumToken(*timeSourceIter, outData.timeSource);

        const auto distributionIter = root.find("distribution");
        if (distributionIter != root.end())
            From_Json(*distributionIter, outData.distribution);
    }

    json To_Json(const MaterialScalarModulationModuleData& data)
    {
        json root = json::object();
        root["modulators"] = json::array();
        for (const MaterialScalarModulatorData& modulator : data.modulators)
            root["modulators"].push_back(To_Json(modulator));

        root["coreColorRgbModulators"] = json::array();
        for (const MaterialCoreColorRgbModulatorData& modulator : data.coreColorRgbModulators)
            root["coreColorRgbModulators"].push_back(To_Json(modulator));
        return root;
    }

    void From_Json(const json& root, MaterialScalarModulationModuleData& outData)
    {
        if (!root.is_object())
            return;

        const auto modulatorsIter = root.find("modulators");
        if (modulatorsIter != root.end() && modulatorsIter->is_array())
        {
            outData.modulators.clear();
            for (const json& modulatorNode : *modulatorsIter)
            {
                MaterialScalarModulatorData modulator{};
                From_Json(modulatorNode, modulator);
                outData.modulators.push_back(modulator);
            }
        }

        const auto coreColorRgbModulatorsIter = root.find("coreColorRgbModulators");
        if (coreColorRgbModulatorsIter != root.end() && coreColorRgbModulatorsIter->is_array())
        {
            outData.coreColorRgbModulators.clear();
            for (const json& modulatorNode : *coreColorRgbModulatorsIter)
            {
                MaterialCoreColorRgbModulatorData modulator{};
                From_Json(modulatorNode, modulator);
                outData.coreColorRgbModulators.push_back(modulator);
            }
        }
    }
}

json EffectAuthoringJsonSerializer::To_Json(const EffectAuthoringDocument& document)
{
    json root = json::object();
    root["version"] = kEffectAuthoringJsonVersion;
    root["name"] = document.name;
    root["particleSystem"] = Internal::To_Json(document.particleSystemData);
    root["emitters"] = json::array();

    for (const AuthoringEmitter& emitter : document.emitters)
        root["emitters"].push_back(To_Json(emitter));

    return root;
}

bool EffectAuthoringJsonSerializer::From_Json(const json& root, EffectAuthoringDocument& outDocument)
{
    if (!root.is_object())
        return false;

    EffectAuthoringDocument document{};
    document.name = root.value("name", string{});

    const auto particleSystemIter = root.find("particleSystem");
    if (particleSystemIter != root.end())
        Internal::From_Json(*particleSystemIter, document.particleSystemData);

    const auto emittersIter = root.find("emitters");
    if (emittersIter == root.end() || !emittersIter->is_array())
        return false;

    for (const json& emitterNode : *emittersIter)
    {
        AuthoringEmitter emitter{};
        if (From_Json(emitterNode, emitter))
            document.emitters.push_back(move(emitter));
    }

    outDocument = move(document);
    return true;
}

uint32 EffectAuthoringJsonSerializer::Find_NextAuthoringId(const EffectAuthoringDocument& document)
{
    return Find_NextAuthoringId(document.emitters);
}

json EffectAuthoringJsonSerializer::To_Json(const AuthoringModule& module)
{
    json root = json::object();
    root["id"] = module.id;
    root["type"] = Internal::To_EnumToken(module.type);
    root["enabled"] = module.enabled;

    switch (module.type)
    {
    case AuthoringModuleType::Required:
        root["data"] = Internal::To_Json(get<RequiredModuleData>(module.data));
        break;
    case AuthoringModuleType::Spawn:
        root["data"] = Internal::To_Json(get<SpawnModuleData>(module.data));
        break;
    case AuthoringModuleType::Lifetime:
        root["data"] = Internal::To_Json(get<LifetimeModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialLocation:
        root["data"] = Internal::To_Json(get<InitialLocationModuleData>(module.data));
        break;
    case AuthoringModuleType::SphereLocation:
        root["data"] = Internal::To_Json(get<SphereLocationModuleData>(module.data));
        break;
    case AuthoringModuleType::PlaneRadialLocation:
        root["data"] = Internal::To_Json(get<PlaneRadialLocationModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialSize:
        root["data"] = Internal::To_Json(get<InitialSizeModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialMeshSize:
        root["data"] = Internal::To_Json(get<InitialMeshSizeModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialVelocity:
        root["data"] = Internal::To_Json(get<InitialVelocityModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialRadialVelocity:
        root["data"] = Internal::To_Json(get<InitialRadialVelocityModuleData>(module.data));
        break;
    case AuthoringModuleType::VelocityCone:
        root["data"] = Internal::To_Json(get<VelocityConeModuleData>(module.data));
        break;
    case AuthoringModuleType::Acceleration:
        root["data"] = Internal::To_Json(get<AccelerationModuleData>(module.data));
        break;
    case AuthoringModuleType::Drag:
        root["data"] = Internal::To_Json(get<DragModuleData>(module.data));
        break;
    case AuthoringModuleType::VelocityOverLife:
        root["data"] = Internal::To_Json(get<VelocityOverLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::OrbitOverLife:
        root["data"] = Internal::To_Json(get<OrbitOverLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialRotation:
        root["data"] = Internal::To_Json(get<InitialRotationModuleData>(module.data));
        break;
    case AuthoringModuleType::SphereRadialOrientation:
        root["data"] = Internal::To_Json(get<SphereRadialOrientationModuleData>(module.data));
        break;
    case AuthoringModuleType::PlaneRadialOrientation:
        root["data"] = Internal::To_Json(get<PlaneRadialOrientationModuleData>(module.data));
        break;
    case AuthoringModuleType::RotationOverLife:
        root["data"] = Internal::To_Json(get<RotationOverLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialRotationRate:
        root["data"] = Internal::To_Json(get<InitialRotationRateModuleData>(module.data));
        break;
    case AuthoringModuleType::RotationRateScaleByLife:
        root["data"] = Internal::To_Json(get<RotationRateScaleByLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialMeshRotation:
        root["data"] = Internal::To_Json(get<InitialMeshRotationModuleData>(module.data));
        break;
    case AuthoringModuleType::MeshRotationOverLife:
        root["data"] = Internal::To_Json(get<MeshRotationOverLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialMeshRotationRate:
        root["data"] = Internal::To_Json(get<InitialMeshRotationRateModuleData>(module.data));
        break;
    case AuthoringModuleType::MeshRotationRateScaleByLife:
        root["data"] = Internal::To_Json(get<MeshRotationRateScaleByLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialColor:
        root["data"] = Internal::To_Json(get<InitialColorModuleData>(module.data));
        break;
    case AuthoringModuleType::ColorOverLife:
        root["data"] = Internal::To_Json(get<ColorOverLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::SubUVFrameOverLife:
        root["data"] = Internal::To_Json(get<SubUVFrameOverLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::SizeByLife:
        root["data"] = Internal::To_Json(get<SizeByLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::MeshSizeByLife:
        root["data"] = Internal::To_Json(get<MeshSizeByLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::SpawnPerUnit:
        root["data"] = Internal::To_Json(get<SpawnPerUnitModuleData>(module.data));
        break;
    case AuthoringModuleType::MaterialScalarModulation:
        root["data"] = Internal::To_Json(get<MaterialScalarModulationModuleData>(module.data));
        break;
    }

    return root;
}

json EffectAuthoringJsonSerializer::To_Json(const AuthoringEmitter& emitter)
{
    json root = json::object();
    root["id"] = emitter.id;
    root["name"] = emitter.name;
    root["rendererType"] = Authoring::Resolve_RendererType(emitter.typeData);
    root["renderLayerOverride"] = Internal::To_EnumToken(emitter.renderLayerOverride);
    root["enabled"] = emitter.enabled;
    if (emitter.typeData.kind != AuthoringTypeDataKind::None)
        root["typeData"] = Internal::To_Json(emitter.typeData);
    root["modules"] = json::array();

    for (const AuthoringModule& module : emitter.modules)
        root["modules"].push_back(To_Json(module));

    return root;
}

bool EffectAuthoringJsonSerializer::From_Json(const json& root, AuthoringModule& outModule)
{
    if (!root.is_object())
        return false;

    AuthoringModuleType moduleType = AuthoringModuleType::Required;
    const auto typeIter = root.find("type");
    if (typeIter == root.end() || !Internal::Try_ReadEnumToken(*typeIter, moduleType))
        return false;

    AuthoringModule module{};
    module.type = moduleType;
    module.data = Make_DefaultModuleDataForRestore(moduleType);
    module.id = root.value("id", module.id);
    module.enabled = root.value("enabled", module.enabled);

    const auto dataIter = root.find("data");
    if (dataIter != root.end())
    {
        switch (module.type)
        {
        case AuthoringModuleType::Required:
            Internal::From_Json(*dataIter, get<RequiredModuleData>(module.data));
            break;
        case AuthoringModuleType::Spawn:
            Internal::From_Json(*dataIter, get<SpawnModuleData>(module.data));
            break;
        case AuthoringModuleType::Lifetime:
            Internal::From_Json(*dataIter, get<LifetimeModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialLocation:
            Internal::From_Json(*dataIter, get<InitialLocationModuleData>(module.data));
            break;
        case AuthoringModuleType::SphereLocation:
            Internal::From_Json(*dataIter, get<SphereLocationModuleData>(module.data));
            break;
        case AuthoringModuleType::PlaneRadialLocation:
            Internal::From_Json(*dataIter, get<PlaneRadialLocationModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialSize:
            Internal::From_Json(*dataIter, get<InitialSizeModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialMeshSize:
            Internal::From_Json(*dataIter, get<InitialMeshSizeModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialVelocity:
            Internal::From_Json(*dataIter, get<InitialVelocityModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialRadialVelocity:
            Internal::From_Json(*dataIter, get<InitialRadialVelocityModuleData>(module.data));
            break;
        case AuthoringModuleType::VelocityCone:
            Internal::From_Json(*dataIter, get<VelocityConeModuleData>(module.data));
            break;
        case AuthoringModuleType::Acceleration:
            Internal::From_Json(*dataIter, get<AccelerationModuleData>(module.data));
            break;
        case AuthoringModuleType::Drag:
            Internal::From_Json(*dataIter, get<DragModuleData>(module.data));
            break;
        case AuthoringModuleType::VelocityOverLife:
            Internal::From_Json(*dataIter, get<VelocityOverLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::OrbitOverLife:
            Internal::From_Json(*dataIter, get<OrbitOverLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialRotation:
            Internal::From_Json(*dataIter, get<InitialRotationModuleData>(module.data));
            break;
        case AuthoringModuleType::SphereRadialOrientation:
            Internal::From_Json(*dataIter, get<SphereRadialOrientationModuleData>(module.data));
            break;
        case AuthoringModuleType::PlaneRadialOrientation:
            Internal::From_Json(*dataIter, get<PlaneRadialOrientationModuleData>(module.data));
            break;
        case AuthoringModuleType::RotationOverLife:
            Internal::From_Json(*dataIter, get<RotationOverLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialRotationRate:
            Internal::From_Json(*dataIter, get<InitialRotationRateModuleData>(module.data));
            break;
        case AuthoringModuleType::RotationRateScaleByLife:
            Internal::From_Json(*dataIter, get<RotationRateScaleByLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialMeshRotation:
            Internal::From_Json(*dataIter, get<InitialMeshRotationModuleData>(module.data));
            break;
        case AuthoringModuleType::MeshRotationOverLife:
            Internal::From_Json(*dataIter, get<MeshRotationOverLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialMeshRotationRate:
            Internal::From_Json(*dataIter, get<InitialMeshRotationRateModuleData>(module.data));
            break;
        case AuthoringModuleType::MeshRotationRateScaleByLife:
            Internal::From_Json(*dataIter, get<MeshRotationRateScaleByLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialColor:
            Internal::From_Json(*dataIter, get<InitialColorModuleData>(module.data));
            break;
        case AuthoringModuleType::ColorOverLife:
            Internal::From_Json(*dataIter, get<ColorOverLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::SubUVFrameOverLife:
            Internal::From_Json(*dataIter, get<SubUVFrameOverLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::SizeByLife:
            Internal::From_Json(*dataIter, get<SizeByLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::MeshSizeByLife:
            Internal::From_Json(*dataIter, get<MeshSizeByLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::SpawnPerUnit:
            Internal::From_Json(*dataIter, get<SpawnPerUnitModuleData>(module.data));
            break;
        case AuthoringModuleType::MaterialScalarModulation:
            Internal::From_Json(*dataIter, get<MaterialScalarModulationModuleData>(module.data));
            break;
        }
    }

    outModule = move(module);
    return true;
}

bool EffectAuthoringJsonSerializer::From_Json(const json& root, AuthoringEmitter& outEmitter)
{
    if (!root.is_object())
        return false;

    AuthoringEmitter emitter{};
    emitter.id = root.value("id", 0u);
    emitter.name = root.value("name", string{});
    emitter.rendererType = root.value("rendererType", emitter.rendererType);
    if (const auto renderLayerOverrideIter = root.find("renderLayerOverride"); renderLayerOverrideIter != root.end())
        Internal::Try_ReadEnumToken(*renderLayerOverrideIter, emitter.renderLayerOverride);
    emitter.enabled = root.value("enabled", emitter.enabled);
    emitter.previewDirty = true;

    const auto typeDataIter = root.find("typeData");
    if (typeDataIter != root.end())
    {
        Internal::From_Json(*typeDataIter, emitter.typeData);
        emitter.rendererType = Authoring::Resolve_RendererType(emitter.typeData);
    }

    const auto modulesIter = root.find("modules");
    if (modulesIter != root.end() && modulesIter->is_array())
    {
        for (const json& moduleNode : *modulesIter)
        {
            AuthoringModule module{};
            if (From_Json(moduleNode, module))
                emitter.modules.push_back(move(module));
        }
    }

    outEmitter = move(emitter);
    return true;
}

uint32 EffectAuthoringJsonSerializer::Find_NextAuthoringId(const vector<AuthoringEmitter>& emitters)
{
    uint32 maxId = 0;
    for (const AuthoringEmitter& emitter : emitters)
    {
        maxId = max(maxId, emitter.id);
        for (const AuthoringModule& module : emitter.modules)
            maxId = max(maxId, module.id);
    }

    return maxId + 1;
}

AuthoringModuleData EffectAuthoringJsonSerializer::Make_DefaultModuleDataForRestore(AuthoringModuleType type)
{
    switch (type)
    {
    case AuthoringModuleType::Required:
        return RequiredModuleData{};
    case AuthoringModuleType::Spawn:
        return SpawnModuleData{};
    case AuthoringModuleType::Lifetime:
        return LifetimeModuleData{};
    case AuthoringModuleType::InitialLocation:
        return InitialLocationModuleData{};
    case AuthoringModuleType::SphereLocation:
        return SphereLocationModuleData{};
    case AuthoringModuleType::PlaneRadialLocation:
        return PlaneRadialLocationModuleData{};
    case AuthoringModuleType::InitialSize:
        return InitialSizeModuleData{};
    case AuthoringModuleType::InitialMeshSize:
        return InitialMeshSizeModuleData{};
    case AuthoringModuleType::InitialVelocity:
        return InitialVelocityModuleData{};
    case AuthoringModuleType::InitialRadialVelocity:
        return InitialRadialVelocityModuleData{};
    case AuthoringModuleType::VelocityCone:
        return VelocityConeModuleData{};
    case AuthoringModuleType::Acceleration:
        return AccelerationModuleData{};
    case AuthoringModuleType::Drag:
        return DragModuleData{};
    case AuthoringModuleType::VelocityOverLife:
        return VelocityOverLifeModuleData{};
    case AuthoringModuleType::OrbitOverLife:
        return OrbitOverLifeModuleData{};
    case AuthoringModuleType::InitialRotation:
        return InitialRotationModuleData{};
    case AuthoringModuleType::SphereRadialOrientation:
        return SphereRadialOrientationModuleData{};
    case AuthoringModuleType::PlaneRadialOrientation:
        return PlaneRadialOrientationModuleData{};
    case AuthoringModuleType::RotationOverLife:
        return RotationOverLifeModuleData{};
    case AuthoringModuleType::InitialRotationRate:
        return InitialRotationRateModuleData{};
    case AuthoringModuleType::RotationRateScaleByLife:
        return RotationRateScaleByLifeModuleData{};
    case AuthoringModuleType::InitialMeshRotation:
        return InitialMeshRotationModuleData{};
    case AuthoringModuleType::MeshRotationOverLife:
        return MeshRotationOverLifeModuleData{};
    case AuthoringModuleType::InitialMeshRotationRate:
        return InitialMeshRotationRateModuleData{};
    case AuthoringModuleType::MeshRotationRateScaleByLife:
        return MeshRotationRateScaleByLifeModuleData{};
    case AuthoringModuleType::InitialColor:
        return InitialColorModuleData{};
    case AuthoringModuleType::ColorOverLife:
        return ColorOverLifeModuleData{};
    case AuthoringModuleType::SubUVFrameOverLife:
        return SubUVFrameOverLifeModuleData{};
    case AuthoringModuleType::SizeByLife:
        return SizeByLifeModuleData{};
    case AuthoringModuleType::MeshSizeByLife:
        return MeshSizeByLifeModuleData{};
    case AuthoringModuleType::SpawnPerUnit:
        return SpawnPerUnitModuleData{};
    case AuthoringModuleType::MaterialScalarModulation:
        return MaterialScalarModulationModuleData{};
    }

    return RequiredModuleData{};
}

NS_END
