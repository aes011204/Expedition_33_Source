#include "pch.h"
#include "EffectAssetRuntimeLoader.h"

#include "GameInstance.h"

NS_BEGIN(Client)

namespace Internal
{
    constexpr uint32 kMaxSubUVFrameCurveKeys{ 4 };

    string To_LogPath(const fs::path& path)
    {
        return String::ToString(path.wstring());
    }

    string Read_String(const json& node, const char* key, const string& fallbackValue = {})
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_string())
            return fallbackValue;

        return iter->get<string>();
    }

    bool Read_Bool(const json& node, const char* key, bool fallbackValue = false)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_boolean())
            return fallbackValue;

        return iter->get<bool>();
    }

    uint32 Read_UInt(const json& node, const char* key, uint32 fallbackValue = 0u)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_number_unsigned())
            return fallbackValue;

        return iter->get<uint32>();
    }

    float Read_Float(const json& node, const char* key, float fallbackValue = 0.f)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_number())
            return fallbackValue;

        return iter->get<float>();
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

    void Read_Vec4(const json& node, Vec4& outValue)
    {
        if (!node.is_array() || node.size() < 4 ||
            !node[0].is_number() || !node[1].is_number() || !node[2].is_number() || !node[3].is_number())
            return;

        outValue.x = node[0].get<float>();
        outValue.y = node[1].get<float>();
        outValue.z = node[2].get<float>();
        outValue.w = node[3].get<float>();
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

    template <typename EnumType>
    void Read_Enum(const json& node, const char* key, EnumType& outValue)
    {
        const string token = Read_String(node, key);
        if (token.empty())
            return;

        const optional<EnumType> parsed = magic_enum::enum_cast<EnumType>(token);
        if (parsed.has_value())
            outValue = parsed.value();
    }
}

using Internal::Read_Bool;
using Internal::Read_Enum;
using Internal::Read_Float;
using Internal::Read_String;
using Internal::Read_UInt;
using Internal::Read_Vec2;
using Internal::Read_Vec3;
using Internal::Read_Vec4;
using Internal::Read_Color;
using Internal::To_LogPath;
using Internal::kMaxSubUVFrameCurveKeys;

Shared<const EffectDefinition> EffectAssetRuntimeLoader::Load_Definition(const wstring& effectAssetPath)
{
    json root{};
    if (!Load_JsonRoot(effectAssetPath, root))
        return nullptr;

    if (!root.is_object())
    {
        LOG_ERROR("Root is not object. path='{}'", String::ToString(effectAssetPath));
        return nullptr;
    }

    const auto emittersIter = root.find("emitters");
    if (emittersIter == root.end() || !emittersIter->is_array())
    {
        LOG_ERROR("Emitters array is missing. path='{}'", String::ToString(effectAssetPath));
        return nullptr;
    }

    auto definition = make_shared<EffectDefinition>();
    definition->name = Read_String(root, "name");
    if (definition->name.empty())
        definition->name = String::ToString(fs::path(effectAssetPath).stem().wstring());

    for (const json& emitter : *emittersIter)
    {
        if (!emitter.is_object())
        {
            LOG_WARN("Skipped invalid emitter node. effect='{}'", definition->name);
            continue;
        }

        if (!Read_Bool(emitter, "enabled", true))
            continue;

        const string rendererType = Resolve_RendererType(emitter);
        const string rendererTypeLower = String::ToLowerCopy(rendererType);

        EffectEmitterDefinition emitterDefinition{};
        bool converted = false;

        if (rendererTypeLower == "sprite")
            converted = Build_EmitterDefinition(emitter, emitterDefinition);
        else if (rendererTypeLower == "trail")
            converted = Build_TrailEmitterDefinition(emitter, emitterDefinition);
        else
        {
            LOG_INFO(
                "Skipped unsupported renderer. effect='{}', emitter='{}', rendererType='{}'",
                definition->name,
                Internal::Read_String(emitter, "name"),
                rendererType
            );
            continue;
        }

        if (!converted)
        {
            LOG_WARN(
                "EffectAssetRuntimeLoader skipped emitter conversion failure. effect='{}', emitter='{}'",
                definition->name,
                Internal::Read_String(emitter, "name")
            );
            continue;
        }

        definition->emitters.push_back(emitterDefinition);
    }

    if (definition->emitters.empty())
    {
        LOG_ERROR("No runtime emitter converted. path='{}'", String::ToString(effectAssetPath));
        return nullptr;
    }

    return definition;
}

Shared<const EffectDefinition> EffectAssetRuntimeLoader::Load_DefinitionByName(const wstring& effectName)
{
    string effectAssetGuid{};
    if (!Find_EffectAssetGuidByName(effectName, effectAssetGuid))
        return nullptr;

    wstring resolvedEffectAssetPath{};
    if (!Resolve_EffectAssetPathByGuid(effectAssetGuid, resolvedEffectAssetPath))
        return nullptr;

    return Load_Definition(resolvedEffectAssetPath);
}

const json* EffectAssetRuntimeLoader::Find_ModuleData(const json& emitter, const string& moduleType)
{
    const auto modulesIter = emitter.find("modules");
    if (modulesIter == emitter.end() || !modulesIter->is_array())
        return nullptr;

    for (const json& module : *modulesIter)
    {
        if (!module.is_object())
            continue;

        if (!Read_Bool(module, "enabled", true))
            continue;

        if (Read_String(module, "type") != moduleType)
            continue;

        const auto dataIter = module.find("data");
        if (dataIter == module.end() || !dataIter->is_object())
            return nullptr;

        return &*dataIter;
    }

    return nullptr;
}

float EffectAssetRuntimeLoader::Evaluate_FloatCurveSegmentLinear(const json& leftKey, const json& rightKey, float x)
{
    const float leftTime = Read_Float(leftKey, "time");
    const float rightTime = Read_Float(rightKey, "time", 1.f);
    const float width = max(0.0001f, rightTime - leftTime);
    const float t = clamp((x - leftTime) / width, 0.f, 1.f);
    return lerp(Read_Float(leftKey, "value"), Read_Float(rightKey, "value"), t);
}

float EffectAssetRuntimeLoader::Evaluate_FloatCurveSegmentAutoClamped(const json& leftKey, const json& rightKey, float x)
{
    const float leftTime = Read_Float(leftKey, "time");
    const float rightTime = Read_Float(rightKey, "time", 1.f);
    const float width = max(0.0001f, rightTime - leftTime);
    const float t = clamp((x - leftTime) / width, 0.f, 1.f);
    const float m0 = Read_Float(leftKey, "leaveTangent") * width;
    const float m1 = Read_Float(rightKey, "arriveTangent") * width;
    const float t2 = t * t;
    const float t3 = t2 * t;

    return (2.f * t3 - 3.f * t2 + 1.f) * Read_Float(leftKey, "value") +
           (t3 - 2.f * t2 + t) * m0 +
           (-2.f * t3 + 3.f * t2) * Read_Float(rightKey, "value") +
           (t3 - t2) * m1;
}

float EffectAssetRuntimeLoader::Evaluate_FloatConstantCurve(const json& keys, float x, float fallbackValue)
{
    if (!keys.is_array() || keys.empty())
        return fallbackValue;

    if (keys.size() == 1)
        return Read_Float(keys.front(), "value", fallbackValue);

    const float clampedX = clamp(x, 0.f, 1.f);
    for (size_t index = 1; index < keys.size(); ++index)
    {
        const json& leftKey = keys[index - 1];
        const json& rightKey = keys[index];
        if (clampedX > Read_Float(rightKey, "time", 1.f))
            continue;

        const string interpolationMode = Read_String(leftKey, "interpolationMode", "Linear");
        if ("Constant" == interpolationMode)
            return Read_Float(leftKey, "value", fallbackValue);

        if ("CurveAutoClamped" == interpolationMode)
            return Evaluate_FloatCurveSegmentAutoClamped(leftKey, rightKey, clampedX);

        return Evaluate_FloatCurveSegmentLinear(leftKey, rightKey, clampedX);
    }

    return Read_Float(keys.back(), "value", fallbackValue);
}

const json* EffectAssetRuntimeLoader::Find_DistributionPayload(const json& distribution)
{
    const auto payloadIter = distribution.find("payload");
    if (payloadIter == distribution.end() || !payloadIter->is_object())
        return nullptr;

    return &*payloadIter;
}

float EffectAssetRuntimeLoader::Evaluate_FloatDistributionMin(const json& distribution, float fallbackValue)
{
    const json* payload = Find_DistributionPayload(distribution);
    if (nullptr == payload)
        return fallbackValue;

    const string mode = Read_String(distribution, "mode");
    if ("Constant" == mode)
        return Read_Float(*payload, "value", fallbackValue);

    if ("Uniform" == mode)
        return Read_Float(*payload, "minValue", fallbackValue);

    if ("ConstantCurve" == mode)
    {
        const auto keysIter = payload->find("keys");
        return keysIter != payload->end() ? Evaluate_FloatConstantCurve(*keysIter, 0.f, fallbackValue) : fallbackValue;
    }

    return fallbackValue;
}

float EffectAssetRuntimeLoader::Evaluate_FloatDistributionMax(const json& distribution, float fallbackValue)
{
    const json* payload = Find_DistributionPayload(distribution);
    if (nullptr == payload)
        return fallbackValue;

    const string mode = Read_String(distribution, "mode");
    if ("Constant" == mode)
        return Read_Float(*payload, "value", fallbackValue);

    if ("Uniform" == mode)
        return Read_Float(*payload, "maxValue", fallbackValue);

    if ("ConstantCurve" == mode)
    {
        const auto keysIter = payload->find("keys");
        return keysIter != payload->end() ? Evaluate_FloatConstantCurve(*keysIter, 1.f, fallbackValue) : fallbackValue;
    }

    return fallbackValue;
}

Vec2 EffectAssetRuntimeLoader::Evaluate_Vector2DistributionMin(const json& distribution, const Vec2& fallbackValue)
{
    const json* payload = Find_DistributionPayload(distribution);
    if (nullptr == payload)
        return fallbackValue;

    Vec2 value = fallbackValue;
    if ("Uniform" == Read_String(distribution, "mode"))
    {
        const auto iter = payload->find("minValue");
        if (iter != payload->end())
            Read_Vec2(*iter, value);
        return value;
    }

    const auto iter = payload->find("value");
    if (iter != payload->end())
        Read_Vec2(*iter, value);
    return value;
}

Vec2 EffectAssetRuntimeLoader::Evaluate_Vector2DistributionMax(const json& distribution, const Vec2& fallbackValue)
{
    const json* payload = Find_DistributionPayload(distribution);
    if (nullptr == payload)
        return fallbackValue;

    Vec2 value = fallbackValue;
    if ("Uniform" == Read_String(distribution, "mode"))
    {
        const auto iter = payload->find("maxValue");
        if (iter != payload->end())
            Read_Vec2(*iter, value);
        return value;
    }

    const auto iter = payload->find("value");
    if (iter != payload->end())
        Read_Vec2(*iter, value);
    return value;
}

Vec3 EffectAssetRuntimeLoader::Evaluate_Vector3DistributionMin(const json& distribution, const Vec3& fallbackValue)
{
    const json* payload = Find_DistributionPayload(distribution);
    if (nullptr == payload)
        return fallbackValue;

    Vec3 value = fallbackValue;
    if ("Uniform" == Read_String(distribution, "mode"))
    {
        const auto iter = payload->find("minValue");
        if (iter != payload->end())
            Read_Vec3(*iter, value);
        return value;
    }

    const auto iter = payload->find("value");
    if (iter != payload->end())
        Read_Vec3(*iter, value);
    return value;
}

Vec3 EffectAssetRuntimeLoader::Evaluate_Vector3DistributionMax(const json& distribution, const Vec3& fallbackValue)
{
    const json* payload = Find_DistributionPayload(distribution);
    if (nullptr == payload)
        return fallbackValue;

    Vec3 value = fallbackValue;
    if ("Uniform" == Read_String(distribution, "mode"))
    {
        const auto iter = payload->find("maxValue");
        if (iter != payload->end())
            Read_Vec3(*iter, value);
        return value;
    }

    const auto iter = payload->find("value");
    if (iter != payload->end())
        Read_Vec3(*iter, value);
    return value;
}

Vec4 EffectAssetRuntimeLoader::Clamp_Color(Vec4 value)
{
    value.x = clamp(value.x, 0.f, 1.f);
    value.y = clamp(value.y, 0.f, 1.f);
    value.z = clamp(value.z, 0.f, 1.f);
    value.w = clamp(value.w, 0.f, 1.f);
    return value;
}

Vec4 EffectAssetRuntimeLoader::Evaluate_ColorDistributionMin(const json& distribution, const Vec4& fallbackValue)
{
    const json* payload = Find_DistributionPayload(distribution);
    if (nullptr == payload)
        return Clamp_Color(fallbackValue);

    Vec4 value = fallbackValue;
    if ("Uniform" == Read_String(distribution, "mode"))
    {
        const auto iter = payload->find("minValue");
        if (iter != payload->end())
            Read_Vec4(*iter, value);
        return Clamp_Color(value);
    }

    const auto iter = payload->find("value");
    if (iter != payload->end())
        Read_Vec4(*iter, value);
    return Clamp_Color(value);
}

Vec4 EffectAssetRuntimeLoader::Evaluate_ColorDistributionMax(const json& distribution, const Vec4& fallbackValue)
{
    const json* payload = Find_DistributionPayload(distribution);
    if (nullptr == payload)
        return Clamp_Color(fallbackValue);

    Vec4 value = fallbackValue;
    if ("Uniform" == Read_String(distribution, "mode"))
    {
        const auto iter = payload->find("maxValue");
        if (iter != payload->end())
            Read_Vec4(*iter, value);
        return Clamp_Color(value);
    }

    const auto iter = payload->find("value");
    if (iter != payload->end())
        Read_Vec4(*iter, value);
    return Clamp_Color(value);
}

void EffectAssetRuntimeLoader::Set_SubUVFrameCurveComponent(Vec4& values, uint32 index, float value)
{
    switch (index)
    {
    case 0:
        values.x = value;
        break;
    case 1:
        values.y = value;
        break;
    case 2:
        values.z = value;
        break;
    case 3:
        values.w = value;
        break;
    default:
        break;
    }
}

void EffectAssetRuntimeLoader::Fill_SubUVFrameCurvePayload(PointParticleSubUVFrameOverLifeDesc& desc, const json& frameIndex)
{
    desc.frameCurveKeyCount = 1;
    desc.frameCurveTimes = Vec4{};
    desc.frameCurveValues = Vec4{};

    const json* payload = Find_DistributionPayload(frameIndex);
    if (nullptr == payload)
        return;

    if ("Constant" == Read_String(frameIndex, "mode"))
    {
        desc.frameCurveValues.x = Read_Float(*payload, "value");
        return;
    }

    if ("ConstantCurve" != Read_String(frameIndex, "mode"))
        return;

    const auto keysIter = payload->find("keys");
    if (keysIter == payload->end() || !keysIter->is_array() || keysIter->empty())
        return;

    const uint32 keyCount = min(kMaxSubUVFrameCurveKeys, static_cast<uint32>(keysIter->size()));
    desc.frameCurveKeyCount = keyCount;

    for (uint32 index = 0; index < keyCount; ++index)
    {
        const json& key = (*keysIter)[index];
        Set_SubUVFrameCurveComponent(desc.frameCurveTimes, index, clamp(Read_Float(key, "time"), 0.f, 1.f));
        Set_SubUVFrameCurveComponent(desc.frameCurveValues, index, Read_Float(key, "value"));
    }
}

string EffectAssetRuntimeLoader::Resolve_RendererType(const json& emitter)
{
    string rendererType = Read_String(emitter, "rendererType");
    if (!rendererType.empty())
        return rendererType;

    if (const json* required = Find_ModuleData(emitter, "Required"))
    {
        rendererType = Read_String(*required, "rendererType");
        if (!rendererType.empty())
            return rendererType;
    }

    const auto typeDataIter = emitter.find("typeData");
    if (typeDataIter != emitter.end() && typeDataIter->is_object())
        return Read_String(*typeDataIter, "kind");

    return {};
}

void EffectAssetRuntimeLoader::Apply_MaterialPayload(
    const json& material,
    EffectRequiredMaterialRuntimeDesc& materialDesc,
    bool includeTrailMaterialFields)
{
    Read_Enum(material, "materialFamily", materialDesc.materialFamily);
    materialDesc.mainTextureGuid = Read_String(material, "mainTextureGuid", materialDesc.mainTextureGuid);
    materialDesc.mainTexturePath = Read_String(material, "mainTexturePath", materialDesc.mainTexturePath);
    materialDesc.noiseTextureGuid = Read_String(material, "noiseTextureGuid", materialDesc.noiseTextureGuid);
    materialDesc.noiseTexturePath = Read_String(material, "noiseTexturePath", materialDesc.noiseTexturePath);
    materialDesc.maskTextureGuid = Read_String(material, "maskTextureGuid", materialDesc.maskTextureGuid);
    materialDesc.maskTexturePath = Read_String(material, "maskTexturePath", materialDesc.maskTexturePath);
    materialDesc.flowTextureGuid = Read_String(material, "flowTextureGuid", materialDesc.flowTextureGuid);
    materialDesc.flowTexturePath = Read_String(material, "flowTexturePath", materialDesc.flowTexturePath);

    Read_Enum(material, "blendMode", materialDesc.blendMode);

    const string opacitySource = Read_String(material, "opacitySource");
    if (!opacitySource.empty())
        materialDesc.opacitySource = opacitySource;

    const auto tintIter = material.find("tint");
    if (tintIter != material.end())
        Read_Vec4(*tintIter, materialDesc.tint);

    materialDesc.intensity = Read_Float(material, "intensity", materialDesc.intensity);
    materialDesc.opacityPower = Read_Float(material, "opacityPower", materialDesc.opacityPower);
    materialDesc.noiseStrength = Read_Float(material, "noiseStrength", materialDesc.noiseStrength);
    materialDesc.alphaCutoff = Read_Float(material, "alphaCutoff", materialDesc.alphaCutoff);
    materialDesc.alphaErosion = Read_Float(material, "alphaErosion", materialDesc.alphaErosion);

    if (const auto mainUVOffsetIter = material.find("mainUVOffset"); mainUVOffsetIter != material.end())
        Read_Vec2(*mainUVOffsetIter, materialDesc.mainUVOffset);

    if (const auto noiseUVOffsetIter = material.find("noiseUVOffset"); noiseUVOffsetIter != material.end())
        Read_Vec2(*noiseUVOffsetIter, materialDesc.noiseUVOffset);

    if (const auto maskUVOffsetIter = material.find("maskUVOffset"); maskUVOffsetIter != material.end())
        Read_Vec2(*maskUVOffsetIter, materialDesc.maskUVOffset);

    if (const auto flowUVScaleIter = material.find("flowUVScale"); flowUVScaleIter != material.end())
        Read_Vec2(*flowUVScaleIter, materialDesc.flowUVScale);

    if (const auto flowUVOffsetIter = material.find("flowUVOffset"); flowUVOffsetIter != material.end())
        Read_Vec2(*flowUVOffsetIter, materialDesc.flowUVOffset);

    if (const auto flowUVScrollSpeedIter = material.find("flowUVScrollSpeed"); flowUVScrollSpeedIter != material.end())
        Read_Vec2(*flowUVScrollSpeedIter, materialDesc.flowUVScrollSpeed);

    Read_Enum(material, "mainUVTilingMode", materialDesc.mainUVTilingMode);
    Read_Enum(material, "noiseUVTilingMode", materialDesc.noiseUVTilingMode);
    Read_Enum(material, "maskUVTilingMode", materialDesc.maskUVTilingMode);
    Read_Enum(material, "flowUVTilingMode", materialDesc.flowUVTilingMode);
    materialDesc.refractionIntensity = Read_Float(material, "refractionIntensity", materialDesc.refractionIntensity);
    materialDesc.refractionPresence = Read_Float(material, "refractionPresence", materialDesc.refractionPresence);
    materialDesc.subUVRows = max(1u, Read_UInt(material, "subUVRows", materialDesc.subUVRows));
    materialDesc.subUVCols = max(1u, Read_UInt(material, "subUVCols", materialDesc.subUVCols));

    if (includeTrailMaterialFields)
    {
        if (const auto mainUVScaleIter = material.find("mainUVScale"); mainUVScaleIter != material.end())
            Read_Vec2(*mainUVScaleIter, materialDesc.mainUVScale);

        if (const auto mainUVScrollSpeedIter = material.find("mainUVScrollSpeed"); mainUVScrollSpeedIter != material.end())
            Read_Vec2(*mainUVScrollSpeedIter, materialDesc.mainUVScrollSpeed);

        if (const auto noiseUVScaleIter = material.find("noiseUVScale"); noiseUVScaleIter != material.end())
            Read_Vec2(*noiseUVScaleIter, materialDesc.noiseUVScale);

        if (const auto noiseUVScrollSpeedIter = material.find("noiseUVScrollSpeed"); noiseUVScrollSpeedIter != material.end())
            Read_Vec2(*noiseUVScrollSpeedIter, materialDesc.noiseUVScrollSpeed);

        if (const auto maskUVScaleIter = material.find("maskUVScale"); maskUVScaleIter != material.end())
            Read_Vec2(*maskUVScaleIter, materialDesc.maskUVScale);

        if (const auto maskUVScrollSpeedIter = material.find("maskUVScrollSpeed"); maskUVScrollSpeedIter != material.end())
            Read_Vec2(*maskUVScrollSpeedIter, materialDesc.maskUVScrollSpeed);

        materialDesc.twoSided = Read_Bool(material, "twoSided", materialDesc.twoSided);
    }

    const auto additiveIter = material.find("additive");
    if (additiveIter != material.end() && additiveIter->is_object())
    {
        const json& additive = *additiveIter;
        Read_Enum(additive, "colorSource", materialDesc.additive.colorSource);
        Read_Enum(additive, "amountSource", materialDesc.additive.amountSource);
        Read_Enum(additive, "coveragePolicy", materialDesc.additive.coveragePolicy);
        materialDesc.additive.intensityScale =
            Read_Float(additive, "intensityScale", materialDesc.additive.intensityScale);
        materialDesc.additive.blackNeutral =
            Read_Bool(additive, "blackNeutral", materialDesc.additive.blackNeutral);

        const auto emissiveColorIter = additive.find("emissiveColor");
        if (emissiveColorIter != additive.end())
            Read_Color(*emissiveColorIter, materialDesc.additive.emissiveColor);
        const auto constantColorIter = additive.find("constantColor");
        if (constantColorIter != additive.end())
            Read_Color(*constantColorIter, materialDesc.additive.constantColor);
    }
}

void EffectAssetRuntimeLoader::Apply_RequiredModule(const json& data, EffectEmitterDefinition& definition, ComputeSpriteEmitterDesc& desc)
{
    const auto materialIter = data.find("material");
    if (materialIter != data.end() && materialIter->is_object())
        Apply_MaterialPayload(*materialIter, desc.required.material, false);

    Read_Enum(data, "screenAlignment", desc.required.spriteRender.screenAlignment);
    Read_Enum(data, "directionalAlignmentMode", desc.required.spriteRender.directionalAlignmentMode);
    Read_Enum(data, "spriteTextureAxis", desc.required.spriteRender.spriteTextureAxis);
    desc.required.spriteRender.spriteRollOffsetDegrees =
        Read_Float(data, "spriteRollOffsetDegrees", desc.required.spriteRender.spriteRollOffsetDegrees);
    desc.required.playback.duration = max(0.0001f, Read_Float(data, "duration", desc.required.playback.duration));
    desc.required.playback.loopCount = Read_UInt(data, "loopCount", desc.required.playback.loopCount);
    desc.required.playback.delay = max(0.f, Read_Float(data, "delay", desc.required.playback.delay));
    desc.required.playback.delayFirstLoopOnly = Read_Bool(data, "delayFirstLoopOnly", desc.required.playback.delayFirstLoopOnly);
    desc.required.playback.killOnDeactivate = Read_Bool(data, "killOnDeactivate", desc.required.playback.killOnDeactivate);
    desc.required.playback.killOnCompleted = Read_Bool(data, "killOnCompleted", desc.required.playback.killOnCompleted);
    desc.required.drawLimit.useMaxDrawCount = Read_Bool(data, "useMaxDrawCount", desc.required.drawLimit.useMaxDrawCount);
    desc.required.drawLimit.maxDrawCount = max(1u, Read_UInt(data, "maxDrawCount", desc.required.drawLimit.maxDrawCount));

    const auto originIter = data.find("emitterOrigin");
    if (originIter != data.end())
        Read_Vec3(*originIter, definition.localPosition);

    const auto rotationIter = data.find("emitterRotationDegrees");
    if (rotationIter != data.end())
        Read_Vec3(*rotationIter, definition.localRotationDegrees);

    definition.useLocalSpace = Read_Bool(data, "useLocalSpace", definition.useLocalSpace);
}

void EffectAssetRuntimeLoader::Apply_RequiredModule(const json& data, EffectEmitterDefinition& definition, ComputeTrailEmitterDesc& desc)
{
    const auto materialIter = data.find("material");
    if (materialIter != data.end() && materialIter->is_object())
        Apply_MaterialPayload(*materialIter, desc.material, true);

    desc.playback.duration = max(0.0001f, Read_Float(data, "duration", desc.playback.duration));
    desc.playback.loopCount = Read_UInt(data, "loopCount", desc.playback.loopCount);
    desc.playback.delay = max(0.f, Read_Float(data, "delay", desc.playback.delay));
    desc.playback.delayFirstLoopOnly = Read_Bool(data, "delayFirstLoopOnly", desc.playback.delayFirstLoopOnly);
    desc.playback.killOnDeactivate = Read_Bool(data, "killOnDeactivate", desc.playback.killOnDeactivate);
    desc.playback.killOnCompleted = Read_Bool(data, "killOnCompleted", desc.playback.killOnCompleted);

    const auto originIter = data.find("emitterOrigin");
    if (originIter != data.end())
        Read_Vec3(*originIter, definition.localPosition);

    const auto rotationIter = data.find("emitterRotationDegrees");
    if (rotationIter != data.end())
        Read_Vec3(*rotationIter, definition.localRotationDegrees);

    definition.useLocalSpace = Read_Bool(data, "useLocalSpace", definition.useLocalSpace);
}

void EffectAssetRuntimeLoader::Apply_SpawnModule(const json& data, ComputeSpriteEmitterDesc& desc)
{
    desc.spawn.instanceCount = max(1u, Read_UInt(data, "maxParticleCount", desc.spawn.instanceCount));
    desc.spawn.particleSpawn.processSpawnRate = Read_Bool(data, "processSpawnRate", desc.spawn.particleSpawn.processSpawnRate);
    desc.spawn.particleSpawn.processBurstList = Read_Bool(data, "processBurstList", desc.spawn.particleSpawn.processBurstList);
    desc.spawn.particleSpawn.maxActiveCount = desc.spawn.instanceCount;

    const auto spawnRateIter = data.find("spawnRate");
    if (spawnRateIter != data.end())
    {
        desc.spawn.particleSpawn.spawnRate =
            max(0.f, Evaluate_FloatDistributionMax(*spawnRateIter, desc.spawn.particleSpawn.spawnRate));
    }

    const auto spawnRateScaleIter = data.find("spawnRateScale");
    if (spawnRateScaleIter != data.end())
    {
        desc.spawn.particleSpawn.spawnRateScale =
            max(0.f, Evaluate_FloatDistributionMax(*spawnRateScaleIter, desc.spawn.particleSpawn.spawnRateScale));
    }

    const auto burstScaleIter = data.find("burstScale");
    if (burstScaleIter != data.end())
    {
        desc.spawn.particleSpawn.burstScale =
            max(0.f, Evaluate_FloatDistributionMax(*burstScaleIter, desc.spawn.particleSpawn.burstScale));
    }

    const auto burstListIter = data.find("burstList");
    if (burstListIter == data.end() || !burstListIter->is_array())
        return;

    desc.spawn.particleSpawn.bursts.clear();
    desc.spawn.particleSpawn.bursts.reserve(burstListIter->size());
    for (const json& burst : *burstListIter)
    {
        if (!burst.is_object())
            continue;

        desc.spawn.particleSpawn.bursts.push_back(
            PointParticleBurstDesc{
                .time = max(0.f, Read_Float(burst, "time")),
                .count = Read_UInt(burst, "count")
            }
        );
    }
}

bool EffectAssetRuntimeLoader::Build_EmitterDefinition(const json& emitter, EffectEmitterDefinition& outDefinition)
{
    outDefinition = EffectEmitterDefinition{};
    outDefinition.id = Read_UInt(emitter, "id");
    outDefinition.name = Read_String(emitter, "name");
    outDefinition.kind = EffectEmitterKind::ComputeSprite;
    outDefinition.enabled = true;

    ComputeSpriteEmitterDesc desc{};
    desc.drawMode = PointParticleDrawMode::DrawIndexedInstancedIndirect;

    if (const json* required = Find_ModuleData(emitter, "Required"))
        Apply_RequiredModule(*required, outDefinition, desc);

    if (const json* spawn = Find_ModuleData(emitter, "Spawn"))
        Apply_SpawnModule(*spawn, desc);

    if (const json* initialSize = Find_ModuleData(emitter, "InitialSize"))
    {
        const auto sizeIter = initialSize->find("size");
        if (sizeIter != initialSize->end())
        {
            const Vec2 minSize = Evaluate_Vector2DistributionMin(*sizeIter, desc.initialSize.sizeMin);
            const Vec2 maxSize = Evaluate_Vector2DistributionMax(*sizeIter, desc.initialSize.sizeMax);
            desc.initialSize.sizeMin = Vec2{ min(minSize.x, maxSize.x), min(minSize.y, maxSize.y) };
            desc.initialSize.sizeMax = Vec2{ max(minSize.x, maxSize.x), max(minSize.y, maxSize.y) };
        }
    }

    if (const json* initialLocation = Find_ModuleData(emitter, "InitialLocation"))
    {
        const auto locationIter = initialLocation->find("location");
        if (locationIter != initialLocation->end())
        {
            const Vec3 minOffset = Evaluate_Vector3DistributionMin(*locationIter, desc.initialLocation.minOffset);
            const Vec3 maxOffset = Evaluate_Vector3DistributionMax(*locationIter, desc.initialLocation.maxOffset);
            desc.initialLocation.enabled = true;
            desc.initialLocation.minOffset = Vec3{
                min(minOffset.x, maxOffset.x),
                min(minOffset.y, maxOffset.y),
                min(minOffset.z, maxOffset.z)
            };
            desc.initialLocation.maxOffset = Vec3{
                max(minOffset.x, maxOffset.x),
                max(minOffset.y, maxOffset.y),
                max(minOffset.z, maxOffset.z)
            };
        }
    }

    if (const json* sphereLocation = Find_ModuleData(emitter, "SphereLocation"))
    {
        desc.sphereLocation.enabled = true;

        const auto offsetIter = sphereLocation->find("offset");
        if (offsetIter != sphereLocation->end())
            Read_Vec3(*offsetIter, desc.sphereLocation.offset);

        desc.sphereLocation.radius = max(0.f, Read_Float(*sphereLocation, "radius", desc.sphereLocation.radius));
        Read_Enum(*sphereLocation, "spawnMode", desc.sphereLocation.mode);
    }

    if (const json* lifetime = Find_ModuleData(emitter, "Lifetime"))
    {
        const auto lifeTimeIter = lifetime->find("lifeTime");
        if (lifeTimeIter != lifetime->end())
        {
            desc.lifetime.lifeTime = Vec2{
                Evaluate_FloatDistributionMin(*lifeTimeIter, desc.lifetime.lifeTime.x),
                Evaluate_FloatDistributionMax(*lifeTimeIter, desc.lifetime.lifeTime.y)
            };
        }
    }

    if (const json* initialColor = Find_ModuleData(emitter, "InitialColor"))
    {
        const auto colorIter = initialColor->find("color");
        if (colorIter != initialColor->end())
        {
            desc.initialColor.startColorMin = Evaluate_ColorDistributionMin(*colorIter, desc.initialColor.startColorMin);
            desc.initialColor.startColorMax = Evaluate_ColorDistributionMax(*colorIter, desc.initialColor.startColorMax);
        }

        const auto alphaIter = initialColor->find("alpha");
        if (alphaIter != initialColor->end())
        {
            desc.initialColor.startColorMin.w =
                clamp(Evaluate_FloatDistributionMin(*alphaIter, desc.initialColor.startColorMin.w), 0.f, 1.f);
            desc.initialColor.startColorMax.w =
                clamp(Evaluate_FloatDistributionMax(*alphaIter, desc.initialColor.startColorMax.w), 0.f, 1.f);
        }

        desc.colorOverLife.endColorMin = desc.initialColor.startColorMin;
        desc.colorOverLife.endColorMax = desc.initialColor.startColorMax;
    }

    if (const json* colorOverLife = Find_ModuleData(emitter, "ColorOverLife"))
    {
        const auto colorIter = colorOverLife->find("colorOverLife");
        if (colorIter != colorOverLife->end())
        {
            desc.colorOverLife.endColorMin = Evaluate_ColorDistributionMin(*colorIter, desc.colorOverLife.endColorMin);
            desc.colorOverLife.endColorMax = Evaluate_ColorDistributionMax(*colorIter, desc.colorOverLife.endColorMax);
        }

        const auto alphaIter = colorOverLife->find("alphaOverLife");
        if (alphaIter != colorOverLife->end())
        {
            desc.colorOverLife.endColorMin.w =
                clamp(Evaluate_FloatDistributionMin(*alphaIter, desc.colorOverLife.endColorMin.w), 0.f, 1.f);
            desc.colorOverLife.endColorMax.w =
                clamp(Evaluate_FloatDistributionMax(*alphaIter, desc.colorOverLife.endColorMax.w), 0.f, 1.f);
        }
    }

    if (const json* subUvFrameOverLife = Find_ModuleData(emitter, "SubUVFrameOverLife"))
    {
        desc.subUVFrameOverLife.enabled = true;
        desc.subUVFrameOverLife.startFrame = Read_UInt(*subUvFrameOverLife, "startFrame", desc.subUVFrameOverLife.startFrame);
        desc.subUVFrameOverLife.endFrame = Read_UInt(*subUvFrameOverLife, "endFrame", desc.subUVFrameOverLife.endFrame);
        desc.subUVFrameOverLife.loop = Read_Bool(*subUvFrameOverLife, "loop", desc.subUVFrameOverLife.loop);
        Read_Enum(*subUvFrameOverLife, "playbackMode", desc.subUVFrameOverLife.playbackMode);
        desc.subUVFrameOverLife.framesPerSecond =
            max(0.f, Read_Float(*subUvFrameOverLife, "framesPerSecond", desc.subUVFrameOverLife.framesPerSecond));

        const auto frameIndexIter = subUvFrameOverLife->find("frameIndex");
        if (frameIndexIter != subUvFrameOverLife->end())
            Fill_SubUVFrameCurvePayload(desc.subUVFrameOverLife, *frameIndexIter);

        const uint32 frameCount = max(1u, desc.required.material.subUVRows * desc.required.material.subUVCols);
        const uint32 lastFrame = frameCount - 1u;
        desc.subUVFrameOverLife.startFrame = min(desc.subUVFrameOverLife.startFrame, lastFrame);
        desc.subUVFrameOverLife.endFrame = min(desc.subUVFrameOverLife.endFrame, lastFrame);

        if (0u == Read_UInt(*subUvFrameOverLife, "startFrame") &&
            0u == Read_UInt(*subUvFrameOverLife, "endFrame") &&
            frameCount > 1u)
            desc.subUVFrameOverLife.endFrame = lastFrame;
    }

    if (const json* sizeByLife = Find_ModuleData(emitter, "SizeByLife"))
    {
        desc.sizeByLife.enabled = true;
        desc.sizeByLife.multiplyXStart = Read_Float(*sizeByLife, "multiplyXStart", desc.sizeByLife.multiplyXStart);
        desc.sizeByLife.multiplyXEnd = Read_Float(*sizeByLife, "multiplyXEnd", desc.sizeByLife.multiplyXEnd);
        desc.sizeByLife.multiplyYStart = Read_Float(*sizeByLife, "multiplyYStart", desc.sizeByLife.multiplyYStart);
        desc.sizeByLife.multiplyYEnd = Read_Float(*sizeByLife, "multiplyYEnd", desc.sizeByLife.multiplyYEnd);
    }

    if (const json* initialVelocity = Find_ModuleData(emitter, "InitialVelocity"))
    {
        desc.motion.enabled = true;
        desc.motion.initialVelocityEnabled = true;
        desc.motion.initialVelocityInWorldSpace = Read_Bool(*initialVelocity, "inWorldSpace", desc.motion.initialVelocityInWorldSpace);

        const auto velocityIter = initialVelocity->find("velocity");
        if (velocityIter != initialVelocity->end())
        {
            desc.motion.initialVelocityMin = Evaluate_Vector3DistributionMin(*velocityIter, desc.motion.initialVelocityMin);
            desc.motion.initialVelocityMax = Evaluate_Vector3DistributionMax(*velocityIter, desc.motion.initialVelocityMax);
        }
    }

    if (const json* initialRadialVelocity = Find_ModuleData(emitter, "InitialRadialVelocity"))
    {
        desc.motion.enabled = true;
        desc.motion.initialRadialVelocityEnabled = true;
        desc.motion.initialRadialVelocityInWorldSpace =
            Read_Bool(*initialRadialVelocity, "inWorldSpace", desc.motion.initialRadialVelocityInWorldSpace);

        const auto radialPivotIter = initialRadialVelocity->find("radialPivot");
        if (radialPivotIter != initialRadialVelocity->end())
            Read_Vec3(*radialPivotIter, desc.motion.radialPivot);

        const auto speedIter = initialRadialVelocity->find("speed");
        if (speedIter != initialRadialVelocity->end())
        {
            desc.motion.radialSpeed = Vec2{
                Evaluate_FloatDistributionMin(*speedIter, desc.motion.radialSpeed.x),
                Evaluate_FloatDistributionMax(*speedIter, desc.motion.radialSpeed.y)
            };
        }
    }

    if (const json* acceleration = Find_ModuleData(emitter, "Acceleration"))
    {
        desc.motion.enabled = true;
        const auto accelerationIter = acceleration->find("acceleration");
        if (accelerationIter != acceleration->end())
        {
            desc.motion.accelerationMin = Evaluate_Vector3DistributionMin(*accelerationIter, desc.motion.accelerationMin);
            desc.motion.accelerationMax = Evaluate_Vector3DistributionMax(*accelerationIter, desc.motion.accelerationMax);
        }
    }

    if (const json* drag = Find_ModuleData(emitter, "Drag"))
    {
        desc.motion.enabled = true;
        const auto dragIter = drag->find("drag");
        if (dragIter != drag->end())
        {
            desc.motion.drag = Vec2{
                max(0.f, Evaluate_FloatDistributionMin(*dragIter, desc.motion.drag.x)),
                max(0.f, Evaluate_FloatDistributionMax(*dragIter, desc.motion.drag.y))
            };
        }
    }

    if (const json* velocityOverLife = Find_ModuleData(emitter, "VelocityOverLife"))
    {
        desc.motion.enabled = true;
        desc.motion.velocityScaleByLife = Vec2{
            Read_Float(*velocityOverLife, "scaleStart", desc.motion.velocityScaleByLife.x),
            Read_Float(*velocityOverLife, "scaleEnd", desc.motion.velocityScaleByLife.y)
        };
    }

    if (const json* initialRotation = Find_ModuleData(emitter, "InitialRotation"))
    {
        desc.rotation.enabled = true;
        const auto rotationIter = initialRotation->find("rotationDegrees");
        if (rotationIter != initialRotation->end())
        {
            desc.rotation.initialRotationDegrees = Vec2{
                Evaluate_FloatDistributionMin(*rotationIter, desc.rotation.initialRotationDegrees.x),
                Evaluate_FloatDistributionMax(*rotationIter, desc.rotation.initialRotationDegrees.y)
            };
        }
    }

    if (const json* rotationOverLife = Find_ModuleData(emitter, "RotationOverLife"))
    {
        desc.rotation.enabled = true;
        desc.rotation.rotationOverLifeDegrees = Vec2{
            Read_Float(*rotationOverLife, "rotationStartDegrees", desc.rotation.rotationOverLifeDegrees.x),
            Read_Float(*rotationOverLife, "rotationEndDegrees", desc.rotation.rotationOverLifeDegrees.y)
        };
    }

    if (const json* initialRotationRate = Find_ModuleData(emitter, "InitialRotationRate"))
    {
        desc.rotation.enabled = true;
        const auto rotationRateIter = initialRotationRate->find("rotationRateDegrees");
        if (rotationRateIter != initialRotationRate->end())
        {
            desc.rotation.initialRotationRateDegrees = Vec2{
                Evaluate_FloatDistributionMin(*rotationRateIter, desc.rotation.initialRotationRateDegrees.x),
                Evaluate_FloatDistributionMax(*rotationRateIter, desc.rotation.initialRotationRateDegrees.y)
            };
        }
    }

    if (const json* rotationRateScaleByLife = Find_ModuleData(emitter, "RotationRateScaleByLife"))
    {
        desc.rotation.enabled = true;
        desc.rotation.rotationRateScaleByLife = Vec2{
            Read_Float(*rotationRateScaleByLife, "scaleStart", desc.rotation.rotationRateScaleByLife.x),
            Read_Float(*rotationRateScaleByLife, "scaleEnd", desc.rotation.rotationRateScaleByLife.y)
        };
    }

    outDefinition.concreteDesc = desc;
    return true;
}

bool EffectAssetRuntimeLoader::Build_TrailEmitterDefinition(const json& emitter, EffectEmitterDefinition& outDefinition)
{
    outDefinition = EffectEmitterDefinition{};
    outDefinition.id = Read_UInt(emitter, "id");
    outDefinition.name = Read_String(emitter, "name");
    outDefinition.kind = EffectEmitterKind::ComputeTrail;
    outDefinition.enabled = true;

    ComputeTrailEmitterDesc desc{};

    const auto typeDataIter = emitter.find("typeData");
    if (typeDataIter != emitter.end() && typeDataIter->is_object())
    {
        const auto dataIter = typeDataIter->find("data");
        if (dataIter != typeDataIter->end() && dataIter->is_object())
        {
            const json& typeData = *dataIter;
            desc.width = max(0.001f, Read_Float(typeData, "width", desc.width));
            desc.segmentLifetime = max(0.001f, Read_Float(typeData, "segmentLifetime", desc.segmentLifetime));
            desc.historyCount = max(2u, Read_UInt(typeData, "historyCount", desc.historyCount));
            desc.uvTiling = max(0.001f, Read_Float(typeData, "uvTiling", desc.uvTiling));
            desc.maxTrailLength = max(0.f, Read_Float(typeData, "maxTrailLength", desc.maxTrailLength));
            desc.tailFadeLength = max(0.f, Read_Float(typeData, "tailFadeLength", desc.tailFadeLength));
            desc.sampleSpacing = max(0.001f, Read_Float(typeData, "sampleSpacing", desc.sampleSpacing));
            desc.curveSubdivision = max(0u, Read_UInt(typeData, "curveSubdivision", desc.curveSubdivision));
            desc.smoothTangent = Read_Bool(typeData, "smoothTangent", desc.smoothTangent);
            desc.sideFade = clamp(Read_Float(typeData, "sideFade", desc.sideFade), 0.001f, 0.49f);
        }
    }

    if (const json* required = Find_ModuleData(emitter, "Required"))
        Apply_RequiredModule(*required, outDefinition, desc);

    if (const json* lifetime = Find_ModuleData(emitter, "Lifetime"))
    {
        const auto lifeTimeIter = lifetime->find("lifeTime");
        if (lifeTimeIter != lifetime->end())
        {
            desc.lifetime.lifeTime = Vec2{
                Evaluate_FloatDistributionMin(*lifeTimeIter, desc.lifetime.lifeTime.x),
                Evaluate_FloatDistributionMax(*lifeTimeIter, desc.lifetime.lifeTime.y)
            };
            desc.segmentLifetime = max(0.001f, desc.lifetime.lifeTime.y);
        }
    }

    if (const json* initialColor = Find_ModuleData(emitter, "InitialColor"))
    {
        const auto colorIter = initialColor->find("color");
        if (colorIter != initialColor->end())
        {
            desc.initialColor.startColorMin = Evaluate_ColorDistributionMin(*colorIter, desc.initialColor.startColorMin);
            desc.initialColor.startColorMax = Evaluate_ColorDistributionMax(*colorIter, desc.initialColor.startColorMax);
        }

        const auto alphaIter = initialColor->find("alpha");
        if (alphaIter != initialColor->end())
        {
            desc.initialColor.startColorMin.w =
                clamp(Evaluate_FloatDistributionMin(*alphaIter, desc.initialColor.startColorMin.w), 0.f, 1.f);
            desc.initialColor.startColorMax.w =
                clamp(Evaluate_FloatDistributionMax(*alphaIter, desc.initialColor.startColorMax.w), 0.f, 1.f);
        }

        desc.colorOverLife.endColorMin = desc.initialColor.startColorMin;
        desc.colorOverLife.endColorMax = desc.initialColor.startColorMax;
    }

    if (const json* colorOverLife = Find_ModuleData(emitter, "ColorOverLife"))
    {
        const auto colorIter = colorOverLife->find("colorOverLife");
        if (colorIter != colorOverLife->end())
        {
            desc.colorOverLife.endColorMin = Evaluate_ColorDistributionMin(*colorIter, desc.colorOverLife.endColorMin);
            desc.colorOverLife.endColorMax = Evaluate_ColorDistributionMax(*colorIter, desc.colorOverLife.endColorMax);
        }

        const auto alphaIter = colorOverLife->find("alphaOverLife");
        if (alphaIter != colorOverLife->end())
        {
            desc.colorOverLife.endColorMin.w =
                clamp(Evaluate_FloatDistributionMin(*alphaIter, desc.colorOverLife.endColorMin.w), 0.f, 1.f);
            desc.colorOverLife.endColorMax.w =
                clamp(Evaluate_FloatDistributionMax(*alphaIter, desc.colorOverLife.endColorMax.w), 0.f, 1.f);
        }
    }

    if (const json* sizeByLife = Find_ModuleData(emitter, "SizeByLife"))
    {
        desc.sizeByLife.enabled = true;
        desc.sizeByLife.multiplyXStart = Read_Float(*sizeByLife, "multiplyXStart", desc.sizeByLife.multiplyXStart);
        desc.sizeByLife.multiplyXEnd = Read_Float(*sizeByLife, "multiplyXEnd", desc.sizeByLife.multiplyXEnd);
        desc.sizeByLife.multiplyYStart = Read_Float(*sizeByLife, "multiplyYStart", desc.sizeByLife.multiplyYStart);
        desc.sizeByLife.multiplyYEnd = Read_Float(*sizeByLife, "multiplyYEnd", desc.sizeByLife.multiplyYEnd);
    }

    if (const json* spawnPerUnit = Find_ModuleData(emitter, "SpawnPerUnit"))
    {
        desc.spawnPerUnit.enabled = Read_Bool(*spawnPerUnit, "enabled", true);

        const auto spawnPerUnitIter = spawnPerUnit->find("spawnPerUnit");
        if (spawnPerUnitIter != spawnPerUnit->end())
        {
            desc.spawnPerUnit.spawnPerUnit =
                max(0.f, Evaluate_FloatDistributionMax(*spawnPerUnitIter, desc.spawnPerUnit.spawnPerUnit));
        }

        desc.spawnPerUnit.unitScalar = max(0.0001f, Read_Float(*spawnPerUnit, "unitScalar", desc.spawnPerUnit.unitScalar));
        desc.spawnPerUnit.movementTolerance =
            max(0.f, Read_Float(*spawnPerUnit, "movementTolerance", desc.spawnPerUnit.movementTolerance));
        desc.spawnPerUnit.maxFrameDistance =
            max(0.f, Read_Float(*spawnPerUnit, "maxFrameDistance", desc.spawnPerUnit.maxFrameDistance));
    }

    outDefinition.concreteDesc = desc;
    return true;
}

bool EffectAssetRuntimeLoader::Try_MakeResourceRelativePath(
    const fs::path& fullPath,
    const char* logContext,
    const string& assetKey,
    wstring& outRelativePath)
{
    outRelativePath.clear();

    const wstring assetRoot = GAME->Get_AssetRoot();
    if (assetRoot.empty())
    {
        LOG_ERROR(
            "EffectAssetRuntimeLoader {} failed: asset root is empty. key='{}'",
            logContext,
            assetKey
        );
        return false;
    }

    const fs::path assetRootPath = fs::path(assetRoot).lexically_normal();
    const fs::path normalizedFullPath = fullPath.lexically_normal();

    error_code errorCode{};
    const fs::path relativePath = fs::relative(normalizedFullPath, assetRootPath, errorCode).lexically_normal();
    if (errorCode || relativePath.empty() || relativePath.native().starts_with(L".."))
    {
        LOG_ERROR(
            "EffectAssetRuntimeLoader {} failed: resolved path is outside asset root. key='{}', path='{}', root='{}'",
            logContext,
            assetKey,
            To_LogPath(normalizedFullPath),
            To_LogPath(assetRootPath)
        );
        return false;
    }

    outRelativePath = relativePath.wstring();
    return true;
}

bool EffectAssetRuntimeLoader::Resolve_EffectAssetPathByGuid(const string& effectAssetGuid, wstring& outEffectAssetPath)
{
    outEffectAssetPath.clear();

    if (effectAssetGuid.empty())
    {
        LOG_ERROR("Empty guid");
        return false;
    }

    const AssetMeta* assetMeta = GAME->Find_AssetByGUID(effectAssetGuid);
    if (nullptr == assetMeta)
    {
        LOG_WARN("Asset not found. guid='{}'", effectAssetGuid);
        return false;
    }

    if (assetMeta->type != "Effect")
    {
        LOG_ERROR("Asset type mismatch. guid='{}', type='{}'", effectAssetGuid, assetMeta->type);
        return false;
    }

    const wstring resolvedPath = GAME->Resolve_AssetPath(effectAssetGuid);
    if (resolvedPath.empty())
    {
        LOG_WARN("Resolved path is empty. guid='{}'", effectAssetGuid);
        return false;
    }

    return Try_MakeResourceRelativePath(fs::path(resolvedPath), "guid resolve", effectAssetGuid, outEffectAssetPath);
}

string EffectAssetRuntimeLoader::Normalize_EffectNameKey(const wstring& effectName)
{
    const string effectJsonSuffix = ".effect.json";
    string key = String::ToLowerCopy(String::ToString(fs::path(effectName).filename().wstring()));
    if (key.ends_with(effectJsonSuffix))
        key.erase(key.size() - effectJsonSuffix.size());
    else
        key = fs::path(key).stem().generic_string();

    return key;
}

bool EffectAssetRuntimeLoader::Is_EffectDefinitionAssetPath(const wstring& relativePath)
{
    const string lowerPath = String::ToLowerCopy(fs::path(relativePath).generic_string());
    return lowerPath.starts_with("effects/assets/") && lowerPath.ends_with(".effect.json");
}

bool EffectAssetRuntimeLoader::Find_EffectAssetGuidByName(const wstring& effectName, string& outEffectAssetGuid)
{
    outEffectAssetGuid.clear();

    const string effectNameKey = Normalize_EffectNameKey(effectName);
    if (effectNameKey.empty())
    {
        LOG_ERROR("Empty effect name");
        return false;
    }

    AssetMeta matchedAsset{};
    bool hasMatch = false;
    const vector<AssetMeta> effectAssets = GAME->Get_AssetsByType("Effect");
    for (const AssetMeta& assetMeta : effectAssets)
    {
        if (!Is_EffectDefinitionAssetPath(assetMeta.relativePath))
            continue;

        const string candidateKey = Normalize_EffectNameKey(fs::path(assetMeta.relativePath).filename().wstring());
        if (candidateKey != effectNameKey)
            continue;

        if (hasMatch)
        {
            LOG_ERROR(
                "Duplicate effect asset name. name='{}', first='{}', duplicate='{}'",
                String::ToString(effectName),
                To_LogPath(matchedAsset.relativePath),
                To_LogPath(assetMeta.relativePath)
            );
            return false;
        }

        matchedAsset = assetMeta;
        hasMatch = true;
    }

    if (!hasMatch)
    {
        LOG_WARN("Effect asset name not found. name='{}'", String::ToString(effectName));
        return false;
    }

    if (matchedAsset.guid.empty())
    {
        LOG_ERROR(
            "Effect asset guid is empty. name='{}', path='{}'",
            String::ToString(effectName),
            To_LogPath(matchedAsset.relativePath)
        );
        return false;
    }

    outEffectAssetGuid = matchedAsset.guid;
    return true;
}

bool EffectAssetRuntimeLoader::Load_JsonRoot(const wstring& effectAssetPath, json& outRoot)
{
    if (effectAssetPath.empty())
    {
        LOG_ERROR("Empty asset path");
        return false;
    }

    const wstring assetRoot = GAME->Get_AssetRoot();
    if (assetRoot.empty())
    {
        LOG_ERROR("Asset root is empty");
        return false;
    }

    const fs::path fullPath = (fs::path(assetRoot) / effectAssetPath).lexically_normal();
    if (!fs::exists(fullPath))
    {
        LOG_WARN("File not found. path='{}'", To_LogPath(fullPath));
        return false;
    }

    try
    {
        ifstream file(fullPath);
        if (!file.is_open())
        {
            LOG_ERROR("Open failed. path='{}'", To_LogPath(fullPath));
            return false;
        }

        file >> outRoot;
        return true;
    }
    catch (const exception& exception)
    {
        LOG_ERROR(
            "Json parse failed. path='{}', error='{}'",
            To_LogPath(fullPath),
            exception.what()
        );
        return false;
    }
}

NS_END
