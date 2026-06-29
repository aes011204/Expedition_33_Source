#pragma once
#include "Client_Defines.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Client)

// MaterialScalarModulation은 legacy module/serialized/shader payload 이름이다.
// shader scalar payload 호환성은 이 이름으로 유지하고, Vec2/Color/refraction 같은 확장 material parameter는
// 아래 MaterialParameter/Uniform resolve helper가 처리한다.

//## Types::ShaderPayload
struct EffectMaterialScalarModulationShaderPayload
{
    static constexpr uint32 maxModulators{ EffectMaterialScalarModulationRuntimeDesc::kMaxModulators };

    Vec4 params{};                     // x: count, y: emitter phase, zw: reserved.
    array<Vec4, maxModulators> meta{}; // x: enabled, y: target, z: time source, w: key count.
    array<Vec4, maxModulators> keyTimes{};
    array<Vec4, maxModulators> keyTimesBlock1{};
    array<Vec4, maxModulators> keyValues{};
    array<Vec4, maxModulators> keyValuesBlock1{};
    array<Vec4, maxModulators> keyArriveTangents{};
    array<Vec4, maxModulators> keyArriveTangentsBlock1{};
    array<Vec4, maxModulators> keyLeaveTangents{};
    array<Vec4, maxModulators> keyLeaveTangentsBlock1{};
    array<Vec4, maxModulators> keyModes{};
    array<Vec4, maxModulators> keyModesBlock1{};
};

//## Helper::CurveEvaluation
inline float Compute_MaterialScalarModulationPhase(float elapsedTime, float duration)
{
    const float safeDuration = max(duration, 0.0001f);
    const float normalizedTime = max(0.f, elapsedTime) / safeDuration;
    return normalizedTime - floor(normalizedTime);
}

inline uint32 Resolve_MaterialModulationSeedSalt(const PointParticleRandomSeedRuntimeDesc& seed, uint32 effectPlaybackSeed)
{
    uint32 salt = seed.manualSeedEnabled ? seed.seed : 0u;
    if (seed.useInstanceSeed)
        salt += effectPlaybackSeed;
    return salt;
}

inline float Hash_MaterialModulation01(uint32 seed)
{
    seed ^= 2747636419u;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    return static_cast<float>(seed & 0x00FFFFFFu) / 16777215.f;
}

inline float Sample_MaterialModulationRange(float minValue, float maxValue, uint32 seed)
{
    const float lower = min(minValue, maxValue);
    const float upper = max(minValue, maxValue);
    return lower + (upper - lower) * Hash_MaterialModulation01(seed);
}

inline float Read_MaterialScalarModulationComponent(const Vec4& values, uint32 index)
{
    switch (index)
    {
    case 1:
        return values.y;
    case 2:
        return values.z;
    case 3:
        return values.w;
    case 0:
    default:
        return values.x;
    }
}

inline float Read_MaterialScalarModulationComponent(const Vec4& valuesBlock0, const Vec4& valuesBlock1, uint32 index)
{
    if (index < 4u)
        return Read_MaterialScalarModulationComponent(valuesBlock0, index);

    return Read_MaterialScalarModulationComponent(valuesBlock1, index - 4u);
}

inline float Evaluate_MaterialScalarModulationCurve(
    const EffectMaterialScalarModulationCurveDesc& curve,
    float phase,
    float fallbackValue)
{
    if (!curve.enabled || curve.keyCount == 0)
        return fallbackValue;

    const uint32 keyCount = min(kEffectDistributionCurveMaxKeys, curve.keyCount);
    if (keyCount == 1)
        return Read_MaterialScalarModulationComponent(curve.keyValues, curve.keyValuesBlock1, 0);

    const float clampedPhase = clamp(phase, 0.f, 1.f);
    for (uint32 index = 1; index < keyCount; ++index)
    {
        const float rightTime = Read_MaterialScalarModulationComponent(curve.keyTimes, curve.keyTimesBlock1, index);
        if (clampedPhase > rightTime)
            continue;

        const uint32 leftIndex = index - 1;
        const float leftTime = Read_MaterialScalarModulationComponent(curve.keyTimes, curve.keyTimesBlock1, leftIndex);
        const float leftValue = Read_MaterialScalarModulationComponent(curve.keyValues, curve.keyValuesBlock1, leftIndex);
        const float rightValue = Read_MaterialScalarModulationComponent(curve.keyValues, curve.keyValuesBlock1, index);
        const float width = max(0.0001f, rightTime - leftTime);
        const float ratio = clamp((clampedPhase - leftTime) / width, 0.f, 1.f);
        const int32 mode = static_cast<int32>(Read_MaterialScalarModulationComponent(curve.keyModes, curve.keyModesBlock1, leftIndex));

        if (mode == 0)
            return leftValue;

        if (mode == 2)
        {
            const float leftLeave = Read_MaterialScalarModulationComponent(curve.keyLeaveTangents, curve.keyLeaveTangentsBlock1, leftIndex) * width;
            const float rightArrive = Read_MaterialScalarModulationComponent(curve.keyArriveTangents, curve.keyArriveTangentsBlock1, index) * width;
            const float t2 = ratio * ratio;
            const float t3 = t2 * ratio;
            return (2.f * t3 - 3.f * t2 + 1.f) * leftValue
                   + (t3 - 2.f * t2 + ratio) * leftLeave
                   + (-2.f * t3 + 3.f * t2) * rightValue
                   + (t3 - t2) * rightArrive;
        }

        return leftValue + (rightValue - leftValue) * ratio;
    }

    return Read_MaterialScalarModulationComponent(curve.keyValues, curve.keyValuesBlock1, keyCount - 1);
}

inline float Evaluate_CoreColorRgbModulationComponent(
    const EffectMaterialCoreColorRgbModulationCurveDesc& curve,
    const Vec4& keyValues,
    const Vec4& keyValuesBlock1,
    const Vec4& keyArriveTangents,
    const Vec4& keyArriveTangentsBlock1,
    const Vec4& keyLeaveTangents,
    const Vec4& keyLeaveTangentsBlock1,
    float phase,
    float fallbackValue)
{
    if (!curve.enabled || curve.keyCount == 0)
        return fallbackValue;

    const uint32 keyCount = min(kEffectDistributionCurveMaxKeys, curve.keyCount);
    if (keyCount == 1)
        return Read_MaterialScalarModulationComponent(keyValues, keyValuesBlock1, 0);

    const float clampedPhase = clamp(phase, 0.f, 1.f);
    for (uint32 index = 1; index < keyCount; ++index)
    {
        const float rightTime = Read_MaterialScalarModulationComponent(curve.keyTimes, curve.keyTimesBlock1, index);
        if (clampedPhase > rightTime)
            continue;

        const uint32 leftIndex = index - 1;
        const float leftTime = Read_MaterialScalarModulationComponent(curve.keyTimes, curve.keyTimesBlock1, leftIndex);
        const float leftValue = Read_MaterialScalarModulationComponent(keyValues, keyValuesBlock1, leftIndex);
        const float rightValue = Read_MaterialScalarModulationComponent(keyValues, keyValuesBlock1, index);
        const float width = max(0.0001f, rightTime - leftTime);
        const float ratio = clamp((clampedPhase - leftTime) / width, 0.f, 1.f);
        const int32 mode = static_cast<int32>(Read_MaterialScalarModulationComponent(curve.keyModes, curve.keyModesBlock1, leftIndex));

        if (mode == 0)
            return leftValue;

        if (mode == 2)
        {
            const float leftLeave = Read_MaterialScalarModulationComponent(keyLeaveTangents, keyLeaveTangentsBlock1, leftIndex) * width;
            const float rightArrive = Read_MaterialScalarModulationComponent(keyArriveTangents, keyArriveTangentsBlock1, index) * width;
            const float t2 = ratio * ratio;
            const float t3 = t2 * ratio;
            return (2.f * t3 - 3.f * t2 + 1.f) * leftValue
                   + (t3 - 2.f * t2 + ratio) * leftLeave
                   + (-2.f * t3 + 3.f * t2) * rightValue
                   + (t3 - t2) * rightArrive;
        }

        return leftValue + (rightValue - leftValue) * ratio;
    }

    return Read_MaterialScalarModulationComponent(keyValues, keyValuesBlock1, keyCount - 1);
}

inline Vec3 Evaluate_CoreColorRgbModulationCurve(
    const EffectMaterialCoreColorRgbModulationCurveDesc& curve,
    float phase,
    const Vec3& fallbackValue,
    uint32 effectPlaybackSeed)
{
    if (curve.enabled && curve.uniformEnabled)
    {
        const uint32 seed = Resolve_MaterialModulationSeedSalt(curve.uniformSeed, effectPlaybackSeed);
        return Vec3{
            Sample_MaterialModulationRange(curve.uniformMin.x, curve.uniformMax.x, seed + 1u),
            Sample_MaterialModulationRange(curve.uniformMin.y, curve.uniformMax.y, seed + 2u),
            Sample_MaterialModulationRange(curve.uniformMin.z, curve.uniformMax.z, seed + 3u)
        };
    }

    return Vec3{
        Evaluate_CoreColorRgbModulationComponent(
            curve,
            curve.keyValuesR,
            curve.keyValuesRBlock1,
            curve.keyArriveTangentsR,
            curve.keyArriveTangentsRBlock1,
            curve.keyLeaveTangentsR,
            curve.keyLeaveTangentsRBlock1,
            phase,
            fallbackValue.x
        ),
        Evaluate_CoreColorRgbModulationComponent(
            curve,
            curve.keyValuesG,
            curve.keyValuesGBlock1,
            curve.keyArriveTangentsG,
            curve.keyArriveTangentsGBlock1,
            curve.keyLeaveTangentsG,
            curve.keyLeaveTangentsGBlock1,
            phase,
            fallbackValue.y
        ),
        Evaluate_CoreColorRgbModulationComponent(
            curve,
            curve.keyValuesB,
            curve.keyValuesBBlock1,
            curve.keyArriveTangentsB,
            curve.keyArriveTangentsBBlock1,
            curve.keyLeaveTangentsB,
            curve.keyLeaveTangentsBBlock1,
            phase,
            fallbackValue.z
        )
    };
}

inline float Evaluate_MaterialVec2ModulationComponent(
    const EffectMaterialVec2ModulationCurveDesc& curve,
    const Vec4& keyValues,
    const Vec4& keyValuesBlock1,
    const Vec4& keyArriveTangents,
    const Vec4& keyArriveTangentsBlock1,
    const Vec4& keyLeaveTangents,
    const Vec4& keyLeaveTangentsBlock1,
    float phase,
    float fallbackValue)
{
    if (!curve.enabled || curve.keyCount == 0)
        return fallbackValue;

    const uint32 keyCount = min(kEffectDistributionCurveMaxKeys, curve.keyCount);
    if (keyCount == 1)
        return Read_MaterialScalarModulationComponent(keyValues, keyValuesBlock1, 0);

    const float clampedPhase = clamp(phase, 0.f, 1.f);
    for (uint32 index = 1; index < keyCount; ++index)
    {
        const float rightTime = Read_MaterialScalarModulationComponent(curve.keyTimes, curve.keyTimesBlock1, index);
        if (clampedPhase > rightTime)
            continue;

        const uint32 leftIndex = index - 1;
        const float leftTime = Read_MaterialScalarModulationComponent(curve.keyTimes, curve.keyTimesBlock1, leftIndex);
        const float leftValue = Read_MaterialScalarModulationComponent(keyValues, keyValuesBlock1, leftIndex);
        const float rightValue = Read_MaterialScalarModulationComponent(keyValues, keyValuesBlock1, index);
        const float width = max(0.0001f, rightTime - leftTime);
        const float ratio = clamp((clampedPhase - leftTime) / width, 0.f, 1.f);
        const int32 mode = static_cast<int32>(Read_MaterialScalarModulationComponent(curve.keyModes, curve.keyModesBlock1, leftIndex));

        if (mode == 0)
            return leftValue;

        if (mode == 2)
        {
            const float leftLeave = Read_MaterialScalarModulationComponent(keyLeaveTangents, keyLeaveTangentsBlock1, leftIndex) * width;
            const float rightArrive = Read_MaterialScalarModulationComponent(keyArriveTangents, keyArriveTangentsBlock1, index) * width;
            const float t2 = ratio * ratio;
            const float t3 = t2 * ratio;
            return (2.f * t3 - 3.f * t2 + 1.f) * leftValue
                   + (t3 - 2.f * t2 + ratio) * leftLeave
                   + (-2.f * t3 + 3.f * t2) * rightValue
                   + (t3 - t2) * rightArrive;
        }

        return leftValue + (rightValue - leftValue) * ratio;
    }

    return Read_MaterialScalarModulationComponent(keyValues, keyValuesBlock1, keyCount - 1);
}

inline Vec2 Evaluate_MaterialVec2ModulationCurve(
    const EffectMaterialVec2ModulationCurveDesc& curve,
    float phase,
    const Vec2& fallbackValue)
{
    return Vec2{
        Evaluate_MaterialVec2ModulationComponent(
            curve,
            curve.keyValuesX,
            curve.keyValuesXBlock1,
            curve.keyArriveTangentsX,
            curve.keyArriveTangentsXBlock1,
            curve.keyLeaveTangentsX,
            curve.keyLeaveTangentsXBlock1,
            phase,
            fallbackValue.x
        ),
        Evaluate_MaterialVec2ModulationComponent(
            curve,
            curve.keyValuesY,
            curve.keyValuesYBlock1,
            curve.keyArriveTangentsY,
            curve.keyArriveTangentsYBlock1,
            curve.keyLeaveTangentsY,
            curve.keyLeaveTangentsYBlock1,
            phase,
            fallbackValue.y
        )
    };
}

//## Helper::MaterialResolve
inline void Apply_MaterialScalarModulationMultiplier(
    EffectRequiredMaterialRuntimeDesc& material,
    EffectMaterialScalarModulationTarget target,
    float multiplier)
{
    switch (target)
    {
    case EffectMaterialScalarModulationTarget::OpacityPower:
        material.opacityPower *= multiplier;
        break;
    case EffectMaterialScalarModulationTarget::NoiseStrength:
        material.noiseStrength *= multiplier;
        break;
    case EffectMaterialScalarModulationTarget::AlphaErosion:
        material.alphaErosion *= multiplier;
        break;
    case EffectMaterialScalarModulationTarget::AlphaCutoff:
        material.alphaCutoff *= multiplier;
        break;
    case EffectMaterialScalarModulationTarget::CoreIntensity:
        material.coreEmissive.coreIntensity *= multiplier;
        break;
    case EffectMaterialScalarModulationTarget::OuterIntensity:
        material.coreEmissive.outerIntensity *= multiplier;
        break;
    case EffectMaterialScalarModulationTarget::CoreColor:
        material.coreEmissive.coreColor.x *= multiplier;
        material.coreEmissive.coreColor.y *= multiplier;
        material.coreEmissive.coreColor.z *= multiplier;
        break;
    case EffectMaterialScalarModulationTarget::RefractionIntensity:
        material.refractionIntensity *= multiplier;
        break;
    case EffectMaterialScalarModulationTarget::MainUVScrollSpeedScale:
        material.mainUVScrollSpeed.x *= multiplier;
        material.mainUVScrollSpeed.y *= multiplier;
        break;
    case EffectMaterialScalarModulationTarget::NoiseUVScrollSpeedScale:
        material.noiseUVScrollSpeed.x *= multiplier;
        material.noiseUVScrollSpeed.y *= multiplier;
        break;
    case EffectMaterialScalarModulationTarget::MaskUVScrollSpeedScale:
        material.maskUVScrollSpeed.x *= multiplier;
        material.maskUVScrollSpeed.y *= multiplier;
        break;
    case EffectMaterialScalarModulationTarget::FlowUVScrollSpeedScale:
        material.flowUVScrollSpeed.x *= multiplier;
        material.flowUVScrollSpeed.y *= multiplier;
        break;
    case EffectMaterialScalarModulationTarget::Intensity:
    default:
        material.intensity *= multiplier;
        break;
    }
}

inline void Apply_MaterialVec2ModulationAdd(
    EffectRequiredMaterialRuntimeDesc& material,
    EffectMaterialVec2ModulationTarget target,
    const Vec2& value)
{
    switch (target)
    {
    case EffectMaterialVec2ModulationTarget::NoiseUVOffset:
        material.noiseUVOffset.x += value.x;
        material.noiseUVOffset.y += value.y;
        break;
    case EffectMaterialVec2ModulationTarget::MaskUVOffset:
        material.maskUVOffset.x += value.x;
        material.maskUVOffset.y += value.y;
        break;
    case EffectMaterialVec2ModulationTarget::FlowUVOffset:
        material.flowUVOffset.x += value.x;
        material.flowUVOffset.y += value.y;
        break;
    case EffectMaterialVec2ModulationTarget::MainUVOffset:
    default:
        material.mainUVOffset.x += value.x;
        material.mainUVOffset.y += value.y;
        break;
    }
}

inline EffectRequiredMaterialRuntimeDesc Resolve_EmitterTimeMaterialParameterModulation(
    const EffectRequiredMaterialRuntimeDesc& baseMaterial,
    float emitterPhase,
    uint32 effectPlaybackSeed)
{
    EffectRequiredMaterialRuntimeDesc material = baseMaterial;
    const auto& coreColorRgbModulation = baseMaterial.coreColorRgbModulation;
    const uint32 coreColorRgbCount = min(EffectMaterialCoreColorRgbModulationRuntimeDesc::kMaxModulators, coreColorRgbModulation.count);

    for (uint32 index = 0; index < coreColorRgbCount; ++index)
    {
        const EffectMaterialCoreColorRgbModulatorRuntimeDesc& modulator = coreColorRgbModulation.modulators[index];
        if (!modulator.enabled || modulator.timeSource != EffectMaterialScalarModulationTimeSource::EmitterTime)
            continue;

        const Vec3 fallbackValue{
            material.coreEmissive.coreColor.x,
            material.coreEmissive.coreColor.y,
            material.coreEmissive.coreColor.z
        };
        const Vec3 coreColorRgb = Evaluate_CoreColorRgbModulationCurve(modulator.curve, emitterPhase, fallbackValue, effectPlaybackSeed);
        material.coreEmissive.coreColor.x = coreColorRgb.x;
        material.coreEmissive.coreColor.y = coreColorRgb.y;
        material.coreEmissive.coreColor.z = coreColorRgb.z;
    }

    const auto& vec2Modulation = baseMaterial.vec2Modulation;
    const uint32 vec2Count = min(EffectMaterialVec2ModulationRuntimeDesc::kMaxModulators, vec2Modulation.count);

    for (uint32 index = 0; index < vec2Count; ++index)
    {
        const EffectMaterialVec2ModulatorRuntimeDesc& modulator = vec2Modulation.modulators[index];
        if (!modulator.enabled || modulator.timeSource != EffectMaterialScalarModulationTimeSource::EmitterTime)
            continue;

        const Vec2 offset = Evaluate_MaterialVec2ModulationCurve(modulator.curve, emitterPhase, Vec2{ 0.f, 0.f });
        Apply_MaterialVec2ModulationAdd(material, modulator.targetField, offset);
    }

    const auto& modulation = baseMaterial.scalarModulation;
    const uint32 count = min(EffectMaterialScalarModulationRuntimeDesc::kMaxModulators, modulation.count);

    for (uint32 index = 0; index < count; ++index)
    {
        const EffectMaterialScalarModulatorRuntimeDesc& modulator = modulation.modulators[index];
        if (!modulator.enabled || modulator.timeSource != EffectMaterialScalarModulationTimeSource::EmitterTime)
            continue;

        const float multiplier = Evaluate_MaterialScalarModulationCurve(modulator.curve, emitterPhase, 1.f);
        Apply_MaterialScalarModulationMultiplier(material, modulator.targetField, multiplier);
    }

    return material;
}

inline EffectRequiredMaterialRuntimeDesc Resolve_EmitterTimeMaterialUniformParameterModulation(
    const EffectRequiredMaterialRuntimeDesc& baseMaterial,
    float emitterPhase,
    uint32 effectPlaybackSeed)
{
    EffectRequiredMaterialRuntimeDesc material = baseMaterial;
    const auto& coreColorRgbModulation = baseMaterial.coreColorRgbModulation;
    const uint32 coreColorRgbCount = min(EffectMaterialCoreColorRgbModulationRuntimeDesc::kMaxModulators, coreColorRgbModulation.count);

    for (uint32 index = 0; index < coreColorRgbCount; ++index)
    {
        const EffectMaterialCoreColorRgbModulatorRuntimeDesc& modulator = coreColorRgbModulation.modulators[index];
        if (!modulator.enabled || modulator.timeSource != EffectMaterialScalarModulationTimeSource::EmitterTime)
            continue;

        const Vec3 fallbackValue{
            material.coreEmissive.coreColor.x,
            material.coreEmissive.coreColor.y,
            material.coreEmissive.coreColor.z
        };
        const Vec3 coreColorRgb = Evaluate_CoreColorRgbModulationCurve(modulator.curve, emitterPhase, fallbackValue, effectPlaybackSeed);
        material.coreEmissive.coreColor.x = coreColorRgb.x;
        material.coreEmissive.coreColor.y = coreColorRgb.y;
        material.coreEmissive.coreColor.z = coreColorRgb.z;
    }

    const auto& vec2Modulation = baseMaterial.vec2Modulation;
    const uint32 vec2Count = min(EffectMaterialVec2ModulationRuntimeDesc::kMaxModulators, vec2Modulation.count);

    for (uint32 index = 0; index < vec2Count; ++index)
    {
        const EffectMaterialVec2ModulatorRuntimeDesc& modulator = vec2Modulation.modulators[index];
        if (!modulator.enabled || modulator.timeSource != EffectMaterialScalarModulationTimeSource::EmitterTime)
            continue;

        const Vec2 offset = Evaluate_MaterialVec2ModulationCurve(modulator.curve, emitterPhase, Vec2{ 0.f, 0.f });
        Apply_MaterialVec2ModulationAdd(material, modulator.targetField, offset);
    }

    const auto& modulation = baseMaterial.scalarModulation;
    const uint32 count = min(EffectMaterialScalarModulationRuntimeDesc::kMaxModulators, modulation.count);

    for (uint32 index = 0; index < count; ++index)
    {
        const EffectMaterialScalarModulatorRuntimeDesc& modulator = modulation.modulators[index];
        if (!modulator.enabled || modulator.timeSource != EffectMaterialScalarModulationTimeSource::EmitterTime)
            continue;

        if (modulator.targetField != EffectMaterialScalarModulationTarget::RefractionIntensity &&
            modulator.targetField != EffectMaterialScalarModulationTarget::MainUVScrollSpeedScale &&
            modulator.targetField != EffectMaterialScalarModulationTarget::NoiseUVScrollSpeedScale &&
            modulator.targetField != EffectMaterialScalarModulationTarget::MaskUVScrollSpeedScale &&
            modulator.targetField != EffectMaterialScalarModulationTarget::FlowUVScrollSpeedScale)
            continue;

        const float multiplier = Evaluate_MaterialScalarModulationCurve(modulator.curve, emitterPhase, 1.f);
        Apply_MaterialScalarModulationMultiplier(material, modulator.targetField, multiplier);
    }

    return material;
}

//## Helper::ShaderPayload
inline EffectMaterialScalarModulationShaderPayload Build_MaterialScalarModulationShaderPayload(
    const EffectMaterialScalarModulationRuntimeDesc& modulation,
    float emitterPhase)
{
    EffectMaterialScalarModulationShaderPayload payload{};
    const uint32 count = min(EffectMaterialScalarModulationShaderPayload::maxModulators, modulation.count);
    payload.params = Vec4(static_cast<float>(count), emitterPhase, 0.f, 0.f);

    for (uint32 index = 0; index < count; ++index)
    {
        const EffectMaterialScalarModulatorRuntimeDesc& modulator = modulation.modulators[index];
        const EffectMaterialScalarModulationCurveDesc& curve = modulator.curve;
        const uint32 keyCount = curve.enabled ? min(kEffectDistributionCurveMaxKeys, curve.keyCount) : 0u;

        payload.meta[index] = Vec4(
            modulator.enabled ? 1.f : 0.f,
            static_cast<float>(static_cast<uint32>(modulator.targetField)),
            static_cast<float>(static_cast<uint32>(modulator.timeSource)),
            static_cast<float>(keyCount)
        );
        payload.keyTimes[index] = curve.keyTimes;
        payload.keyTimesBlock1[index] = curve.keyTimesBlock1;
        payload.keyValues[index] = curve.keyValues;
        payload.keyValuesBlock1[index] = curve.keyValuesBlock1;
        payload.keyArriveTangents[index] = curve.keyArriveTangents;
        payload.keyArriveTangentsBlock1[index] = curve.keyArriveTangentsBlock1;
        payload.keyLeaveTangents[index] = curve.keyLeaveTangents;
        payload.keyLeaveTangentsBlock1[index] = curve.keyLeaveTangentsBlock1;
        payload.keyModes[index] = curve.keyModes;
        payload.keyModesBlock1[index] = curve.keyModesBlock1;
    }

    return payload;
}

NS_END
