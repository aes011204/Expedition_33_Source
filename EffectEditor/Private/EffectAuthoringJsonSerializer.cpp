#include "EffectAuthoringJsonSerializer.h"

#include "Helper_EffectAuthoring.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
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

    void Read_BoolField(const json& node, const char* key, bool& outValue)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_boolean())
            return;

        outValue = iter->get<bool>();
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
            outGuid = "3c90794a-31bb-4e97-969a-3fd4497acc0f";
            outPath = "Effects/Textures/Shared/DefaultTexture.dds";
            return true;
        }

        if (textureId == "Texture_Effect_Clouds4")
        {
            outGuid = "321bf66c-e6f9-4495-9855-275274727e83";
            outPath = "Effects/Textures/Shared/clouds4_T.dds";
            return true;
        }

        return false;
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
        const auto curveQualityIter = root.find("curveQuality");
        if (curveQualityIter != root.end())
            Try_ReadEnumToken(*curveQualityIter, outData.curveQuality);
        Read_NumberField(root, "sampleSpacing", outData.sampleSpacing);
        Read_NumberField(root, "curveSubdivision", outData.curveSubdivision);
        Read_BoolField(root, "smoothTangent", outData.smoothTangent);
        Read_NumberField(root, "sideFade", outData.sideFade);
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
        root["mainTexturePath"] = data.mainTexturePath;
        root["noiseTexturePath"] = data.noiseTexturePath;
        root["tint"] = To_Json(data.tint);
        root["intensity"] = data.intensity;
        root["opacityPower"] = data.opacityPower;
        root["noiseStrength"] = data.noiseStrength;
        root["blendMode"] = data.blendMode;
        root["opacitySource"] = data.opacitySource;
        root["subUVRows"] = data.subUVRows;
        root["subUVCols"] = data.subUVCols;
        return root;
    }

    void From_Json(const json& root, EffectMaterialInstanceData& outData)
    {
        if (!root.is_object())
            return;

        Read_StringField(root, "sourcePresetName", outData.sourcePresetName);
        Read_StringField(root, "mainTextureGuid", outData.mainTextureGuid);
        Read_StringField(root, "mainTexturePath", outData.mainTexturePath);
        Read_StringField(root, "noiseTextureGuid", outData.noiseTextureGuid);
        Read_StringField(root, "noiseTexturePath", outData.noiseTexturePath);
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
        Read_NumberField(root, "intensity", outData.intensity);
        Read_NumberField(root, "opacityPower", outData.opacityPower);
        Read_NumberField(root, "noiseStrength", outData.noiseStrength);
        Read_StringField(root, "blendMode", outData.blendMode);
        Read_StringField(root, "opacitySource", outData.opacitySource);
        Read_NumberField(root, "subUVRows", outData.subUVRows);
        Read_NumberField(root, "subUVCols", outData.subUVCols);

        const auto tintIter = root.find("tint");
        if (tintIter != root.end())
            Read_Color(*tintIter, outData.tint);
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
            if (parsedMode == DistributionMode::Constant || parsedMode == DistributionMode::Uniform)
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
            if (parsedMode == DistributionMode::Constant || parsedMode == DistributionMode::Uniform)
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
            if (parsedMode == DistributionMode::Constant || parsedMode == DistributionMode::Uniform)
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
        root["sortMode"] = To_EnumToken(data.sortMode);
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
        const auto sortModeIter = root.find("sortMode");
        if (sortModeIter != root.end())
            Try_ReadEnumToken(*sortModeIter, outData.sortMode);

        Read_NumberField(root, "spriteRollOffsetDegrees", outData.spriteRollOffsetDegrees);
        Read_NumberField(root, "minCameraBlendDistance", outData.minCameraBlendDistance);
        Read_NumberField(root, "maxCameraBlendDistance", outData.maxCameraBlendDistance);
        Read_BoolField(root, "useLocalSpace", outData.useLocalSpace);
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
    }

    json To_Json(const SphereLocationModuleData& data)
    {
        json root = json::object();
        root["offset"] = To_Json(data.offset);
        root["radius"] = data.radius;
        root["spawnMode"] = To_EnumToken(data.spawnMode);
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
    }

    json To_Json(const InitialRadialVelocityModuleData& data)
    {
        json root = json::object();
        root["speed"] = To_Json(data.speed);
        root["radialPivot"] = To_Json(data.radialPivot);
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
        Read_BoolField(root, "inWorldSpace", outData.inWorldSpace);
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
    }

    json To_Json(const VelocityOverLifeModuleData& data)
    {
        json root = json::object();
        root["scaleStart"] = data.scaleStart;
        root["scaleEnd"] = data.scaleEnd;
        return root;
    }

    void From_Json(const json& root, VelocityOverLifeModuleData& outData)
    {
        if (!root.is_object())
            return;

        Read_NumberField(root, "scaleStart", outData.scaleStart);
        Read_NumberField(root, "scaleEnd", outData.scaleEnd);
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
    }

    json To_Json(const RotationOverLifeModuleData& data)
    {
        json root = json::object();
        root["rotationStartDegrees"] = data.rotationStartDegrees;
        root["rotationEndDegrees"] = data.rotationEndDegrees;
        return root;
    }

    void From_Json(const json& root, RotationOverLifeModuleData& outData)
    {
        if (!root.is_object())
            return;

        Read_NumberField(root, "rotationStartDegrees", outData.rotationStartDegrees);
        Read_NumberField(root, "rotationEndDegrees", outData.rotationEndDegrees);
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
    }

    json To_Json(const RotationRateScaleByLifeModuleData& data)
    {
        json root = json::object();
        root["scaleStart"] = data.scaleStart;
        root["scaleEnd"] = data.scaleEnd;
        return root;
    }

    void From_Json(const json& root, RotationRateScaleByLifeModuleData& outData)
    {
        if (!root.is_object())
            return;

        Read_NumberField(root, "scaleStart", outData.scaleStart);
        Read_NumberField(root, "scaleEnd", outData.scaleEnd);
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
    }

    //## Module Data Codec

    json To_Json(const SizeByLifeModuleData& data)
    {
        json root = json::object();
        root["multiplyXStart"] = data.multiplyXStart;
        root["multiplyXEnd"] = data.multiplyXEnd;
        root["multiplyYStart"] = data.multiplyYStart;
        root["multiplyYEnd"] = data.multiplyYEnd;
        return root;
    }

    void From_Json(const json& root, SizeByLifeModuleData& outData)
    {
        if (!root.is_object())
            return;

        Read_NumberField(root, "multiplyXStart", outData.multiplyXStart);
        Read_NumberField(root, "multiplyXEnd", outData.multiplyXEnd);
        Read_NumberField(root, "multiplyYStart", outData.multiplyYStart);
        Read_NumberField(root, "multiplyYEnd", outData.multiplyYEnd);
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
    case AuthoringModuleType::InitialSize:
        root["data"] = Internal::To_Json(get<InitialSizeModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialVelocity:
        root["data"] = Internal::To_Json(get<InitialVelocityModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialRadialVelocity:
        root["data"] = Internal::To_Json(get<InitialRadialVelocityModuleData>(module.data));
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
    case AuthoringModuleType::InitialRotation:
        root["data"] = Internal::To_Json(get<InitialRotationModuleData>(module.data));
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
    }

    return root;
}

json EffectAuthoringJsonSerializer::To_Json(const AuthoringEmitter& emitter)
{
    json root = json::object();
    root["id"] = emitter.id;
    root["name"] = emitter.name;
    root["rendererType"] = Authoring::Resolve_RendererType(emitter.typeData);
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
        case AuthoringModuleType::InitialSize:
            Internal::From_Json(*dataIter, get<InitialSizeModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialVelocity:
            Internal::From_Json(*dataIter, get<InitialVelocityModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialRadialVelocity:
            Internal::From_Json(*dataIter, get<InitialRadialVelocityModuleData>(module.data));
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
        case AuthoringModuleType::InitialRotation:
            Internal::From_Json(*dataIter, get<InitialRotationModuleData>(module.data));
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
    case AuthoringModuleType::InitialSize:
        return InitialSizeModuleData{};
    case AuthoringModuleType::InitialVelocity:
        return InitialVelocityModuleData{};
    case AuthoringModuleType::InitialRadialVelocity:
        return InitialRadialVelocityModuleData{};
    case AuthoringModuleType::Acceleration:
        return AccelerationModuleData{};
    case AuthoringModuleType::Drag:
        return DragModuleData{};
    case AuthoringModuleType::VelocityOverLife:
        return VelocityOverLifeModuleData{};
    case AuthoringModuleType::InitialRotation:
        return InitialRotationModuleData{};
    case AuthoringModuleType::RotationOverLife:
        return RotationOverLifeModuleData{};
    case AuthoringModuleType::InitialRotationRate:
        return InitialRotationRateModuleData{};
    case AuthoringModuleType::RotationRateScaleByLife:
        return RotationRateScaleByLifeModuleData{};
    case AuthoringModuleType::InitialColor:
        return InitialColorModuleData{};
    case AuthoringModuleType::ColorOverLife:
        return ColorOverLifeModuleData{};
    case AuthoringModuleType::SubUVFrameOverLife:
        return SubUVFrameOverLifeModuleData{};
    case AuthoringModuleType::SizeByLife:
        return SizeByLifeModuleData{};
    }

    return RequiredModuleData{};
}

NS_END
