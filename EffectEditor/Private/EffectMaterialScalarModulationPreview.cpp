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
        Set_Component(desc.keyTimes, index, clamp(key.time, 0.f, 1.f));
        Set_Component(desc.keyValues, index, key.value);
        Set_Component(desc.keyArriveTangents, index, key.arriveTangent);
        Set_Component(desc.keyLeaveTangents, index, key.leaveTangent);
        Set_Component(desc.keyModes, index, To_RuntimeCurveMode(key.interpolationMode));
    }

    void Fill_CoreColorRgbCurveComponent(
        EffectMaterialCoreColorRgbModulationCurveDesc& desc,
        uint32 index,
        const Vector3CurveKeyData& key)
    {
        Set_Component(desc.keyTimes, index, clamp(key.time, 0.f, 1.f));
        Set_Component(desc.keyValuesR, index, key.value.x);
        Set_Component(desc.keyValuesG, index, key.value.y);
        Set_Component(desc.keyValuesB, index, key.value.z);
        Set_Component(desc.keyArriveTangentsR, index, key.arriveTangent.x);
        Set_Component(desc.keyArriveTangentsG, index, key.arriveTangent.y);
        Set_Component(desc.keyArriveTangentsB, index, key.arriveTangent.z);
        Set_Component(desc.keyLeaveTangentsR, index, key.leaveTangent.x);
        Set_Component(desc.keyLeaveTangentsG, index, key.leaveTangent.y);
        Set_Component(desc.keyLeaveTangentsB, index, key.leaveTangent.z);
        Set_Component(desc.keyModes, index, To_RuntimeCurveMode(key.interpolationMode));
    }

    EffectMaterialScalarModulationCurveDesc Build_CurveDesc(const FloatDistributionData& distribution)
    {
        constexpr uint32 kMaxCurveKeys{ 4 };
        EffectMaterialScalarModulationCurveDesc desc{};

        if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&distribution.payload);
            distribution.mode == DistributionMode::ConstantCurve && curve != nullptr && !curve->keys.empty())
        {
            const uint32 keyCount = min(kMaxCurveKeys, static_cast<uint32>(curve->keys.size()));
            desc.enabled = true;
            desc.keyCount = keyCount;
            desc.keyTimes = Vec4{};
            desc.keyValues = Vec4{};
            desc.keyArriveTangents = Vec4{};
            desc.keyLeaveTangents = Vec4{};
            desc.keyModes = Vec4{};

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
        constexpr uint32 kMaxCurveKeys{ 4 };
        EffectMaterialCoreColorRgbModulationCurveDesc desc{};

        if (const auto* curve = get_if<ConstantCurveVector3DistributionData>(&distribution.payload);
            distribution.mode == DistributionMode::ConstantCurve && curve != nullptr && !curve->keys.empty())
        {
            const uint32 keyCount = min(kMaxCurveKeys, static_cast<uint32>(curve->keys.size()));
            desc.enabled = true;
            desc.keyCount = keyCount;
            desc.keyTimes = Vec4{};
            desc.keyValuesR = Vec4{};
            desc.keyValuesG = Vec4{};
            desc.keyValuesB = Vec4{};
            desc.keyArriveTangentsR = Vec4{};
            desc.keyArriveTangentsG = Vec4{};
            desc.keyArriveTangentsB = Vec4{};
            desc.keyLeaveTangentsR = Vec4{};
            desc.keyLeaveTangentsG = Vec4{};
            desc.keyLeaveTangentsB = Vec4{};
            desc.keyModes = Vec4{};

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

        const uint32 keyCount = min(4u, curve.keyCount);
        if (keyCount == 1)
            return Get_Component(curve.keyValues, 0);

        const float clampedX = clamp(x, 0.f, 1.f);
        for (uint32 index = 1; index < keyCount; ++index)
        {
            const float rightTime = Get_Component(curve.keyTimes, index);
            if (clampedX > rightTime)
                continue;

            const uint32 leftIndex = index - 1;
            const float leftTime = Get_Component(curve.keyTimes, leftIndex);
            const float leftValue = Get_Component(curve.keyValues, leftIndex);
            const float rightValue = Get_Component(curve.keyValues, index);
            const int32 mode = static_cast<int32>(Get_Component(curve.keyModes, leftIndex));

            if (mode == 0)
                return leftValue;

            if (mode == 2)
            {
                return Evaluate_CurveAutoClamped(
                    leftTime,
                    leftValue,
                    Get_Component(curve.keyLeaveTangents, leftIndex),
                    rightTime,
                    rightValue,
                    Get_Component(curve.keyArriveTangents, index),
                    clampedX
                );
            }

            return Evaluate_CurveLinear(leftTime, leftValue, rightTime, rightValue, clampedX);
        }

        return Get_Component(curve.keyValues, keyCount - 1);
    }

    float Evaluate_CurveComponents(
        const Vec4& keyTimes,
        const Vec4& keyValues,
        const Vec4& keyArriveTangents,
        const Vec4& keyLeaveTangents,
        const Vec4& keyModes,
        uint32 keyCount,
        bool enabled,
        float x,
        float fallbackValue)
    {
        if (!enabled || keyCount == 0)
            return fallbackValue;

        const uint32 clampedKeyCount = min(4u, keyCount);
        if (clampedKeyCount == 1)
            return Get_Component(keyValues, 0);

        const float clampedX = clamp(x, 0.f, 1.f);
        for (uint32 index = 1; index < clampedKeyCount; ++index)
        {
            const float rightTime = Get_Component(keyTimes, index);
            if (clampedX > rightTime)
                continue;

            const uint32 leftIndex = index - 1;
            const float leftTime = Get_Component(keyTimes, leftIndex);
            const float leftValue = Get_Component(keyValues, leftIndex);
            const float rightValue = Get_Component(keyValues, index);
            const int32 mode = static_cast<int32>(Get_Component(keyModes, leftIndex));

            if (mode == 0)
                return leftValue;

            if (mode == 2)
            {
                return Evaluate_CurveAutoClamped(
                    leftTime,
                    leftValue,
                    Get_Component(keyLeaveTangents, leftIndex),
                    rightTime,
                    rightValue,
                    Get_Component(keyArriveTangents, index),
                    clampedX
                );
            }

            return Evaluate_CurveLinear(leftTime, leftValue, rightTime, rightValue, clampedX);
        }

        return Get_Component(keyValues, clampedKeyCount - 1);
    }

    Vec3 Evaluate_CoreColorRgbCurve(const EffectMaterialCoreColorRgbModulationCurveDesc& curve, float x, const Vec3& fallbackValue)
    {
        return Vec3{
            Evaluate_CurveComponents(curve.keyTimes, curve.keyValuesR, curve.keyArriveTangentsR, curve.keyLeaveTangentsR, curve.keyModes, curve.keyCount, curve.enabled, x, fallbackValue.x),
            Evaluate_CurveComponents(curve.keyTimes, curve.keyValuesG, curve.keyArriveTangentsG, curve.keyLeaveTangentsG, curve.keyModes, curve.keyCount, curve.enabled, x, fallbackValue.y),
            Evaluate_CurveComponents(curve.keyTimes, curve.keyValuesB, curve.keyArriveTangentsB, curve.keyLeaveTangentsB, curve.keyModes, curve.keyCount, curve.enabled, x, fallbackValue.z)
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
