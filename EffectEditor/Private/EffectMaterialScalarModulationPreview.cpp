#include "EffectMaterialScalarModulationPreview.h"

#include <cmath>

NS_BEGIN(EffectEditor)

namespace
{
    float Get_Component(const Vec4& value, uint32 index)
    {
        switch (index)
        {
        case 0:
            return value.x;
        case 1:
            return value.y;
        case 2:
            return value.z;
        case 3:
            return value.w;
        default:
            return 0.f;
        }
    }

    void Set_Component(Vec4& value, uint32 index, float component)
    {
        switch (index)
        {
        case 0:
            value.x = component;
            break;
        case 1:
            value.y = component;
            break;
        case 2:
            value.z = component;
            break;
        case 3:
            value.w = component;
            break;
        default:
            break;
        }
    }

    float Get_Component(const Vec4& block0, const Vec4& block1, uint32 index)
    {
        if (index < 4u)
            return Get_Component(block0, index);

        return Get_Component(block1, index - 4u);
    }

    void Set_Component(Vec4& block0, Vec4& block1, uint32 index, float component)
    {
        if (index < 4u)
        {
            Set_Component(block0, index, component);
            return;
        }

        Set_Component(block1, index - 4u, component);
    }

    EffectMaterialScalarModulationTarget To_RuntimeTarget(MaterialScalarModulationTargetField target)
    {
        switch (target)
        {
        case MaterialScalarModulationTargetField::OpacityPower:
            return EffectMaterialScalarModulationTarget::OpacityPower;
        case MaterialScalarModulationTargetField::NoiseStrength:
            return EffectMaterialScalarModulationTarget::NoiseStrength;
        case MaterialScalarModulationTargetField::AlphaErosion:
            return EffectMaterialScalarModulationTarget::AlphaErosion;
        case MaterialScalarModulationTargetField::AlphaCutoff:
            return EffectMaterialScalarModulationTarget::AlphaCutoff;
        case MaterialScalarModulationTargetField::CoreIntensity:
            return EffectMaterialScalarModulationTarget::CoreIntensity;
        case MaterialScalarModulationTargetField::OuterIntensity:
            return EffectMaterialScalarModulationTarget::OuterIntensity;
        case MaterialScalarModulationTargetField::CoreColor:
            return EffectMaterialScalarModulationTarget::CoreColor;
        case MaterialScalarModulationTargetField::Intensity:
        default:
            return EffectMaterialScalarModulationTarget::Intensity;
        }
    }

    EffectMaterialScalarModulationTimeSource To_RuntimeTimeSource(MaterialScalarModulationTimeSource timeSource)
    {
        switch (timeSource)
        {
        case MaterialScalarModulationTimeSource::EmitterTime:
            return EffectMaterialScalarModulationTimeSource::EmitterTime;
        case MaterialScalarModulationTimeSource::ParticleLife:
        default:
            return EffectMaterialScalarModulationTimeSource::ParticleLife;
        }
    }

    float To_RuntimeCurveMode(FloatCurveInterpolationMode mode)
    {
        switch (mode)
        {
        case FloatCurveInterpolationMode::Constant:
            return 0.f;
        case FloatCurveInterpolationMode::CurveAutoClamped:
            return 2.f;
        case FloatCurveInterpolationMode::Linear:
        default:
            return 1.f;
        }
    }

    void Fill_CurveComponent(
        EffectMaterialScalarModulationCurveDesc& desc,
        uint32 index,
        const FloatCurveKeyData& key)
    {
        Set_Component(desc.keyTimes, desc.keyTimesBlock1, index, clamp(key.time, 0.f, 1.f));
        Set_Component(desc.keyValues, desc.keyValuesBlock1, index, key.value);
        Set_Component(desc.keyArriveTangents, desc.keyArriveTangentsBlock1, index, key.arriveTangent);
        Set_Component(desc.keyLeaveTangents, desc.keyLeaveTangentsBlock1, index, key.leaveTangent);
        Set_Component(desc.keyModes, desc.keyModesBlock1, index, To_RuntimeCurveMode(key.interpolationMode));
    }

    void Fill_CoreColorRgbCurveComponent(
        EffectMaterialCoreColorRgbModulationCurveDesc& desc,
        uint32 index,
        const Vector3CurveKeyData& key)
    {
        Set_Component(desc.keyTimes, desc.keyTimesBlock1, index, clamp(key.time, 0.f, 1.f));
        Set_Component(desc.keyValuesR, desc.keyValuesRBlock1, index, key.value.x);
        Set_Component(desc.keyValuesG, desc.keyValuesGBlock1, index, key.value.y);
        Set_Component(desc.keyValuesB, desc.keyValuesBBlock1, index, key.value.z);
        Set_Component(desc.keyArriveTangentsR, desc.keyArriveTangentsRBlock1, index, key.arriveTangent.x);
        Set_Component(desc.keyArriveTangentsG, desc.keyArriveTangentsGBlock1, index, key.arriveTangent.y);
        Set_Component(desc.keyArriveTangentsB, desc.keyArriveTangentsBBlock1, index, key.arriveTangent.z);
        Set_Component(desc.keyLeaveTangentsR, desc.keyLeaveTangentsRBlock1, index, key.leaveTangent.x);
        Set_Component(desc.keyLeaveTangentsG, desc.keyLeaveTangentsGBlock1, index, key.leaveTangent.y);
        Set_Component(desc.keyLeaveTangentsB, desc.keyLeaveTangentsBBlock1, index, key.leaveTangent.z);
        Set_Component(desc.keyModes, desc.keyModesBlock1, index, To_RuntimeCurveMode(key.interpolationMode));
    }

    EffectMaterialScalarModulationCurveDesc Build_CurveDesc(const FloatDistributionData& distribution)
    {
        constexpr uint32 kMaxCurveKeys{ ::Engine::kEffectDistributionCurveMaxKeys };
        EffectMaterialScalarModulationCurveDesc desc{};

        if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&distribution.payload);
            distribution.mode == DistributionMode::ConstantCurve && curve != nullptr && !curve->keys.empty())
        {
            const uint32 keyCount = min(kMaxCurveKeys, static_cast<uint32>(curve->keys.size()));
            desc.enabled = true;
            desc.keyCount = keyCount;
            desc.keyTimes = Vec4{};
            desc.keyTimesBlock1 = Vec4{};
            desc.keyValues = Vec4{};
            desc.keyValuesBlock1 = Vec4{};
            desc.keyArriveTangents = Vec4{};
            desc.keyArriveTangentsBlock1 = Vec4{};
            desc.keyLeaveTangents = Vec4{};
            desc.keyLeaveTangentsBlock1 = Vec4{};
            desc.keyModes = Vec4{};
            desc.keyModesBlock1 = Vec4{};

            for (uint32 index = 0; index < keyCount; ++index)
                Fill_CurveComponent(desc, index, curve->keys[index]);

            return desc;
        }

        float value = 1.f;
        if (const auto* constant = get_if<ConstantFloatDistributionData>(&distribution.payload))
            value = constant->value;
        else if (const auto* uniform = get_if<UniformFloatDistributionData>(&distribution.payload))
            value = (uniform->minValue + uniform->maxValue) * 0.5f;

        desc.enabled = true;
        desc.keyCount = 2;
        desc.keyValues = Vec4{ value, value, 0.f, 0.f };
        return desc;
    }

    EffectMaterialCoreColorRgbModulationCurveDesc Build_CoreColorRgbCurveDesc(const Vector3DistributionData& distribution)
    {
        constexpr uint32 kMaxCurveKeys{ ::Engine::kEffectDistributionCurveMaxKeys };
        EffectMaterialCoreColorRgbModulationCurveDesc desc{};

        if (const auto* curve = get_if<ConstantCurveVector3DistributionData>(&distribution.payload);
            distribution.mode == DistributionMode::ConstantCurve && curve != nullptr && !curve->keys.empty())
        {
            const uint32 keyCount = min(kMaxCurveKeys, static_cast<uint32>(curve->keys.size()));
            desc.enabled = true;
            desc.keyCount = keyCount;
            desc.keyTimes = Vec4{};
            desc.keyTimesBlock1 = Vec4{};
            desc.keyValuesR = Vec4{};
            desc.keyValuesRBlock1 = Vec4{};
            desc.keyValuesG = Vec4{};
            desc.keyValuesGBlock1 = Vec4{};
            desc.keyValuesB = Vec4{};
            desc.keyValuesBBlock1 = Vec4{};
            desc.keyArriveTangentsR = Vec4{};
            desc.keyArriveTangentsRBlock1 = Vec4{};
            desc.keyArriveTangentsG = Vec4{};
            desc.keyArriveTangentsGBlock1 = Vec4{};
            desc.keyArriveTangentsB = Vec4{};
            desc.keyArriveTangentsBBlock1 = Vec4{};
            desc.keyLeaveTangentsR = Vec4{};
            desc.keyLeaveTangentsRBlock1 = Vec4{};
            desc.keyLeaveTangentsG = Vec4{};
            desc.keyLeaveTangentsGBlock1 = Vec4{};
            desc.keyLeaveTangentsB = Vec4{};
            desc.keyLeaveTangentsBBlock1 = Vec4{};
            desc.keyModes = Vec4{};
            desc.keyModesBlock1 = Vec4{};

            for (uint32 index = 0; index < keyCount; ++index)
                Fill_CoreColorRgbCurveComponent(desc, index, curve->keys[index]);

            return desc;
        }

        Vec3 value{ 1.f, 0.85f, 0.45f };
        if (const auto* constant = get_if<ConstantVector3DistributionData>(&distribution.payload))
            value = constant->value;
        else if (const auto* uniform = get_if<UniformVector3DistributionData>(&distribution.payload))
            value = (uniform->minValue + uniform->maxValue) * 0.5f;

        desc.enabled = true;
        desc.keyCount = 2;
        desc.keyValuesR = Vec4{ value.x, value.x, 0.f, 0.f };
        desc.keyValuesG = Vec4{ value.y, value.y, 0.f, 0.f };
        desc.keyValuesB = Vec4{ value.z, value.z, 0.f, 0.f };
        return desc;
    }

    float Evaluate_CurveLinear(float leftTime, float leftValue, float rightTime, float rightValue, float x)
    {
        const float width = max(0.0001f, rightTime - leftTime);
        const float t = clamp((x - leftTime) / width, 0.f, 1.f);
        return lerp(leftValue, rightValue, t);
    }

    float Evaluate_CurveAutoClamped(
        float leftTime,
        float leftValue,
        float leftLeaveTangent,
        float rightTime,
        float rightValue,
        float rightArriveTangent,
        float x)
    {
        const float width = max(0.0001f, rightTime - leftTime);
        const float t = clamp((x - leftTime) / width, 0.f, 1.f);
        const float m0 = leftLeaveTangent * width;
        const float m1 = rightArriveTangent * width;
        const float t2 = t * t;
        const float t3 = t2 * t;
        return (2.f * t3 - 3.f * t2 + 1.f) * leftValue +
               (t3 - 2.f * t2 + t) * m0 +
               (-2.f * t3 + 3.f * t2) * rightValue +
               (t3 - t2) * m1;
    }

    float Evaluate_Curve(const EffectMaterialScalarModulationCurveDesc& curve, float x, float fallbackValue)
    {
        if (!curve.enabled || curve.keyCount == 0)
            return fallbackValue;

        const uint32 keyCount = min(::Engine::kEffectDistributionCurveMaxKeys, curve.keyCount);
        if (keyCount == 1)
            return Get_Component(curve.keyValues, curve.keyValuesBlock1, 0);

        const float clampedX = clamp(x, 0.f, 1.f);
        for (uint32 index = 1; index < keyCount; ++index)
        {
            const float rightTime = Get_Component(curve.keyTimes, curve.keyTimesBlock1, index);
            if (clampedX > rightTime)
                continue;

            const uint32 leftIndex = index - 1;
            const float leftTime = Get_Component(curve.keyTimes, curve.keyTimesBlock1, leftIndex);
            const float leftValue = Get_Component(curve.keyValues, curve.keyValuesBlock1, leftIndex);
            const float rightValue = Get_Component(curve.keyValues, curve.keyValuesBlock1, index);
            const int32 mode = static_cast<int32>(Get_Component(curve.keyModes, curve.keyModesBlock1, leftIndex));

            if (mode == 0)
                return leftValue;

            if (mode == 2)
            {
                return Evaluate_CurveAutoClamped(
                    leftTime,
                    leftValue,
                    Get_Component(curve.keyLeaveTangents, curve.keyLeaveTangentsBlock1, leftIndex),
                    rightTime,
                    rightValue,
                    Get_Component(curve.keyArriveTangents, curve.keyArriveTangentsBlock1, index),
                    clampedX
                );
            }

            return Evaluate_CurveLinear(leftTime, leftValue, rightTime, rightValue, clampedX);
        }

        return Get_Component(curve.keyValues, curve.keyValuesBlock1, keyCount - 1);
    }

    float Evaluate_CurveComponents(
        const Vec4& keyTimes,
        const Vec4& keyTimesBlock1,
        const Vec4& keyValues,
        const Vec4& keyValuesBlock1,
        const Vec4& keyArriveTangents,
        const Vec4& keyArriveTangentsBlock1,
        const Vec4& keyLeaveTangents,
        const Vec4& keyLeaveTangentsBlock1,
        const Vec4& keyModes,
        const Vec4& keyModesBlock1,
        uint32 keyCount,
        bool enabled,
        float x,
        float fallbackValue)
    {
        if (!enabled || keyCount == 0)
            return fallbackValue;

        const uint32 clampedKeyCount = min(::Engine::kEffectDistributionCurveMaxKeys, keyCount);
        if (clampedKeyCount == 1)
            return Get_Component(keyValues, keyValuesBlock1, 0);

        const float clampedX = clamp(x, 0.f, 1.f);
        for (uint32 index = 1; index < clampedKeyCount; ++index)
        {
            const float rightTime = Get_Component(keyTimes, keyTimesBlock1, index);
            if (clampedX > rightTime)
                continue;

            const uint32 leftIndex = index - 1;
            const float leftTime = Get_Component(keyTimes, keyTimesBlock1, leftIndex);
            const float leftValue = Get_Component(keyValues, keyValuesBlock1, leftIndex);
            const float rightValue = Get_Component(keyValues, keyValuesBlock1, index);
            const int32 mode = static_cast<int32>(Get_Component(keyModes, keyModesBlock1, leftIndex));

            if (mode == 0)
                return leftValue;

            if (mode == 2)
            {
                return Evaluate_CurveAutoClamped(
                    leftTime,
                    leftValue,
                    Get_Component(keyLeaveTangents, keyLeaveTangentsBlock1, leftIndex),
                    rightTime,
                    rightValue,
                    Get_Component(keyArriveTangents, keyArriveTangentsBlock1, index),
                    clampedX
                );
            }

            return Evaluate_CurveLinear(leftTime, leftValue, rightTime, rightValue, clampedX);
        }

        return Get_Component(keyValues, keyValuesBlock1, clampedKeyCount - 1);
    }

    Vec3 Evaluate_CoreColorRgbCurve(const EffectMaterialCoreColorRgbModulationCurveDesc& curve, float x, const Vec3& fallbackValue)
    {
        return Vec3{
            Evaluate_CurveComponents(
                curve.keyTimes,
                curve.keyTimesBlock1,
                curve.keyValuesR,
                curve.keyValuesRBlock1,
                curve.keyArriveTangentsR,
                curve.keyArriveTangentsRBlock1,
                curve.keyLeaveTangentsR,
                curve.keyLeaveTangentsRBlock1,
                curve.keyModes,
                curve.keyModesBlock1,
                curve.keyCount,
                curve.enabled,
                x,
                fallbackValue.x
            ),
            Evaluate_CurveComponents(
                curve.keyTimes,
                curve.keyTimesBlock1,
                curve.keyValuesG,
                curve.keyValuesGBlock1,
                curve.keyArriveTangentsG,
                curve.keyArriveTangentsGBlock1,
                curve.keyLeaveTangentsG,
                curve.keyLeaveTangentsGBlock1,
                curve.keyModes,
                curve.keyModesBlock1,
                curve.keyCount,
                curve.enabled,
                x,
                fallbackValue.y
            ),
            Evaluate_CurveComponents(
                curve.keyTimes,
                curve.keyTimesBlock1,
                curve.keyValuesB,
                curve.keyValuesBBlock1,
                curve.keyArriveTangentsB,
                curve.keyArriveTangentsBBlock1,
                curve.keyLeaveTangentsB,
                curve.keyLeaveTangentsBBlock1,
                curve.keyModes,
                curve.keyModesBlock1,
                curve.keyCount,
                curve.enabled,
                x,
                fallbackValue.z
            )
        };
    }

    void Apply_Multiplier(
        EffectMaterialInstanceData& material,
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
        case EffectMaterialScalarModulationTarget::Intensity:
        default:
            material.intensity *= multiplier;
            break;
        }
    }
}

const RequiredModuleData* Find_RequiredModuleData(const AuthoringEmitter& emitter)
{
    for (const AuthoringModule& module : emitter.modules)
    {
        if (module.type != AuthoringModuleType::Required)
            continue;

        return get_if<RequiredModuleData>(&module.data);
    }

    return nullptr;
}

const MaterialScalarModulationModuleData* Find_MaterialScalarModulationModuleData(const AuthoringEmitter& emitter)
{
    for (const AuthoringModule& module : emitter.modules)
    {
        if (module.type != AuthoringModuleType::MaterialScalarModulation || !module.enabled)
            continue;

        return get_if<MaterialScalarModulationModuleData>(&module.data);
    }

    return nullptr;
}

EffectMaterialScalarModulationRuntimeDesc Build_MaterialScalarModulationRuntimeDesc(const MaterialScalarModulationModuleData& data)
{
    EffectMaterialScalarModulationRuntimeDesc desc{};
    const uint32 count = min(
        EffectMaterialScalarModulationRuntimeDesc::kMaxModulators,
        static_cast<uint32>(data.modulators.size())
    );
    desc.count = count;

    for (uint32 index = 0; index < count; ++index)
    {
        const MaterialScalarModulatorData& source = data.modulators[index];
        EffectMaterialScalarModulatorRuntimeDesc& target = desc.modulators[index];
        target.enabled = source.enabled;
        target.targetField = To_RuntimeTarget(source.targetField);
        target.operation = EffectMaterialScalarModulationOperation::Multiply;
        target.timeSource = To_RuntimeTimeSource(source.timeSource);
        target.curve = Build_CurveDesc(source.distribution);
    }

    return desc;
}

EffectMaterialCoreColorRgbModulationRuntimeDesc Build_CoreColorRgbModulationRuntimeDesc(const MaterialScalarModulationModuleData& data)
{
    EffectMaterialCoreColorRgbModulationRuntimeDesc desc{};
    const uint32 count = min(
        EffectMaterialCoreColorRgbModulationRuntimeDesc::kMaxModulators,
        static_cast<uint32>(data.coreColorRgbModulators.size())
    );
    desc.count = count;

    for (uint32 index = 0; index < count; ++index)
    {
        const MaterialCoreColorRgbModulatorData& source = data.coreColorRgbModulators[index];
        EffectMaterialCoreColorRgbModulatorRuntimeDesc& target = desc.modulators[index];
        target.enabled = source.enabled;
        target.timeSource = To_RuntimeTimeSource(source.timeSource);
        target.curve = Build_CoreColorRgbCurveDesc(source.distribution);
    }

    return desc;
}

float Compute_MaterialScalarPreviewPhase(float elapsedTime, float duration)
{
    const float safeDuration = max(duration, 0.0001f);
    const float normalizedTime = max(0.f, elapsedTime) / safeDuration;
    return normalizedTime - std::floor(normalizedTime);
}

MaterialScalarModulationPreviewResult Evaluate_MaterialScalarModulationPreview(
    const EffectMaterialInstanceData& baseMaterial,
    const EffectMaterialCoreColorRgbModulationRuntimeDesc& coreColorRgbModulation,
    const EffectMaterialScalarModulationRuntimeDesc& modulation,
    float previewPhase,
    bool evaluateParticleLife)
{
    MaterialScalarModulationPreviewResult result{};
    result.material = baseMaterial;

    const uint32 coreColorRgbCount = min(EffectMaterialCoreColorRgbModulationRuntimeDesc::kMaxModulators, coreColorRgbModulation.count);
    for (uint32 index = 0; index < coreColorRgbCount; ++index)
    {
        const EffectMaterialCoreColorRgbModulatorRuntimeDesc& modulator = coreColorRgbModulation.modulators[index];
        if (!modulator.enabled)
            continue;

        if (modulator.timeSource == EffectMaterialScalarModulationTimeSource::ParticleLife)
        {
            // TODO : Core Color RGB의 ParticleLife는 per-particle RGB 변조 payload가 생긴 뒤 개방한다.
            result.skippedParticleLife = true;
            continue;
        }

        const Vec3 fallbackValue{
            result.material.coreEmissive.coreColor.x,
            result.material.coreEmissive.coreColor.y,
            result.material.coreEmissive.coreColor.z
        };
        const Vec3 coreColorRgb = Evaluate_CoreColorRgbCurve(modulator.curve, previewPhase, fallbackValue);
        result.material.coreEmissive.coreColor.x = coreColorRgb.x;
        result.material.coreEmissive.coreColor.y = coreColorRgb.y;
        result.material.coreEmissive.coreColor.z = coreColorRgb.z;
        result.applied = true;
    }

    const uint32 count = min(EffectMaterialScalarModulationRuntimeDesc::kMaxModulators, modulation.count);
    for (uint32 index = 0; index < count; ++index)
    {
        const EffectMaterialScalarModulatorRuntimeDesc& modulator = modulation.modulators[index];
        if (!modulator.enabled)
            continue;

        if (modulator.timeSource == EffectMaterialScalarModulationTimeSource::ParticleLife && !evaluateParticleLife)
        {
            result.skippedParticleLife = true;
            continue;
        }

        const float multiplier = Evaluate_Curve(modulator.curve, previewPhase, 1.f);
        Apply_Multiplier(result.material, modulator.targetField, multiplier);
        result.applied = true;
    }

    return result;
}

NS_END
