#include "Level_EffectEditor.h"

#include "Camera.h"
#include "ClientInstance.h"
#include "EffectEditorCamera.h"
#include "EffectEditorInstance.h"
#include "EffectEditorSceneGrid.h"
#include "EffectEditorSkybox.h"
#include "EffectMaterialPresetReader.h"
#include "Emitter_View.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "TrailPreviewController.h"
#include "TrailPreviewPlayerObject.h"
#include "TrailPreviewWeaponObject.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    constexpr const wchar_t* kPreviewPoolKey = L"EffectEditorPreview";

    //## Module Lookup

    template <typename TData>
    const TData* Find_ModuleData(const AuthoringEmitter& emitter, AuthoringModuleType type)
    {
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.type != type)
                continue;

            if (!module.enabled)
                continue;

            return get_if<TData>(&module.data);
        }

        return nullptr;
    }

    //## Preview Policy

    Color Clamp_PreviewColor(Color value)
    {
        value.R(clamp(value.R(), 0.f, 1.f));
        value.G(clamp(value.G(), 0.f, 1.f));
        value.B(clamp(value.B(), 0.f, 1.f));
        value.A(clamp(value.A(), 0.f, 1.f));
        return value;
    }

    uint32 Resolve_SubUVFrameCount(const RequiredModuleData& required)
    {
        return max(1u, max(1u, required.material.subUVRows) * max(1u, required.material.subUVCols));
    }

    EffectEmitterRenderLayerOverride Resolve_EffectRenderLayerOverride(EmitterRenderLayerOverride value)
    {
        switch (value)
        {
        case EmitterRenderLayerOverride::UIEffect:
            return EffectEmitterRenderLayerOverride::UIEffect;
        case EmitterRenderLayerOverride::Auto:
        default:
            return EffectEmitterRenderLayerOverride::Auto;
        }
    }

    void Copy_MaterialToRuntimeDesc(const EffectMaterialInstanceData& material, EffectRequiredMaterialRuntimeDesc& outMaterial)
    {
        outMaterial.materialFamily = material.materialFamily;
        outMaterial.mainTextureGuid = material.mainTextureGuid;
        outMaterial.mainTexturePath = material.mainTexturePath;
        outMaterial.noiseTextureGuid = material.noiseTextureGuid;
        outMaterial.noiseTexturePath = material.noiseTexturePath;
        outMaterial.maskTextureGuid = material.maskTextureGuid;
        outMaterial.maskTexturePath = material.maskTexturePath;
        outMaterial.flowTextureGuid = material.flowTextureGuid;
        outMaterial.flowTexturePath = material.flowTexturePath;

        outMaterial.blendMode = material.blendMode;
        outMaterial.opacitySource = material.opacitySource.empty() ? "Alpha" : material.opacitySource;
        outMaterial.tint = material.tint;
        outMaterial.intensity = material.intensity;
        outMaterial.opacityPower = material.opacityPower;
        outMaterial.noiseStrength = material.noiseStrength;
        outMaterial.alphaCutoff = material.alphaCutoff;
        outMaterial.alphaErosion = material.alphaErosion;
        outMaterial.mainUVScale = material.mainUVScale;
        outMaterial.mainUVOffset = material.mainUVOffset;
        outMaterial.mainUVScrollSpeed = material.mainUVScrollSpeed;
        outMaterial.mainUVTilingMode = material.mainUVTilingMode;
        outMaterial.noiseUVScale = material.noiseUVScale;
        outMaterial.noiseUVOffset = material.noiseUVOffset;
        outMaterial.noiseUVScrollSpeed = material.noiseUVScrollSpeed;
        outMaterial.noiseUVTilingMode = material.noiseUVTilingMode;
        outMaterial.maskUVScale = material.maskUVScale;
        outMaterial.maskUVOffset = material.maskUVOffset;
        outMaterial.maskUVScrollSpeed = material.maskUVScrollSpeed;
        outMaterial.maskUVTilingMode = material.maskUVTilingMode;
        outMaterial.flowUVScale = material.flowUVScale;
        outMaterial.flowUVOffset = material.flowUVOffset;
        outMaterial.flowUVScrollSpeed = material.flowUVScrollSpeed;
        outMaterial.flowUVTilingMode = material.flowUVTilingMode;

        outMaterial.additive = material.additive;
        outMaterial.coreEmissive = material.coreEmissive;
        outMaterial.refractionIntensity = material.refractionIntensity;
        outMaterial.refractionPresence = material.refractionPresence;
        outMaterial.distortionShapeMode = material.distortionShapeMode;
        outMaterial.distortionShapeRadius = material.distortionShapeRadius;
        outMaterial.distortionShapeThickness = material.distortionShapeThickness;
        outMaterial.distortionShapeSoftness = material.distortionShapeSoftness;
        outMaterial.twoSided = material.twoSided;
        outMaterial.subUVRows = material.subUVRows;
        outMaterial.subUVCols = material.subUVCols;
    }

    fs::path Resolve_AuthoredAssetPath(const string& guid, const string& path)
    {
        if (GAME == nullptr)
            return fs::path(String::ToWString(path));

        if (!guid.empty())
        {
            const wstring resolvedPath = GAME->Resolve_AssetPath(guid);
            if (!resolvedPath.empty())
                return fs::path(resolvedPath).lexically_normal();
        }

        fs::path resolvedPath = fs::path(String::ToWString(path));
        if (resolvedPath.is_relative())
            resolvedPath = fs::path(GAME->Get_AssetRoot()) / resolvedPath;

        return resolvedPath.lexically_normal();
    }

    EffectScreenAlignment Resolve_EffectScreenAlignment(EmitterScreenAlignment alignment)
    {
        switch (alignment)
        {
        case EmitterScreenAlignment::Rectangle:
            return EffectScreenAlignment::Rectangle;

        case EmitterScreenAlignment::Square:
            return EffectScreenAlignment::Square;

        case EmitterScreenAlignment::AwayFromCenter:
            return EffectScreenAlignment::AwayFromCenter;

        case EmitterScreenAlignment::Velocity:
            return EffectScreenAlignment::Velocity;

        case EmitterScreenAlignment::WorldPlaneXY:
            return EffectScreenAlignment::WorldPlaneXY;

        case EmitterScreenAlignment::WorldPlaneXZ:
            return EffectScreenAlignment::WorldPlaneXZ;

        case EmitterScreenAlignment::FacingCameraPosition:
        case EmitterScreenAlignment::TypeSpecific:
        case EmitterScreenAlignment::FacingCameraDistanceBlend:
        default:
            return EffectScreenAlignment::FacingCameraPosition;
        }
    }

    EffectSpriteDirectionalAlignmentMode Resolve_EffectSpriteDirectionalAlignmentMode(EmitterDirectionalAlignmentMode mode)
    {
        switch (mode)
        {
        case EmitterDirectionalAlignmentMode::LookDirection:
            return EffectSpriteDirectionalAlignmentMode::LookDirection;

        case EmitterDirectionalAlignmentMode::TextureAxis:
        default:
            return EffectSpriteDirectionalAlignmentMode::TextureAxis;
        }
    }

    EffectSpriteTextureAxis Resolve_EffectSpriteTextureAxis(EmitterSpriteTextureAxis axis)
    {
        switch (axis)
        {
        case EmitterSpriteTextureAxis::X:
            return EffectSpriteTextureAxis::X;

        case EmitterSpriteTextureAxis::Y:
        default:
            return EffectSpriteTextureAxis::Y;
        }
    }

    PointParticleSphereLocationMode Resolve_PointParticleSphereLocationMode(SphereLocationSpawnMode mode)
    {
        switch (mode)
        {
        case SphereLocationSpawnMode::Surface:
            return PointParticleSphereLocationMode::Surface;

        case SphereLocationSpawnMode::Volume:
        default:
            return PointParticleSphereLocationMode::Volume;
        }
    }

    //## Float Curve Eval

    float Evaluate_FloatCurveSegmentLinear(const FloatCurveKeyData& leftKey, const FloatCurveKeyData& rightKey, float x)
    {
        const float width = max(0.0001f, rightKey.time - leftKey.time);
        const float t = clamp((x - leftKey.time) / width, 0.f, 1.f);
        return lerp(leftKey.value, rightKey.value, t);
    }

    float Evaluate_FloatCurveSegmentAutoClamped(const FloatCurveKeyData& leftKey, const FloatCurveKeyData& rightKey, float x)
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

    float Evaluate_FloatConstantCurve(const ConstantCurveFloatDistributionData& curve, float x, float fallbackValue)
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

            switch (leftKey.interpolationMode)
            {
            case FloatCurveInterpolationMode::Constant:
                return leftKey.value;

            case FloatCurveInterpolationMode::CurveAutoClamped:
                return Evaluate_FloatCurveSegmentAutoClamped(leftKey, rightKey, clampedX);

            case FloatCurveInterpolationMode::Linear:
            default:
                return Evaluate_FloatCurveSegmentLinear(leftKey, rightKey, clampedX);
            }
        }

        return curve.keys.back().value;
    }

    Vec2 Evaluate_Vector2CurveSegmentLinear(const Vector2CurveKeyData& leftKey, const Vector2CurveKeyData& rightKey, float x)
    {
        const float width = max(0.0001f, rightKey.time - leftKey.time);
        const float t = clamp((x - leftKey.time) / width, 0.f, 1.f);
        return Vec2{
            lerp(leftKey.value.x, rightKey.value.x, t),
            lerp(leftKey.value.y, rightKey.value.y, t)
        };
    }

    Vec2 Evaluate_Vector2CurveSegmentAutoClamped(const Vector2CurveKeyData& leftKey, const Vector2CurveKeyData& rightKey, float x)
    {
        const float width = max(0.0001f, rightKey.time - leftKey.time);
        const float t = clamp((x - leftKey.time) / width, 0.f, 1.f);
        const Vec2 m0{ leftKey.leaveTangent.x * width, leftKey.leaveTangent.y * width };
        const Vec2 m1{ rightKey.arriveTangent.x * width, rightKey.arriveTangent.y * width };
        const float t2 = t * t;
        const float t3 = t2 * t;
        return Vec2{
            (2.f * t3 - 3.f * t2 + 1.f) * leftKey.value.x +
            (t3 - 2.f * t2 + t) * m0.x +
            (-2.f * t3 + 3.f * t2) * rightKey.value.x +
            (t3 - t2) * m1.x,
            (2.f * t3 - 3.f * t2 + 1.f) * leftKey.value.y +
            (t3 - 2.f * t2 + t) * m0.y +
            (-2.f * t3 + 3.f * t2) * rightKey.value.y +
            (t3 - t2) * m1.y
        };
    }

    Vec2 Evaluate_Vector2ConstantCurve(const ConstantCurveVector2DistributionData& curve, float x, const Vec2& fallbackValue)
    {
        if (curve.keys.empty())
            return fallbackValue;

        if (curve.keys.size() == 1)
            return curve.keys.front().value;

        const float clampedX = clamp(x, 0.f, 1.f);
        for (size_t index = 1; index < curve.keys.size(); ++index)
        {
            const Vector2CurveKeyData& leftKey = curve.keys[index - 1];
            const Vector2CurveKeyData& rightKey = curve.keys[index];
            if (clampedX > rightKey.time)
                continue;

            switch (leftKey.interpolationMode)
            {
            case FloatCurveInterpolationMode::Constant:
                return leftKey.value;

            case FloatCurveInterpolationMode::CurveAutoClamped:
                return Evaluate_Vector2CurveSegmentAutoClamped(leftKey, rightKey, clampedX);

            case FloatCurveInterpolationMode::Linear:
            default:
                return Evaluate_Vector2CurveSegmentLinear(leftKey, rightKey, clampedX);
            }
        }

        return curve.keys.back().value;
    }

    //## Distribution Eval

    float Evaluate_FloatDistributionMin(const FloatDistributionData& data, float fallbackValue)
    {
        if (const auto* constant = get_if<ConstantFloatDistributionData>(&data.payload))
            return constant->value;

        if (const auto* uniform = get_if<UniformFloatDistributionData>(&data.payload))
            return uniform->minValue;

        if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&data.payload))
            return Evaluate_FloatConstantCurve(*curve, 0.f, fallbackValue);

        return fallbackValue;
    }

    float Evaluate_FloatDistributionMax(const FloatDistributionData& data, float fallbackValue)
    {
        if (const auto* constant = get_if<ConstantFloatDistributionData>(&data.payload))
            return constant->value;

        if (const auto* uniform = get_if<UniformFloatDistributionData>(&data.payload))
            return uniform->maxValue;

        if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&data.payload))
            return Evaluate_FloatConstantCurve(*curve, 1.f, fallbackValue);

        return fallbackValue;
    }

    static void Set_SubUVFrameCurveComponent(Vec4& values, uint32 index, float value)
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

    void Fill_SizeByLifeCurveComponent(PointParticleSizeByLifeDesc& desc, uint32 index, float time, const Vec2& value)
    {
        Set_SubUVFrameCurveComponent(desc.curveKeyTimes, index, clamp(time, 0.f, 1.f));
        Set_SubUVFrameCurveComponent(desc.curveKeyValuesX, index, value.x);
        Set_SubUVFrameCurveComponent(desc.curveKeyValuesY, index, value.y);
    }

    void Fill_RotationOverLifeCurveComponent(PointParticleRotationDesc& desc, uint32 index, float time, float value)
    {
        Set_SubUVFrameCurveComponent(desc.rotationOverLifeCurveTimes, index, clamp(time, 0.f, 1.f));
        Set_SubUVFrameCurveComponent(desc.rotationOverLifeCurveValues, index, value);
    }

    void Fill_RotationRateScaleByLifeCurveComponent(PointParticleRotationDesc& desc, uint32 index, float time, float value)
    {
        Set_SubUVFrameCurveComponent(desc.rotationRateScaleByLifeCurveTimes, index, clamp(time, 0.f, 1.f));
        Set_SubUVFrameCurveComponent(desc.rotationRateScaleByLifeCurveValues, index, value);
    }

    void Fill_MeshVector3CurveComponent(MeshVector3CurveRuntimeDesc& desc, uint32 index, float time, const Vec3& value)
    {
        Set_SubUVFrameCurveComponent(desc.curveKeyTimes, index, clamp(time, 0.f, 1.f));
        Set_SubUVFrameCurveComponent(desc.curveKeyValuesX, index, value.x);
        Set_SubUVFrameCurveComponent(desc.curveKeyValuesY, index, value.y);
        Set_SubUVFrameCurveComponent(desc.curveKeyValuesZ, index, value.z);
    }

    float To_RuntimeCurveInterpolationMode(FloatCurveInterpolationMode mode)
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

    EffectMaterialScalarModulationTarget To_RuntimeMaterialScalarTarget(MaterialScalarModulationTargetField target)
    {
        switch (target)
        {
        case MaterialScalarModulationTargetField::OpacityPower:
            return EffectMaterialScalarModulationTarget::OpacityPower;
        case MaterialScalarModulationTargetField::NoiseStrength:
            return EffectMaterialScalarModulationTarget::NoiseStrength;
        case MaterialScalarModulationTargetField::AlphaErosion:
            return EffectMaterialScalarModulationTarget::AlphaErosion;
        case MaterialScalarModulationTargetField::CoreIntensity:
            return EffectMaterialScalarModulationTarget::CoreIntensity;
        case MaterialScalarModulationTargetField::OuterIntensity:
            return EffectMaterialScalarModulationTarget::OuterIntensity;
        case MaterialScalarModulationTargetField::Intensity:
        default:
            return EffectMaterialScalarModulationTarget::Intensity;
        }
    }

    EffectMaterialScalarModulationOperation To_RuntimeMaterialScalarOperation(MaterialScalarModulationOperation operation)
    {
        switch (operation)
        {
        case MaterialScalarModulationOperation::Multiply:
        default:
            return EffectMaterialScalarModulationOperation::Multiply;
        }
    }

    EffectMaterialScalarModulationTimeSource To_RuntimeMaterialScalarTimeSource(MaterialScalarModulationTimeSource timeSource)
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

    void Fill_MaterialScalarCurveComponent(
        EffectMaterialScalarModulationCurveDesc& desc,
        uint32 index,
        const FloatCurveKeyData& key)
    {
        Set_SubUVFrameCurveComponent(desc.keyTimes, index, clamp(key.time, 0.f, 1.f));
        Set_SubUVFrameCurveComponent(desc.keyValues, index, key.value);
        Set_SubUVFrameCurveComponent(desc.keyArriveTangents, index, key.arriveTangent);
        Set_SubUVFrameCurveComponent(desc.keyLeaveTangents, index, key.leaveTangent);
        Set_SubUVFrameCurveComponent(desc.keyModes, index, To_RuntimeCurveInterpolationMode(key.interpolationMode));
    }

    void Fill_MaterialScalarCurvePayload(
        EffectMaterialScalarModulationCurveDesc& desc,
        const FloatDistributionData& distribution)
    {
        constexpr uint32 maxCurveKeys{ 4 };
        desc = EffectMaterialScalarModulationCurveDesc{};

        if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&distribution.payload);
            distribution.mode == DistributionMode::ConstantCurve && curve != nullptr && !curve->keys.empty())
        {
            const uint32 keyCount = min(maxCurveKeys, static_cast<uint32>(curve->keys.size()));
            desc.enabled = true;
            desc.keyCount = keyCount;
            desc.keyTimes = Vec4{};
            desc.keyValues = Vec4{};
            desc.keyArriveTangents = Vec4{};
            desc.keyLeaveTangents = Vec4{};
            desc.keyModes = Vec4{};

            for (uint32 index = 0; index < keyCount; ++index)
                Fill_MaterialScalarCurveComponent(desc, index, curve->keys[index]);

            return;
        }

        float value = 1.f;
        if (const auto* constant = get_if<ConstantFloatDistributionData>(&distribution.payload))
            value = constant->value;
        else if (const auto* uniform = get_if<UniformFloatDistributionData>(&distribution.payload))
            value = (uniform->minValue + uniform->maxValue) * 0.5f;

        desc.enabled = true;
        desc.keyCount = 2;
        desc.keyValues = Vec4{ value, value, 0.f, 0.f };
    }

    void Fill_MaterialScalarModulationPayload(
        EffectMaterialScalarModulationRuntimeDesc& desc,
        const MaterialScalarModulationModuleData& data)
    {
        desc = EffectMaterialScalarModulationRuntimeDesc{};

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
            target.targetField = To_RuntimeMaterialScalarTarget(source.targetField);
            target.operation = To_RuntimeMaterialScalarOperation(source.operation);
            target.timeSource = To_RuntimeMaterialScalarTimeSource(source.timeSource);
            Fill_MaterialScalarCurvePayload(target.curve, source.distribution);
        }
    }

    void Fill_ColorOverLifeColorCurveComponent(
        PointParticleColorOverLifeCurveDesc& desc,
        uint32 index,
        const ColorCurveKeyData& key)
    {
        Set_SubUVFrameCurveComponent(desc.colorCurveTimes, index, clamp(key.time, 0.f, 1.f));
        Set_SubUVFrameCurveComponent(desc.colorCurveValuesR, index, key.value.R());
        Set_SubUVFrameCurveComponent(desc.colorCurveValuesG, index, key.value.G());
        Set_SubUVFrameCurveComponent(desc.colorCurveValuesB, index, key.value.B());
        Set_SubUVFrameCurveComponent(desc.colorCurveArriveR, index, key.arriveTangent.R());
        Set_SubUVFrameCurveComponent(desc.colorCurveArriveG, index, key.arriveTangent.G());
        Set_SubUVFrameCurveComponent(desc.colorCurveArriveB, index, key.arriveTangent.B());
        Set_SubUVFrameCurveComponent(desc.colorCurveLeaveR, index, key.leaveTangent.R());
        Set_SubUVFrameCurveComponent(desc.colorCurveLeaveG, index, key.leaveTangent.G());
        Set_SubUVFrameCurveComponent(desc.colorCurveLeaveB, index, key.leaveTangent.B());
        Set_SubUVFrameCurveComponent(desc.colorCurveModes, index, To_RuntimeCurveInterpolationMode(key.interpolationMode));
    }

    void Fill_ColorOverLifeAlphaCurveComponent(
        PointParticleColorOverLifeCurveDesc& desc,
        uint32 index,
        const FloatCurveKeyData& key)
    {
        Set_SubUVFrameCurveComponent(desc.alphaCurveTimes, index, clamp(key.time, 0.f, 1.f));
        Set_SubUVFrameCurveComponent(desc.alphaCurveValues, index, key.value);
        Set_SubUVFrameCurveComponent(desc.alphaCurveArrive, index, key.arriveTangent);
        Set_SubUVFrameCurveComponent(desc.alphaCurveLeave, index, key.leaveTangent);
        Set_SubUVFrameCurveComponent(desc.alphaCurveModes, index, To_RuntimeCurveInterpolationMode(key.interpolationMode));
    }

    void Fill_ColorOverLifeCurvePayload(
        PointParticleColorOverLifeCurveDesc& desc,
        const ColorOverLifeModuleData& data)
    {
        constexpr uint32 maxColorOverLifeCurveKeys{ 4 };

        if (const auto* curve = get_if<ConstantCurveColorDistributionData>(&data.colorOverLife.payload))
        {
            vector<ColorCurveKeyData> keys = curve->keys;
            ranges::sort(keys, {}, &ColorCurveKeyData::time);
            const uint32 keyCount = min(maxColorOverLifeCurveKeys, static_cast<uint32>(keys.size()));
            if (keyCount > 0)
            {
                desc.colorCurveEnabled = true;
                desc.colorCurveKeyCount = keyCount;
                desc.colorCurveTimes = Vec4{};
                desc.colorCurveValuesR = Vec4{};
                desc.colorCurveValuesG = Vec4{};
                desc.colorCurveValuesB = Vec4{};
                desc.colorCurveArriveR = Vec4{};
                desc.colorCurveArriveG = Vec4{};
                desc.colorCurveArriveB = Vec4{};
                desc.colorCurveLeaveR = Vec4{};
                desc.colorCurveLeaveG = Vec4{};
                desc.colorCurveLeaveB = Vec4{};
                desc.colorCurveModes = Vec4{};
                for (uint32 index = 0; index < keyCount; ++index)
                    Fill_ColorOverLifeColorCurveComponent(desc, index, keys[index]);
            }
        }

        if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&data.alphaOverLife.payload))
        {
            vector<FloatCurveKeyData> keys = curve->keys;
            ranges::sort(keys, {}, &FloatCurveKeyData::time);
            const uint32 keyCount = min(maxColorOverLifeCurveKeys, static_cast<uint32>(keys.size()));
            if (keyCount > 0)
            {
                desc.alphaCurveEnabled = true;
                desc.alphaCurveKeyCount = keyCount;
                desc.alphaCurveTimes = Vec4{};
                desc.alphaCurveValues = Vec4{};
                desc.alphaCurveArrive = Vec4{};
                desc.alphaCurveLeave = Vec4{};
                desc.alphaCurveModes = Vec4{};
                for (uint32 index = 0; index < keyCount; ++index)
                    Fill_ColorOverLifeAlphaCurveComponent(desc, index, keys[index]);
            }
        }
    }

    void Fill_SubUVFrameCurvePayload(PointParticleSubUVFrameOverLifeDesc& desc, const FloatDistributionData& frameIndex)
    {
        desc.frameCurveKeyCount = 1;
        desc.frameCurveTimes = Vec4{};
        desc.frameCurveValues = Vec4{};

        if (const auto* constant = get_if<ConstantFloatDistributionData>(&frameIndex.payload))
        {
            desc.frameCurveValues.x = constant->value;
            return;
        }

        const auto* curve = get_if<ConstantCurveFloatDistributionData>(&frameIndex.payload);
        if (nullptr == curve || curve->keys.empty())
            return;

        constexpr uint32 maxSubUVFrameCurveKeys{ 4 };
        const uint32 keyCount = min(maxSubUVFrameCurveKeys, static_cast<uint32>(curve->keys.size()));
        desc.frameCurveKeyCount = keyCount;

        for (uint32 index = 0; index < keyCount; ++index)
        {
            const FloatCurveKeyData& key = curve->keys[index];
            Set_SubUVFrameCurveComponent(desc.frameCurveTimes, index, clamp(key.time, 0.f, 1.f));
            Set_SubUVFrameCurveComponent(desc.frameCurveValues, index, key.value);
        }
    }

    Vec2 Evaluate_Vector2DistributionMin(const Vector2DistributionData& data, const Vec2& fallbackValue)
    {
        if (const auto* constant = get_if<ConstantVector2DistributionData>(&data.payload))
            return constant->value;

        if (const auto* uniform = get_if<UniformVector2DistributionData>(&data.payload))
            return uniform->minValue;

        if (const auto* curve = get_if<ConstantCurveVector2DistributionData>(&data.payload))
            return Evaluate_Vector2ConstantCurve(*curve, 0.f, fallbackValue);

        return fallbackValue;
    }

    Vec2 Evaluate_Vector2DistributionMax(const Vector2DistributionData& data, const Vec2& fallbackValue)
    {
        if (const auto* constant = get_if<ConstantVector2DistributionData>(&data.payload))
            return constant->value;

        if (const auto* uniform = get_if<UniformVector2DistributionData>(&data.payload))
            return uniform->maxValue;

        if (const auto* curve = get_if<ConstantCurveVector2DistributionData>(&data.payload))
            return Evaluate_Vector2ConstantCurve(*curve, 1.f, fallbackValue);

        return fallbackValue;
    }

    PointParticleSizeByLifeAxisLock To_RuntimeAxisLock(SizeByLifeModuleData::AxisLock axisLock)
    {
        switch (axisLock)
        {
        case SizeByLifeModuleData::AxisLock::X:
            return PointParticleSizeByLifeAxisLock::X;
        case SizeByLifeModuleData::AxisLock::Y:
            return PointParticleSizeByLifeAxisLock::Y;
        case SizeByLifeModuleData::AxisLock::None:
        default:
            return PointParticleSizeByLifeAxisLock::None;
        }
    }

    void Fill_SizeByLifePayload(PointParticleSizeByLifeDesc& desc, const SizeByLifeModuleData& data)
    {
        desc.enabled = true;
        desc.multiplyX = data.multiplyX;
        desc.multiplyY = data.multiplyY;
        desc.axisLock = To_RuntimeAxisLock(data.axisLock);

        const Vec2 startValue = Evaluate_Vector2DistributionMin(data.scaleOverLife, Vec2{ 1.f, 1.f });
        const Vec2 endValue = Evaluate_Vector2DistributionMax(data.scaleOverLife, Vec2{ 1.f, 1.f });
        desc.multiplyXStart = startValue.x;
        desc.multiplyXEnd = endValue.x;
        desc.multiplyYStart = startValue.y;
        desc.multiplyYEnd = endValue.y;

        desc.curveKeyCount = 2;
        desc.curveKeyTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
        desc.curveKeyValuesX = Vec4{ startValue.x, endValue.x, 0.f, 0.f };
        desc.curveKeyValuesY = Vec4{ startValue.y, endValue.y, 0.f, 0.f };

        if (const auto* curve = get_if<ConstantCurveVector2DistributionData>(&data.scaleOverLife.payload))
        {
            vector<Vector2CurveKeyData> keys = curve->keys;
            ranges::sort(keys, {}, &Vector2CurveKeyData::time);
            constexpr uint32 maxSizeByLifeCurveKeys{ 4 };
            const uint32 keyCount = min(maxSizeByLifeCurveKeys, static_cast<uint32>(keys.size()));
            if (keyCount == 0)
                return;

            desc.curveKeyCount = keyCount;
            desc.curveKeyTimes = Vec4{};
            desc.curveKeyValuesX = Vec4{};
            desc.curveKeyValuesY = Vec4{};
            for (uint32 index = 0; index < keyCount; ++index)
                Fill_SizeByLifeCurveComponent(desc, index, keys[index].time, keys[index].value);
        }
    }

    void Fill_RotationOverLifePayload(PointParticleRotationDesc& desc, const RotationOverLifeModuleData& data)
    {
        const float startValue = Evaluate_FloatDistributionMin(data.rotationOverLife, 0.f);
        const float endValue = Evaluate_FloatDistributionMax(data.rotationOverLife, 0.f);
        desc.rotationOverLifeDegrees = Vec2{ startValue, endValue };
        desc.rotationOverLifeCurveKeyCount = 2;
        desc.rotationOverLifeCurveTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
        desc.rotationOverLifeCurveValues = Vec4{ startValue, endValue, 0.f, 0.f };

        if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&data.rotationOverLife.payload))
        {
            vector<FloatCurveKeyData> keys = curve->keys;
            ranges::sort(keys, {}, &FloatCurveKeyData::time);
            constexpr uint32 maxRotationOverLifeCurveKeys{ 4 };
            const uint32 keyCount = min(maxRotationOverLifeCurveKeys, static_cast<uint32>(keys.size()));
            if (keyCount == 0)
                return;

            desc.rotationOverLifeCurveKeyCount = keyCount;
            desc.rotationOverLifeCurveTimes = Vec4{};
            desc.rotationOverLifeCurveValues = Vec4{};
            for (uint32 index = 0; index < keyCount; ++index)
                Fill_RotationOverLifeCurveComponent(desc, index, keys[index].time, keys[index].value);
        }
    }

    void Fill_RotationRateScaleByLifePayload(PointParticleRotationDesc& desc, const RotationRateScaleByLifeModuleData& data)
    {
        const float startValue = Evaluate_FloatDistributionMin(data.scaleOverLife, 1.f);
        const float endValue = Evaluate_FloatDistributionMax(data.scaleOverLife, 1.f);
        desc.rotationRateScaleByLife = Vec2{ startValue, endValue };
        desc.rotationRateScaleByLifeCurveKeyCount = 2;
        desc.rotationRateScaleByLifeCurveTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
        desc.rotationRateScaleByLifeCurveValues = Vec4{ startValue, endValue, 0.f, 0.f };

        if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&data.scaleOverLife.payload))
        {
            vector<FloatCurveKeyData> keys = curve->keys;
            ranges::sort(keys, {}, &FloatCurveKeyData::time);
            constexpr uint32 maxRotationRateScaleByLifeCurveKeys{ 4 };
            const uint32 keyCount = min(maxRotationRateScaleByLifeCurveKeys, static_cast<uint32>(keys.size()));
            if (keyCount == 0)
                return;

            desc.rotationRateScaleByLifeCurveKeyCount = keyCount;
            desc.rotationRateScaleByLifeCurveTimes = Vec4{};
            desc.rotationRateScaleByLifeCurveValues = Vec4{};
            for (uint32 index = 0; index < keyCount; ++index)
                Fill_RotationRateScaleByLifeCurveComponent(desc, index, keys[index].time, keys[index].value);
        }
    }

    Vec3 Evaluate_Vector3DistributionMin(const Vector3DistributionData& data, const Vec3& fallbackValue)
    {
        if (const auto* constant = get_if<ConstantVector3DistributionData>(&data.payload))
            return constant->value;

        if (const auto* uniform = get_if<UniformVector3DistributionData>(&data.payload))
            return uniform->minValue;

        if (const auto* curve = get_if<ConstantCurveVector3DistributionData>(&data.payload))
        {
            if (!curve->keys.empty())
            {
                vector<Vector3CurveKeyData> keys = curve->keys;
                ranges::sort(keys, {}, &Vector3CurveKeyData::time);
                return keys.front().value;
            }
        }

        return fallbackValue;
    }

    Vec3 Evaluate_Vector3DistributionMax(const Vector3DistributionData& data, const Vec3& fallbackValue)
    {
        if (const auto* constant = get_if<ConstantVector3DistributionData>(&data.payload))
            return constant->value;

        if (const auto* uniform = get_if<UniformVector3DistributionData>(&data.payload))
            return uniform->maxValue;

        if (const auto* curve = get_if<ConstantCurveVector3DistributionData>(&data.payload))
        {
            if (!curve->keys.empty())
            {
                vector<Vector3CurveKeyData> keys = curve->keys;
                ranges::sort(keys, {}, &Vector3CurveKeyData::time);
                return keys.back().value;
            }
        }

        return fallbackValue;
    }

    void Fill_MeshVector3CurvePayload(
        MeshVector3CurveRuntimeDesc& desc,
        const Vector3DistributionData& data,
        const Vec3& fallbackValue)
    {
        desc.enabled = true;
        const Vec3 startValue = Evaluate_Vector3DistributionMin(data, fallbackValue);
        const Vec3 endValue = Evaluate_Vector3DistributionMax(data, fallbackValue);
        desc.start = startValue;
        desc.end = endValue;
        desc.curveKeyCount = 2;
        desc.curveKeyTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
        desc.curveKeyValuesX = Vec4{ startValue.x, endValue.x, 0.f, 0.f };
        desc.curveKeyValuesY = Vec4{ startValue.y, endValue.y, 0.f, 0.f };
        desc.curveKeyValuesZ = Vec4{ startValue.z, endValue.z, 0.f, 0.f };

        if (const auto* curve = get_if<ConstantCurveVector3DistributionData>(&data.payload))
        {
            vector<Vector3CurveKeyData> keys = curve->keys;
            ranges::sort(keys, {}, &Vector3CurveKeyData::time);
            constexpr uint32 maxMeshCurveKeys{ 4 };
            const uint32 keyCount = min(maxMeshCurveKeys, static_cast<uint32>(keys.size()));
            if (keyCount == 0)
                return;

            desc.curveKeyCount = keyCount;
            desc.curveKeyTimes = Vec4{};
            desc.curveKeyValuesX = Vec4{};
            desc.curveKeyValuesY = Vec4{};
            desc.curveKeyValuesZ = Vec4{};
            for (uint32 index = 0; index < keyCount; ++index)
                Fill_MeshVector3CurveComponent(desc, index, keys[index].time, keys[index].value);
        }
    }

    void Fill_AccelerationCurvePayload(PointParticleMotionDesc& desc, const Vector3DistributionData& data)
    {
        desc.accelerationCurveEnabled = false;
        desc.accelerationCurveKeyCount = 2;
        desc.accelerationCurveTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
        desc.accelerationCurveValuesX = Vec4{ desc.accelerationMin.x, desc.accelerationMax.x, 0.f, 0.f };
        desc.accelerationCurveValuesY = Vec4{ desc.accelerationMin.y, desc.accelerationMax.y, 0.f, 0.f };
        desc.accelerationCurveValuesZ = Vec4{ desc.accelerationMin.z, desc.accelerationMax.z, 0.f, 0.f };

        const auto* curve = get_if<ConstantCurveVector3DistributionData>(&data.payload);
        if (curve == nullptr)
            return;

        vector<Vector3CurveKeyData> keys = curve->keys;
        ranges::sort(keys, {}, &Vector3CurveKeyData::time);
        constexpr uint32 maxAccelerationCurveKeys{ 4 };
        const uint32 keyCount = min(maxAccelerationCurveKeys, static_cast<uint32>(keys.size()));
        if (keyCount == 0)
            return;

        desc.accelerationCurveEnabled = true;
        desc.accelerationCurveKeyCount = keyCount;
        desc.accelerationCurveTimes = Vec4{};
        desc.accelerationCurveValuesX = Vec4{};
        desc.accelerationCurveValuesY = Vec4{};
        desc.accelerationCurveValuesZ = Vec4{};
        for (uint32 index = 0; index < keyCount; ++index)
        {
            Set_SubUVFrameCurveComponent(desc.accelerationCurveTimes, index, clamp(keys[index].time, 0.f, 1.f));
            Set_SubUVFrameCurveComponent(desc.accelerationCurveValuesX, index, keys[index].value.x);
            Set_SubUVFrameCurveComponent(desc.accelerationCurveValuesY, index, keys[index].value.y);
            Set_SubUVFrameCurveComponent(desc.accelerationCurveValuesZ, index, keys[index].value.z);
        }
    }

    Color Evaluate_ColorDistributionMin(const ColorDistributionData& data, const Color& fallbackValue)
    {
        if (const auto* constant = get_if<ConstantColorDistributionData>(&data.payload))
            return Clamp_PreviewColor(constant->value);

        if (const auto* uniform = get_if<UniformColorDistributionData>(&data.payload))
            return Clamp_PreviewColor(uniform->minValue);

        if (const auto* curve = get_if<ConstantCurveColorDistributionData>(&data.payload))
        {
            if (!curve->keys.empty())
            {
                vector<ColorCurveKeyData> keys = curve->keys;
                ranges::sort(keys, {}, &ColorCurveKeyData::time);
                Color value = keys.front().value;
                value.w = fallbackValue.w;
                return Clamp_PreviewColor(value);
            }
        }

        return Clamp_PreviewColor(fallbackValue);
    }

    Color Evaluate_ColorDistributionMax(const ColorDistributionData& data, const Color& fallbackValue)
    {
        if (const auto* constant = get_if<ConstantColorDistributionData>(&data.payload))
            return Clamp_PreviewColor(constant->value);

        if (const auto* uniform = get_if<UniformColorDistributionData>(&data.payload))
            return Clamp_PreviewColor(uniform->maxValue);

        if (const auto* curve = get_if<ConstantCurveColorDistributionData>(&data.payload))
        {
            if (!curve->keys.empty())
            {
                vector<ColorCurveKeyData> keys = curve->keys;
                ranges::sort(keys, {}, &ColorCurveKeyData::time);
                Color value = keys.back().value;
                value.w = fallbackValue.w;
                return Clamp_PreviewColor(value);
            }
        }

        return Clamp_PreviewColor(fallbackValue);
    }

    Color Evaluate_ColorOverLifeEndpointColorMin(const ColorDistributionData& data, const Color& fallbackValue)
    {
        if (const auto* curve = get_if<ConstantCurveColorDistributionData>(&data.payload))
        {
            if (!curve->keys.empty())
            {
                vector<ColorCurveKeyData> keys = curve->keys;
                ranges::sort(keys, {}, &ColorCurveKeyData::time);
                Color value = keys.back().value;
                value.w = fallbackValue.w;
                return Clamp_PreviewColor(value);
            }
        }

        return Evaluate_ColorDistributionMin(data, fallbackValue);
    }

    Color Evaluate_ColorOverLifeEndpointColorMax(const ColorDistributionData& data, const Color& fallbackValue)
    {
        if (const auto* curve = get_if<ConstantCurveColorDistributionData>(&data.payload))
        {
            if (!curve->keys.empty())
            {
                vector<ColorCurveKeyData> keys = curve->keys;
                ranges::sort(keys, {}, &ColorCurveKeyData::time);
                Color value = keys.back().value;
                value.w = fallbackValue.w;
                return Clamp_PreviewColor(value);
            }
        }

        return Evaluate_ColorDistributionMax(data, fallbackValue);
    }

    float Evaluate_ColorOverLifeEndpointAlphaMin(const FloatDistributionData& data, float fallbackValue)
    {
        if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&data.payload))
            return Evaluate_FloatConstantCurve(*curve, 1.f, fallbackValue);

        return Evaluate_FloatDistributionMin(data, fallbackValue);
    }

    float Evaluate_ColorOverLifeEndpointAlphaMax(const FloatDistributionData& data, float fallbackValue)
    {
        if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&data.payload))
            return Evaluate_FloatConstantCurve(*curve, 1.f, fallbackValue);

        return Evaluate_FloatDistributionMax(data, fallbackValue);
    }
}

Level_EffectEditor::Level_EffectEditor(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Level{ device, context }
{
}

Level_EffectEditor::~Level_EffectEditor()
{
    Free();
}

HRESULT Level_EffectEditor::Initialize()
{
    CHECK_FAILED(Level::Initialize(), E_FAIL);
    CHECK_FAILED(Ready_Lights(), E_FAIL);
    CHECK_FAILED(Ready_Layer_Camera(L"Layer_Camera"), E_FAIL);
    CHECK_FAILED(Ready_Layer_Grid(L"Layer_Grid"), E_FAIL);
    CHECK_FAILED(Ready_Layer_Sky(L"Layer_Sky"), E_FAIL);
    CHECK_FAILED(Ready_Layer_Preview(kPreviewLayerTag), E_FAIL);

    return S_OK;
}

void Level_EffectEditor::Update(float timeDelta)
{
    __super::Update(timeDelta);
}

HRESULT Level_EffectEditor::Render()
{
    CHECK_FAILED(__super::Render(), E_FAIL);

    return S_OK;
}

HRESULT Level_EffectEditor::Restart_PreviewFromAuthoring()
{
    const Shared<Editor_Window> emitterWindow = EDITOR->Get_Window(L"Emitter");
    const Shared<Emitter_View> emitterView = dynamic_pointer_cast<Emitter_View>(emitterWindow);
    if (nullptr == emitterView)
    {
        LOG_ERROR("Emitter view is not available for preview restart.");
        return E_FAIL;
    }

    const Shared<const EffectDefinition> previewDefinition =
        Build_PreviewEffectDefinition(emitterView->Get_Emitters());
    CHECK_NULL(previewDefinition, E_FAIL);

    EffectInstancePool* effectPool = CLIENT ? CLIENT->Get_EffectInstancePool() : nullptr;
    CHECK_NULL(effectPool, E_FAIL);

    CHECK_FAILED(Restart_PreviewFromDefinition(previewDefinition), E_FAIL);
    _currentPreviewDefinition = previewDefinition;

    for (const AuthoringEmitter& emitter : emitterView->Get_Emitters())
        emitterView->Clear_PreviewDirty(emitter.id);

    return S_OK;
}

HRESULT Level_EffectEditor::Reset_PreviewRuntime()
{
    if (_currentPreviewDefinition == nullptr)
        return S_OK;

    return Restart_PreviewFromDefinition(_currentPreviewDefinition);
}

HRESULT Level_EffectEditor::Restart_PreviewFromDefinition(const Shared<const EffectDefinition>& previewDefinition)
{
    CHECK_NULL(previewDefinition, E_FAIL);

    EffectInstancePool* effectPool = CLIENT ? CLIENT->Get_EffectInstancePool() : nullptr;
    CHECK_NULL(effectPool, E_FAIL);

    EffectInstancePool::AcquireDesc acquireDesc{};
    acquireDesc.effectName = Internal::kPreviewPoolKey;
    acquireDesc.definition = previewDefinition;
    acquireDesc.layerLevelIndex = ETOI(LevelType::Static);
    acquireDesc.layerTag = kPreviewLayerTag;
    acquireDesc.trailSampleProvider = _trailPreviewSampleProvider;

    if (_previewPoolHandle.effectObject != nullptr)
    {
        if (SUCCEEDED(effectPool->Reload_Attached(acquireDesc, _previewPoolHandle)))
            return S_OK;

        LOG_WARN("[EffectEditor] Preview reload failed. Falling back to acquire path.");
        effectPool->Release(_previewPoolHandle);
        _previewPoolHandle = {};
    }

    CHECK_FAILED(effectPool->Acquire(acquireDesc, _previewPoolHandle), E_FAIL);
    return S_OK;
}

Shared<const EffectDefinition> Level_EffectEditor::Build_PreviewEffectDefinition(const vector<AuthoringEmitter>& emitters) const
{
    auto definition = make_shared<EffectDefinition>();
    definition->name = "EffectEditorPreview";

    for (const AuthoringEmitter& emitter : emitters)
    {
        if (!emitter.enabled)
            continue;

        switch (emitter.typeData.kind)
        {
        case AuthoringTypeDataKind::Trail:
            definition->emitters.push_back(Build_PreviewTrailEmitterDefinition(emitter));
            break;

        case AuthoringTypeDataKind::Mesh:
            definition->emitters.push_back(Build_PreviewMeshEmitterDefinition(emitter));
            break;

        case AuthoringTypeDataKind::None:
        default:
            definition->emitters.push_back(Build_PreviewEmitterDefinition(emitter));
            break;
        }
    }

    return definition;
}

EffectEmitterDefinition Level_EffectEditor::Build_PreviewEmitterDefinition(const AuthoringEmitter& emitter) const
{
    EffectEmitterDefinition definition{};
    definition.id = emitter.id;
    definition.name = emitter.name;
    definition.kind = EffectEmitterKind::ComputeSprite;
    definition.enabled = emitter.enabled;
    definition.renderLayerOverride = Internal::Resolve_EffectRenderLayerOverride(emitter.renderLayerOverride);

    ComputeSpriteEmitterDesc desc{};
    desc.drawMode = PointParticleDrawMode::DrawIndexedInstancedIndirect;
    desc.renderLayerOverride = definition.renderLayerOverride;

    if (const auto* required = Internal::Find_ModuleData<RequiredModuleData>(emitter, AuthoringModuleType::Required))
    {
        const EffectMaterialInstanceData& material = required->material;

        desc.required.material.materialFamily = material.materialFamily;
        desc.required.material.mainTextureGuid = material.mainTextureGuid;
        desc.required.material.mainTexturePath = material.mainTexturePath;
        desc.required.material.blendMode = material.blendMode;

        if (!material.opacitySource.empty())
            desc.required.material.opacitySource = material.opacitySource;

        desc.required.material.tint = material.tint;
        desc.required.material.intensity = material.intensity;
        desc.required.material.opacityPower = material.opacityPower;
        desc.required.material.noiseTextureGuid = material.noiseTextureGuid;
        desc.required.material.noiseTexturePath = material.noiseTexturePath;
        desc.required.material.maskTextureGuid = material.maskTextureGuid;
        desc.required.material.maskTexturePath = material.maskTexturePath;
        desc.required.material.flowTextureGuid = material.flowTextureGuid;
        desc.required.material.flowTexturePath = material.flowTexturePath;

        desc.required.material.noiseStrength = material.noiseStrength;
        desc.required.material.alphaCutoff = material.alphaCutoff;
        desc.required.material.alphaErosion = material.alphaErosion;
        desc.required.material.mainUVScale = material.mainUVScale;
        desc.required.material.mainUVOffset = material.mainUVOffset;
        desc.required.material.mainUVScrollSpeed = material.mainUVScrollSpeed;
        desc.required.material.mainUVTilingMode = material.mainUVTilingMode;
        desc.required.material.noiseUVScale = material.noiseUVScale;
        desc.required.material.noiseUVOffset = material.noiseUVOffset;
        desc.required.material.noiseUVScrollSpeed = material.noiseUVScrollSpeed;
        desc.required.material.noiseUVTilingMode = material.noiseUVTilingMode;
        desc.required.material.maskUVScale = material.maskUVScale;
        desc.required.material.maskUVOffset = material.maskUVOffset;
        desc.required.material.maskUVScrollSpeed = material.maskUVScrollSpeed;
        desc.required.material.maskUVTilingMode = material.maskUVTilingMode;
        desc.required.material.flowUVScale = material.flowUVScale;
        desc.required.material.flowUVOffset = material.flowUVOffset;
        desc.required.material.flowUVScrollSpeed = material.flowUVScrollSpeed;
        desc.required.material.flowUVTilingMode = material.flowUVTilingMode;

        desc.required.material.additive = material.additive;
        desc.required.material.coreEmissive = material.coreEmissive;
        desc.required.material.refractionIntensity = material.refractionIntensity;
        desc.required.material.refractionPresence = material.refractionPresence;
        desc.required.material.distortionShapeMode = material.distortionShapeMode;
        desc.required.material.distortionShapeRadius = material.distortionShapeRadius;
        desc.required.material.distortionShapeThickness = material.distortionShapeThickness;
        desc.required.material.distortionShapeSoftness = material.distortionShapeSoftness;
        desc.required.material.twoSided = material.twoSided;
        desc.required.material.subUVRows = max(1u, material.subUVRows);
        desc.required.material.subUVCols = max(1u, material.subUVCols);

        desc.required.spriteRender.screenAlignment = Internal::Resolve_EffectScreenAlignment(required->screenAlignment);
        desc.required.spriteRender.directionalAlignmentMode = Internal::Resolve_EffectSpriteDirectionalAlignmentMode(required->directionalAlignmentMode);
        desc.required.spriteRender.spriteTextureAxis = Internal::Resolve_EffectSpriteTextureAxis(required->spriteTextureAxis);
        desc.required.spriteRender.spriteRollOffsetDegrees = required->spriteRollOffsetDegrees;

        desc.required.playback.duration = max(0.0001f, required->duration);
        desc.required.playback.loopCount = required->loopCount;
        desc.required.playback.delay = max(0.f, required->delay);
        desc.required.playback.delayFirstLoopOnly = required->delayFirstLoopOnly;
        desc.required.playback.killOnDeactivate = required->killOnDeactivate;
        desc.required.playback.killOnCompleted = required->killOnCompleted;

        desc.required.drawLimit.useMaxDrawCount = required->useMaxDrawCount;
        desc.required.drawLimit.maxDrawCount = max(1u, required->maxDrawCount);
        definition.localPosition = required->emitterOrigin;
        definition.localRotationDegrees = required->emitterRotationDegrees;
        definition.useLocalSpace = required->useLocalSpace;
    }

    if (const auto* materialScalarModulation = Internal::Find_ModuleData<MaterialScalarModulationModuleData>(
        emitter,
        AuthoringModuleType::MaterialScalarModulation
    ))
    {
        Internal::Fill_MaterialScalarModulationPayload(
            desc.required.material.scalarModulation,
            *materialScalarModulation
        );
    }

    if (const auto* spawn = Internal::Find_ModuleData<SpawnModuleData>(emitter, AuthoringModuleType::Spawn))
    {
        desc.spawn.instanceCount = max(1u, spawn->maxParticleCount);
        desc.spawn.particleSpawn.processSpawnRate = spawn->processSpawnRate;
        desc.spawn.particleSpawn.spawnRate = max(0.f, Internal::Evaluate_FloatDistributionMax(spawn->spawnRate, desc.spawn.particleSpawn.spawnRate));
        desc.spawn.particleSpawn.spawnRateScale = max(
            0.f,
            Internal::Evaluate_FloatDistributionMax(spawn->spawnRateScale, desc.spawn.particleSpawn.spawnRateScale)
        );
        desc.spawn.particleSpawn.processBurstList = spawn->processBurstList;
        desc.spawn.particleSpawn.burstScale = max(0.f, Internal::Evaluate_FloatDistributionMax(spawn->burstScale, desc.spawn.particleSpawn.burstScale));
        desc.spawn.particleSpawn.maxActiveCount = desc.spawn.instanceCount;
        desc.spawn.particleSpawn.bursts.clear();
        desc.spawn.particleSpawn.bursts.reserve(spawn->burstList.size());

        for (const SpawnModuleData::ParticleBurstData& burst : spawn->burstList)
        {
            desc.spawn.particleSpawn.bursts.push_back(
                PointParticleBurstDesc{
                    .time = max(0.f, burst.time),
                    .count = burst.count
                }
            );
        }
    }

    if (const auto* initialSize = Internal::Find_ModuleData<InitialSizeModuleData>(emitter, AuthoringModuleType::InitialSize))
    {
        const Vec2 minSize = Internal::Evaluate_Vector2DistributionMin(initialSize->size, desc.initialSize.sizeMin);
        const Vec2 maxSize = Internal::Evaluate_Vector2DistributionMax(initialSize->size, desc.initialSize.sizeMax);
        desc.initialSize.sizeMin = Vec2{ min(minSize.x, maxSize.x), min(minSize.y, maxSize.y) };
        desc.initialSize.sizeMax = Vec2{ max(minSize.x, maxSize.x), max(minSize.y, maxSize.y) };
    }

    if (const auto* initialLocation = Internal::Find_ModuleData<InitialLocationModuleData>(emitter, AuthoringModuleType::InitialLocation))
    {
        const Vec3 minOffset = Internal::Evaluate_Vector3DistributionMin(initialLocation->location, desc.initialLocation.minOffset);
        const Vec3 maxOffset = Internal::Evaluate_Vector3DistributionMax(initialLocation->location, desc.initialLocation.maxOffset);
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

    if (const auto* sphereLocation = Internal::Find_ModuleData<SphereLocationModuleData>(emitter, AuthoringModuleType::SphereLocation))
    {
        desc.sphereLocation.enabled = true;
        desc.sphereLocation.offset = sphereLocation->offset;
        desc.sphereLocation.radius = max(0.f, sphereLocation->radius);
        desc.sphereLocation.mode = Internal::Resolve_PointParticleSphereLocationMode(sphereLocation->spawnMode);
    }

    if (const auto* lifetime = Internal::Find_ModuleData<LifetimeModuleData>(emitter, AuthoringModuleType::Lifetime))
    {
        desc.lifetime.lifeTime = Vec2{
            Internal::Evaluate_FloatDistributionMin(lifetime->lifeTime, desc.lifetime.lifeTime.x),
            Internal::Evaluate_FloatDistributionMax(lifetime->lifeTime, desc.lifetime.lifeTime.y)
        };
    }

    if (const auto* initialColor = Internal::Find_ModuleData<InitialColorModuleData>(emitter, AuthoringModuleType::InitialColor))
    {
        const Color minColor = Internal::Evaluate_ColorDistributionMin(
            initialColor->color,
            Color{ desc.initialColor.startColorMin.x, desc.initialColor.startColorMin.y, desc.initialColor.startColorMin.z, desc.initialColor.startColorMin.w }
        );
        const Color maxColor = Internal::Evaluate_ColorDistributionMax(
            initialColor->color,
            Color{ desc.initialColor.startColorMax.x, desc.initialColor.startColorMax.y, desc.initialColor.startColorMax.z, desc.initialColor.startColorMax.w }
        );
        const float minAlpha = clamp(
            Internal::Evaluate_FloatDistributionMin(initialColor->alpha, desc.initialColor.startColorMin.w),
            0.f,
            1.f
        );
        const float maxAlpha = clamp(
            Internal::Evaluate_FloatDistributionMax(initialColor->alpha, desc.initialColor.startColorMax.w),
            0.f,
            1.f
        );

        desc.initialColor.startColorMin = Vec4{ minColor.R(), minColor.G(), minColor.B(), minAlpha };
        desc.initialColor.startColorMax = Vec4{ maxColor.R(), maxColor.G(), maxColor.B(), maxAlpha };
        desc.colorOverLife.endColorMin = desc.initialColor.startColorMin;
        desc.colorOverLife.endColorMax = desc.initialColor.startColorMax;
    }

    if (const auto* colorOverLife = Internal::Find_ModuleData<ColorOverLifeModuleData>(emitter, AuthoringModuleType::ColorOverLife))
    {
        const Color minEndColor = Internal::Evaluate_ColorOverLifeEndpointColorMin(
            colorOverLife->colorOverLife,
            Color{ desc.colorOverLife.endColorMin.x, desc.colorOverLife.endColorMin.y, desc.colorOverLife.endColorMin.z, desc.colorOverLife.endColorMin.w }
        );
        const Color maxEndColor = Internal::Evaluate_ColorOverLifeEndpointColorMax(
            colorOverLife->colorOverLife,
            Color{ desc.colorOverLife.endColorMax.x, desc.colorOverLife.endColorMax.y, desc.colorOverLife.endColorMax.z, desc.colorOverLife.endColorMax.w }
        );
        const float minEndAlpha = clamp(
            Internal::Evaluate_ColorOverLifeEndpointAlphaMin(colorOverLife->alphaOverLife, desc.colorOverLife.endColorMin.w),
            0.f,
            1.f
        );
        const float maxEndAlpha = clamp(
            Internal::Evaluate_ColorOverLifeEndpointAlphaMax(colorOverLife->alphaOverLife, desc.colorOverLife.endColorMax.w),
            0.f,
            1.f
        );

        desc.colorOverLife.endColorMin = Vec4{ minEndColor.R(), minEndColor.G(), minEndColor.B(), minEndAlpha };
        desc.colorOverLife.endColorMax = Vec4{ maxEndColor.R(), maxEndColor.G(), maxEndColor.B(), maxEndAlpha };
        Internal::Fill_ColorOverLifeCurvePayload(desc.colorOverLife.curve, *colorOverLife);
    }

    if (const auto* subUvFrameOverLife = Internal::Find_ModuleData<SubUVFrameOverLifeModuleData>(emitter, AuthoringModuleType::SubUVFrameOverLife))
    {
        desc.subUVFrameOverLife.enabled = true;
        desc.subUVFrameOverLife.startFrame = subUvFrameOverLife->startFrame;
        desc.subUVFrameOverLife.endFrame = subUvFrameOverLife->endFrame;
        desc.subUVFrameOverLife.loop = subUvFrameOverLife->loop;
        desc.subUVFrameOverLife.playbackMode = subUvFrameOverLife->playbackMode;
        desc.subUVFrameOverLife.framesPerSecond = max(0.f, subUvFrameOverLife->framesPerSecond);
        Internal::Fill_SubUVFrameCurvePayload(desc.subUVFrameOverLife, subUvFrameOverLife->frameIndex);

        if (const auto* required = Internal::Find_ModuleData<RequiredModuleData>(emitter, AuthoringModuleType::Required))
        {
            const uint32 frameCount = Internal::Resolve_SubUVFrameCount(*required);
            const uint32 lastFrame = frameCount > 0 ? frameCount - 1 : 0;
            desc.subUVFrameOverLife.startFrame = min(desc.subUVFrameOverLife.startFrame, lastFrame);
            desc.subUVFrameOverLife.endFrame = min(desc.subUVFrameOverLife.endFrame, lastFrame);

            if (0 == subUvFrameOverLife->startFrame && 0 == subUvFrameOverLife->endFrame && frameCount > 1)
                desc.subUVFrameOverLife.endFrame = lastFrame;
        }
    }

    if (const auto* sizeByLife = Internal::Find_ModuleData<SizeByLifeModuleData>(emitter, AuthoringModuleType::SizeByLife))
        Internal::Fill_SizeByLifePayload(desc.sizeByLife, *sizeByLife);

    if (const auto* initialVelocity = Internal::Find_ModuleData<InitialVelocityModuleData>(emitter, AuthoringModuleType::InitialVelocity))
    {
        desc.motion.enabled = true;
        desc.motion.initialVelocityEnabled = true;
        desc.motion.initialVelocityInWorldSpace = initialVelocity->inWorldSpace;
        desc.motion.initialVelocityMin =
            Internal::Evaluate_Vector3DistributionMin(initialVelocity->velocity, desc.motion.initialVelocityMin);
        desc.motion.initialVelocityMax =
            Internal::Evaluate_Vector3DistributionMax(initialVelocity->velocity, desc.motion.initialVelocityMax);
    }

    if (const auto* initialRadialVelocity = Internal::Find_ModuleData<InitialRadialVelocityModuleData>(
        emitter,
        AuthoringModuleType::InitialRadialVelocity
    ))
    {
        desc.motion.enabled = true;
        desc.motion.initialRadialVelocityEnabled = true;
        desc.motion.initialRadialVelocityInWorldSpace = initialRadialVelocity->inWorldSpace;
        desc.motion.radialPivot = initialRadialVelocity->radialPivot;
        desc.motion.radialSpeed = Vec2{
            Internal::Evaluate_FloatDistributionMin(initialRadialVelocity->speed, desc.motion.radialSpeed.x),
            Internal::Evaluate_FloatDistributionMax(initialRadialVelocity->speed, desc.motion.radialSpeed.y)
        };
    }

    if (const auto* acceleration = Internal::Find_ModuleData<AccelerationModuleData>(emitter, AuthoringModuleType::Acceleration))
    {
        desc.motion.enabled = true;
        desc.motion.accelerationMin = Internal::Evaluate_Vector3DistributionMin(acceleration->acceleration, desc.motion.accelerationMin);
        desc.motion.accelerationMax = Internal::Evaluate_Vector3DistributionMax(acceleration->acceleration, desc.motion.accelerationMax);
        Internal::Fill_AccelerationCurvePayload(desc.motion, acceleration->acceleration);
    }

    if (const auto* drag = Internal::Find_ModuleData<DragModuleData>(emitter, AuthoringModuleType::Drag))
    {
        desc.motion.enabled = true;
        desc.motion.drag = Vec2{
            max(0.f, Internal::Evaluate_FloatDistributionMin(drag->drag, desc.motion.drag.x)),
            max(0.f, Internal::Evaluate_FloatDistributionMax(drag->drag, desc.motion.drag.y))
        };
    }

    if (const auto* velocityOverLife = Internal::Find_ModuleData<VelocityOverLifeModuleData>(emitter, AuthoringModuleType::VelocityOverLife))
    {
        desc.motion.enabled = true;
        desc.motion.velocityScaleByLife = Vec2{ velocityOverLife->scaleStart, velocityOverLife->scaleEnd };
    }

    if (const auto* initialRotation = Internal::Find_ModuleData<InitialRotationModuleData>(emitter, AuthoringModuleType::InitialRotation))
    {
        desc.rotation.enabled = true;
        desc.rotation.initialRotationDegrees = Vec2{
            Internal::Evaluate_FloatDistributionMin(initialRotation->rotationDegrees, desc.rotation.initialRotationDegrees.x),
            Internal::Evaluate_FloatDistributionMax(initialRotation->rotationDegrees, desc.rotation.initialRotationDegrees.y)
        };
    }

    if (const auto* rotationOverLife = Internal::Find_ModuleData<RotationOverLifeModuleData>(emitter, AuthoringModuleType::RotationOverLife))
    {
        desc.rotation.enabled = true;
        Internal::Fill_RotationOverLifePayload(desc.rotation, *rotationOverLife);
    }

    if (const auto* initialRotationRate = Internal::Find_ModuleData<InitialRotationRateModuleData>(emitter, AuthoringModuleType::InitialRotationRate))
    {
        desc.rotation.enabled = true;
        desc.rotation.initialRotationRateDegrees = Vec2{
            Internal::Evaluate_FloatDistributionMin(initialRotationRate->rotationRateDegrees, desc.rotation.initialRotationRateDegrees.x),
            Internal::Evaluate_FloatDistributionMax(initialRotationRate->rotationRateDegrees, desc.rotation.initialRotationRateDegrees.y)
        };
    }

    if (const auto* rotationRateScaleByLife = Internal::Find_ModuleData<RotationRateScaleByLifeModuleData>(
        emitter,
        AuthoringModuleType::RotationRateScaleByLife
    ))
    {
        desc.rotation.enabled = true;
        Internal::Fill_RotationRateScaleByLifePayload(desc.rotation, *rotationRateScaleByLife);
    }

    definition.concreteDesc = desc;
    return definition;
}

EffectEmitterDefinition Level_EffectEditor::Build_PreviewTrailEmitterDefinition(const AuthoringEmitter& emitter) const
{
    EffectEmitterDefinition definition{};
    definition.id = emitter.id;
    definition.name = emitter.name;
    definition.kind = EffectEmitterKind::ComputeTrail;
    definition.enabled = emitter.enabled;
    definition.renderLayerOverride = Internal::Resolve_EffectRenderLayerOverride(emitter.renderLayerOverride);

    ComputeTrailEmitterDesc emitterDesc{};
    emitterDesc.renderLayerOverride = definition.renderLayerOverride;

    if (const auto* trailData = get_if<TrailTypeData>(&emitter.typeData.payload))
    {
        emitterDesc.width = max(0.001f, trailData->width);
        emitterDesc.segmentLifetime = max(0.001f, trailData->segmentLifetime);
        emitterDesc.historyCount = max(2u, trailData->historyCount);
        emitterDesc.uvTiling = max(0.001f, trailData->uvTiling);
        emitterDesc.maxTrailLength = max(0.f, trailData->maxTrailLength);
        emitterDesc.tailFadeLength = max(0.f, trailData->tailFadeLength);
        emitterDesc.sampleSpacing = max(0.001f, trailData->sampleSpacing);
        emitterDesc.curveSubdivision = max(0u, trailData->curveSubdivision);
        emitterDesc.smoothTangent = trailData->smoothTangent;
        emitterDesc.sideFade = clamp(trailData->sideFade, 0.001f, 0.49f);
    }

    if (const auto* required = Internal::Find_ModuleData<RequiredModuleData>(emitter, AuthoringModuleType::Required))
    {
        const EffectMaterialInstanceData& material = required->material;

        emitterDesc.material.materialFamily = material.materialFamily;
        emitterDesc.material.mainTextureGuid = material.mainTextureGuid;
        emitterDesc.material.mainTexturePath = material.mainTexturePath;
        emitterDesc.material.noiseTextureGuid = material.noiseTextureGuid;
        emitterDesc.material.noiseTexturePath = material.noiseTexturePath;
        emitterDesc.material.maskTextureGuid = material.maskTextureGuid;
        emitterDesc.material.maskTexturePath = material.maskTexturePath;
        emitterDesc.material.flowTextureGuid = material.flowTextureGuid;
        emitterDesc.material.flowTexturePath = material.flowTexturePath;

        emitterDesc.material.blendMode = material.blendMode;
        emitterDesc.material.opacitySource = material.opacitySource.empty() ? "Alpha" : material.opacitySource;
        emitterDesc.material.tint = material.tint;
        emitterDesc.material.intensity = material.intensity;
        emitterDesc.material.opacityPower = material.opacityPower;
        emitterDesc.material.noiseStrength = material.noiseStrength;
        emitterDesc.material.alphaCutoff = material.alphaCutoff;
        emitterDesc.material.alphaErosion = material.alphaErosion;
        emitterDesc.material.mainUVScale = material.mainUVScale;
        emitterDesc.material.mainUVOffset = material.mainUVOffset;
        emitterDesc.material.mainUVScrollSpeed = material.mainUVScrollSpeed;
        emitterDesc.material.mainUVTilingMode = material.mainUVTilingMode;
        emitterDesc.material.noiseUVScale = material.noiseUVScale;
        emitterDesc.material.noiseUVOffset = material.noiseUVOffset;
        emitterDesc.material.noiseUVScrollSpeed = material.noiseUVScrollSpeed;
        emitterDesc.material.noiseUVTilingMode = material.noiseUVTilingMode;
        emitterDesc.material.maskUVScale = material.maskUVScale;
        emitterDesc.material.maskUVOffset = material.maskUVOffset;
        emitterDesc.material.maskUVScrollSpeed = material.maskUVScrollSpeed;
        emitterDesc.material.maskUVTilingMode = material.maskUVTilingMode;
        emitterDesc.material.flowUVScale = material.flowUVScale;
        emitterDesc.material.flowUVOffset = material.flowUVOffset;
        emitterDesc.material.flowUVScrollSpeed = material.flowUVScrollSpeed;
        emitterDesc.material.flowUVTilingMode = material.flowUVTilingMode;

        emitterDesc.material.additive = material.additive;
        emitterDesc.material.coreEmissive = material.coreEmissive;
        emitterDesc.material.refractionIntensity = material.refractionIntensity;
        emitterDesc.material.refractionPresence = material.refractionPresence;
        emitterDesc.material.distortionShapeMode = material.distortionShapeMode;
        emitterDesc.material.distortionShapeRadius = material.distortionShapeRadius;
        emitterDesc.material.distortionShapeThickness = material.distortionShapeThickness;
        emitterDesc.material.distortionShapeSoftness = material.distortionShapeSoftness;
        emitterDesc.material.twoSided = material.twoSided;

        emitterDesc.playback.duration = max(0.0001f, required->duration);
        emitterDesc.playback.loopCount = required->loopCount;
        emitterDesc.playback.delay = max(0.f, required->delay);
        emitterDesc.playback.delayFirstLoopOnly = required->delayFirstLoopOnly;
        definition.localPosition = required->emitterOrigin;
        definition.localRotationDegrees = required->emitterRotationDegrees;
        definition.useLocalSpace = required->useLocalSpace;
    }

    if (const auto* materialScalarModulation = Internal::Find_ModuleData<MaterialScalarModulationModuleData>(
        emitter,
        AuthoringModuleType::MaterialScalarModulation
    ))
    {
        Internal::Fill_MaterialScalarModulationPayload(
            emitterDesc.material.scalarModulation,
            *materialScalarModulation
        );
    }

    if (const auto* lifetime = Internal::Find_ModuleData<LifetimeModuleData>(emitter, AuthoringModuleType::Lifetime))
    {
        emitterDesc.lifetime.lifeTime = Vec2{
            Internal::Evaluate_FloatDistributionMin(lifetime->lifeTime, emitterDesc.lifetime.lifeTime.x),
            Internal::Evaluate_FloatDistributionMax(lifetime->lifeTime, emitterDesc.lifetime.lifeTime.y)
        };
        emitterDesc.segmentLifetime = max(0.001f, emitterDesc.lifetime.lifeTime.y);
    }

    if (const auto* initialColor = Internal::Find_ModuleData<InitialColorModuleData>(emitter, AuthoringModuleType::InitialColor))
    {
        const Color minColor = Internal::Evaluate_ColorDistributionMin(
            initialColor->color,
            Color{
                emitterDesc.initialColor.startColorMin.x,
                emitterDesc.initialColor.startColorMin.y,
                emitterDesc.initialColor.startColorMin.z,
                emitterDesc.initialColor.startColorMin.w
            }
        );
        const Color maxColor = Internal::Evaluate_ColorDistributionMax(
            initialColor->color,
            Color{
                emitterDesc.initialColor.startColorMax.x,
                emitterDesc.initialColor.startColorMax.y,
                emitterDesc.initialColor.startColorMax.z,
                emitterDesc.initialColor.startColorMax.w
            }
        );
        const float minAlpha = clamp(
            Internal::Evaluate_FloatDistributionMin(initialColor->alpha, emitterDesc.initialColor.startColorMin.w),
            0.f,
            1.f
        );
        const float maxAlpha = clamp(
            Internal::Evaluate_FloatDistributionMax(initialColor->alpha, emitterDesc.initialColor.startColorMax.w),
            0.f,
            1.f
        );

        emitterDesc.initialColor.startColorMin = Vec4{ minColor.R(), minColor.G(), minColor.B(), minAlpha };
        emitterDesc.initialColor.startColorMax = Vec4{ maxColor.R(), maxColor.G(), maxColor.B(), maxAlpha };
        emitterDesc.colorOverLife.endColorMin = emitterDesc.initialColor.startColorMin;
        emitterDesc.colorOverLife.endColorMax = emitterDesc.initialColor.startColorMax;
    }

    if (const auto* colorOverLife = Internal::Find_ModuleData<ColorOverLifeModuleData>(emitter, AuthoringModuleType::ColorOverLife))
    {
        const Color minEndColor = Internal::Evaluate_ColorOverLifeEndpointColorMin(
            colorOverLife->colorOverLife,
            Color{
                emitterDesc.colorOverLife.endColorMin.x,
                emitterDesc.colorOverLife.endColorMin.y,
                emitterDesc.colorOverLife.endColorMin.z,
                emitterDesc.colorOverLife.endColorMin.w
            }
        );
        const Color maxEndColor = Internal::Evaluate_ColorOverLifeEndpointColorMax(
            colorOverLife->colorOverLife,
            Color{
                emitterDesc.colorOverLife.endColorMax.x,
                emitterDesc.colorOverLife.endColorMax.y,
                emitterDesc.colorOverLife.endColorMax.z,
                emitterDesc.colorOverLife.endColorMax.w
            }
        );
        const float minEndAlpha = clamp(
            Internal::Evaluate_ColorOverLifeEndpointAlphaMin(colorOverLife->alphaOverLife, emitterDesc.colorOverLife.endColorMin.w),
            0.f,
            1.f
        );
        const float maxEndAlpha = clamp(
            Internal::Evaluate_ColorOverLifeEndpointAlphaMax(colorOverLife->alphaOverLife, emitterDesc.colorOverLife.endColorMax.w),
            0.f,
            1.f
        );

        emitterDesc.colorOverLife.endColorMin = Vec4{ minEndColor.R(), minEndColor.G(), minEndColor.B(), minEndAlpha };
        emitterDesc.colorOverLife.endColorMax = Vec4{ maxEndColor.R(), maxEndColor.G(), maxEndColor.B(), maxEndAlpha };
        Internal::Fill_ColorOverLifeCurvePayload(emitterDesc.colorOverLife.curve, *colorOverLife);
    }

    if (const auto* sizeByLife = Internal::Find_ModuleData<SizeByLifeModuleData>(emitter, AuthoringModuleType::SizeByLife))
        Internal::Fill_SizeByLifePayload(emitterDesc.sizeByLife, *sizeByLife);

    if (const auto* spawnPerUnit = Internal::Find_ModuleData<SpawnPerUnitModuleData>(emitter, AuthoringModuleType::SpawnPerUnit))
    {
        emitterDesc.spawnPerUnit.enabled = spawnPerUnit->enabled;
        emitterDesc.spawnPerUnit.spawnPerUnit =
            max(0.f, Internal::Evaluate_FloatDistributionMax(spawnPerUnit->spawnPerUnit, emitterDesc.spawnPerUnit.spawnPerUnit));
        emitterDesc.spawnPerUnit.unitScalar = max(0.0001f, spawnPerUnit->unitScalar);
        emitterDesc.spawnPerUnit.movementTolerance = max(0.f, spawnPerUnit->movementTolerance);
        emitterDesc.spawnPerUnit.maxFrameDistance = max(0.f, spawnPerUnit->maxFrameDistance);
    }

    emitterDesc.sampleProvider = _trailPreviewSampleProvider;
    definition.concreteDesc = emitterDesc;
    return definition;
}

EffectEmitterDefinition Level_EffectEditor::Build_PreviewMeshEmitterDefinition(const AuthoringEmitter& emitter) const
{
    EffectEmitterDefinition definition{};
    definition.id = emitter.id;
    definition.name = emitter.name;
    definition.kind = EffectEmitterKind::Mesh;
    definition.enabled = emitter.enabled;
    definition.renderLayerOverride = Internal::Resolve_EffectRenderLayerOverride(emitter.renderLayerOverride);

    MeshEmitterDesc emitterDesc{};
    emitterDesc.renderLayerOverride = definition.renderLayerOverride;

    if (const auto* meshData = get_if<MeshTypeData>(&emitter.typeData.payload))
    {
        emitterDesc.modelGuid = meshData->modelGuid;
        emitterDesc.modelPath = meshData->modelPath;
        emitterDesc.previewScale = meshData->previewScale;
        emitterDesc.useModelMaterials = meshData->useModelMaterials;

        if (meshData->hasAssignedMaterialInstance)
            Internal::Copy_MaterialToRuntimeDesc(meshData->assignedMaterial, emitterDesc.material);
        else
        {
            const fs::path assignedMaterialPath = Internal::Resolve_AuthoredAssetPath(
                meshData->assignedEffectMaterialGuid,
                meshData->assignedEffectMaterialPath
            );
            EffectMaterialInstanceData assignedMaterial{};
            if (!assignedMaterialPath.empty() && EffectMaterialPresetReader::Read(assignedMaterialPath, assignedMaterial))
                Internal::Copy_MaterialToRuntimeDesc(assignedMaterial, emitterDesc.material);
        }
    }

    if (const auto* required = Internal::Find_ModuleData<RequiredModuleData>(emitter, AuthoringModuleType::Required))
    {
        emitterDesc.playback.duration = max(0.0001f, required->duration);
        emitterDesc.playback.loopCount = required->loopCount;
        emitterDesc.playback.delay = max(0.f, required->delay);
        emitterDesc.playback.delayFirstLoopOnly = required->delayFirstLoopOnly;
        emitterDesc.playback.killOnDeactivate = required->killOnDeactivate;
        emitterDesc.playback.killOnCompleted = required->killOnCompleted;

        emitterDesc.drawLimit.useMaxDrawCount = required->useMaxDrawCount;
        emitterDesc.drawLimit.maxDrawCount = max(1u, required->maxDrawCount);
        emitterDesc.meshTransform.alignment = Internal::Resolve_EffectScreenAlignment(required->screenAlignment);

        definition.localPosition = required->emitterOrigin;
        definition.localRotationDegrees = required->emitterRotationDegrees;
        definition.useLocalSpace = required->useLocalSpace;
    }

    if (const auto* materialScalarModulation = Internal::Find_ModuleData<MaterialScalarModulationModuleData>(
        emitter,
        AuthoringModuleType::MaterialScalarModulation
    ))
    {
        Internal::Fill_MaterialScalarModulationPayload(
            emitterDesc.material.scalarModulation,
            *materialScalarModulation
        );
    }

    if (const auto* spawn = Internal::Find_ModuleData<SpawnModuleData>(emitter, AuthoringModuleType::Spawn))
    {
        emitterDesc.spawn.instanceCount = max(1u, spawn->maxParticleCount);
        emitterDesc.spawn.particleSpawn.processSpawnRate = spawn->processSpawnRate;
        emitterDesc.spawn.particleSpawn.spawnRate = max(
            0.f,
            Internal::Evaluate_FloatDistributionMax(spawn->spawnRate, emitterDesc.spawn.particleSpawn.spawnRate)
        );
        emitterDesc.spawn.particleSpawn.spawnRateScale = max(
            0.f,
            Internal::Evaluate_FloatDistributionMax(spawn->spawnRateScale, emitterDesc.spawn.particleSpawn.spawnRateScale)
        );
        emitterDesc.spawn.particleSpawn.processBurstList = spawn->processBurstList;
        emitterDesc.spawn.particleSpawn.burstScale = max(
            0.f,
            Internal::Evaluate_FloatDistributionMax(spawn->burstScale, emitterDesc.spawn.particleSpawn.burstScale)
        );
        emitterDesc.spawn.particleSpawn.maxActiveCount = emitterDesc.spawn.instanceCount;
        emitterDesc.spawn.particleSpawn.bursts.clear();
        emitterDesc.spawn.particleSpawn.bursts.reserve(spawn->burstList.size());

        for (const SpawnModuleData::ParticleBurstData& burst : spawn->burstList)
        {
            emitterDesc.spawn.particleSpawn.bursts.push_back(
                PointParticleBurstDesc{
                    .time = max(0.f, burst.time),
                    .count = burst.count
                }
            );
        }
    }

    if (const auto* initialMeshSize = Internal::Find_ModuleData<InitialMeshSizeModuleData>(emitter, AuthoringModuleType::InitialMeshSize))
    {
        emitterDesc.meshTransform.initialScaleEnabled = true;
        const Vec3 meshMinSize = Internal::Evaluate_Vector3DistributionMin(initialMeshSize->size, emitterDesc.meshTransform.initialScaleMin);
        const Vec3 meshMaxSize = Internal::Evaluate_Vector3DistributionMax(initialMeshSize->size, emitterDesc.meshTransform.initialScaleMax);
        emitterDesc.meshTransform.initialScaleMin = Vec3{
            min(meshMinSize.x, meshMaxSize.x),
            min(meshMinSize.y, meshMaxSize.y),
            min(meshMinSize.z, meshMaxSize.z)
        };
        emitterDesc.meshTransform.initialScaleMax = Vec3{
            max(meshMinSize.x, meshMaxSize.x),
            max(meshMinSize.y, meshMaxSize.y),
            max(meshMinSize.z, meshMaxSize.z)
        };
    }

    if (const auto* initialLocation = Internal::Find_ModuleData<InitialLocationModuleData>(emitter, AuthoringModuleType::InitialLocation))
    {
        const Vec3 minOffset = Internal::Evaluate_Vector3DistributionMin(initialLocation->location, emitterDesc.initialLocation.minOffset);
        const Vec3 maxOffset = Internal::Evaluate_Vector3DistributionMax(initialLocation->location, emitterDesc.initialLocation.maxOffset);
        emitterDesc.initialLocation.enabled = true;
        emitterDesc.initialLocation.minOffset = Vec3{
            min(minOffset.x, maxOffset.x),
            min(minOffset.y, maxOffset.y),
            min(minOffset.z, maxOffset.z)
        };
        emitterDesc.initialLocation.maxOffset = Vec3{
            max(minOffset.x, maxOffset.x),
            max(minOffset.y, maxOffset.y),
            max(minOffset.z, maxOffset.z)
        };
    }

    if (const auto* sphereLocation = Internal::Find_ModuleData<SphereLocationModuleData>(emitter, AuthoringModuleType::SphereLocation))
    {
        emitterDesc.sphereLocation.enabled = true;
        emitterDesc.sphereLocation.offset = sphereLocation->offset;
        emitterDesc.sphereLocation.radius = max(0.f, sphereLocation->radius);
        emitterDesc.sphereLocation.mode = sphereLocation->spawnMode == SphereLocationSpawnMode::Surface
                                          ? PointParticleSphereLocationMode::Surface
                                          : PointParticleSphereLocationMode::Volume;
    }

    if (const auto* lifetime = Internal::Find_ModuleData<LifetimeModuleData>(emitter, AuthoringModuleType::Lifetime))
    {
        emitterDesc.lifetime.lifeTime = Vec2{
            Internal::Evaluate_FloatDistributionMin(lifetime->lifeTime, emitterDesc.lifetime.lifeTime.x),
            Internal::Evaluate_FloatDistributionMax(lifetime->lifeTime, emitterDesc.lifetime.lifeTime.y)
        };
    }

    if (const auto* initialColor = Internal::Find_ModuleData<InitialColorModuleData>(emitter, AuthoringModuleType::InitialColor))
    {
        const Color minColor = Internal::Evaluate_ColorDistributionMin(
            initialColor->color,
            Color{
                emitterDesc.initialColor.startColorMin.x,
                emitterDesc.initialColor.startColorMin.y,
                emitterDesc.initialColor.startColorMin.z,
                emitterDesc.initialColor.startColorMin.w
            }
        );
        const Color maxColor = Internal::Evaluate_ColorDistributionMax(
            initialColor->color,
            Color{
                emitterDesc.initialColor.startColorMax.x,
                emitterDesc.initialColor.startColorMax.y,
                emitterDesc.initialColor.startColorMax.z,
                emitterDesc.initialColor.startColorMax.w
            }
        );
        const float minAlpha = clamp(
            Internal::Evaluate_FloatDistributionMin(initialColor->alpha, emitterDesc.initialColor.startColorMin.w),
            0.f,
            1.f
        );
        const float maxAlpha = clamp(
            Internal::Evaluate_FloatDistributionMax(initialColor->alpha, emitterDesc.initialColor.startColorMax.w),
            0.f,
            1.f
        );

        emitterDesc.initialColor.startColorMin = Vec4{ minColor.R(), minColor.G(), minColor.B(), minAlpha };
        emitterDesc.initialColor.startColorMax = Vec4{ maxColor.R(), maxColor.G(), maxColor.B(), maxAlpha };
        emitterDesc.colorOverLife.endColorMin = emitterDesc.initialColor.startColorMin;
        emitterDesc.colorOverLife.endColorMax = emitterDesc.initialColor.startColorMax;
    }

    if (const auto* colorOverLife = Internal::Find_ModuleData<ColorOverLifeModuleData>(emitter, AuthoringModuleType::ColorOverLife))
    {
        const Color minEndColor = Internal::Evaluate_ColorOverLifeEndpointColorMin(
            colorOverLife->colorOverLife,
            Color{
                emitterDesc.colorOverLife.endColorMin.x,
                emitterDesc.colorOverLife.endColorMin.y,
                emitterDesc.colorOverLife.endColorMin.z,
                emitterDesc.colorOverLife.endColorMin.w
            }
        );
        const Color maxEndColor = Internal::Evaluate_ColorOverLifeEndpointColorMax(
            colorOverLife->colorOverLife,
            Color{
                emitterDesc.colorOverLife.endColorMax.x,
                emitterDesc.colorOverLife.endColorMax.y,
                emitterDesc.colorOverLife.endColorMax.z,
                emitterDesc.colorOverLife.endColorMax.w
            }
        );
        const float minEndAlpha = clamp(
            Internal::Evaluate_ColorOverLifeEndpointAlphaMin(colorOverLife->alphaOverLife, emitterDesc.colorOverLife.endColorMin.w),
            0.f,
            1.f
        );
        const float maxEndAlpha = clamp(
            Internal::Evaluate_ColorOverLifeEndpointAlphaMax(colorOverLife->alphaOverLife, emitterDesc.colorOverLife.endColorMax.w),
            0.f,
            1.f
        );

        emitterDesc.colorOverLife.endColorMin = Vec4{ minEndColor.R(), minEndColor.G(), minEndColor.B(), minEndAlpha };
        emitterDesc.colorOverLife.endColorMax = Vec4{ maxEndColor.R(), maxEndColor.G(), maxEndColor.B(), maxEndAlpha };
        Internal::Fill_ColorOverLifeCurvePayload(emitterDesc.colorOverLife.curve, *colorOverLife);
    }

    if (const auto* meshSizeByLife = Internal::Find_ModuleData<MeshSizeByLifeModuleData>(emitter, AuthoringModuleType::MeshSizeByLife))
    {
        Internal::Fill_MeshVector3CurvePayload(
            emitterDesc.meshTransform.scaleByLife,
            meshSizeByLife->scaleOverLife,
            Vec3{ 1.f, 1.f, 1.f }
        );
        if (!meshSizeByLife->multiplyX)
        {
            emitterDesc.meshTransform.scaleByLife.start.x = 1.f;
            emitterDesc.meshTransform.scaleByLife.end.x = 1.f;
            emitterDesc.meshTransform.scaleByLife.curveKeyValuesX = Vec4{ 1.f, 1.f, 1.f, 1.f };
        }
        if (!meshSizeByLife->multiplyY)
        {
            emitterDesc.meshTransform.scaleByLife.start.y = 1.f;
            emitterDesc.meshTransform.scaleByLife.end.y = 1.f;
            emitterDesc.meshTransform.scaleByLife.curveKeyValuesY = Vec4{ 1.f, 1.f, 1.f, 1.f };
        }
        if (!meshSizeByLife->multiplyZ)
        {
            emitterDesc.meshTransform.scaleByLife.start.z = 1.f;
            emitterDesc.meshTransform.scaleByLife.end.z = 1.f;
            emitterDesc.meshTransform.scaleByLife.curveKeyValuesZ = Vec4{ 1.f, 1.f, 1.f, 1.f };
        }
    }

    if (const auto* initialVelocity = Internal::Find_ModuleData<InitialVelocityModuleData>(emitter, AuthoringModuleType::InitialVelocity))
    {
        emitterDesc.motion.enabled = true;
        emitterDesc.motion.initialVelocityEnabled = true;
        emitterDesc.motion.initialVelocityInWorldSpace = initialVelocity->inWorldSpace;
        emitterDesc.motion.initialVelocityMin =
            Internal::Evaluate_Vector3DistributionMin(initialVelocity->velocity, emitterDesc.motion.initialVelocityMin);
        emitterDesc.motion.initialVelocityMax =
            Internal::Evaluate_Vector3DistributionMax(initialVelocity->velocity, emitterDesc.motion.initialVelocityMax);
    }

    if (const auto* initialRadialVelocity = Internal::Find_ModuleData<InitialRadialVelocityModuleData>(
        emitter,
        AuthoringModuleType::InitialRadialVelocity
    ))
    {
        emitterDesc.motion.enabled = true;
        emitterDesc.motion.initialRadialVelocityEnabled = true;
        emitterDesc.motion.initialRadialVelocityInWorldSpace = initialRadialVelocity->inWorldSpace;
        emitterDesc.motion.radialPivot = initialRadialVelocity->radialPivot;
        emitterDesc.motion.radialSpeed = Vec2{
            Internal::Evaluate_FloatDistributionMin(initialRadialVelocity->speed, emitterDesc.motion.radialSpeed.x),
            Internal::Evaluate_FloatDistributionMax(initialRadialVelocity->speed, emitterDesc.motion.radialSpeed.y)
        };
    }

    if (const auto* acceleration = Internal::Find_ModuleData<AccelerationModuleData>(emitter, AuthoringModuleType::Acceleration))
    {
        emitterDesc.motion.enabled = true;
        emitterDesc.motion.accelerationMin =
            Internal::Evaluate_Vector3DistributionMin(acceleration->acceleration, emitterDesc.motion.accelerationMin);
        emitterDesc.motion.accelerationMax =
            Internal::Evaluate_Vector3DistributionMax(acceleration->acceleration, emitterDesc.motion.accelerationMax);
        Internal::Fill_AccelerationCurvePayload(emitterDesc.motion, acceleration->acceleration);
    }

    if (const auto* drag = Internal::Find_ModuleData<DragModuleData>(emitter, AuthoringModuleType::Drag))
    {
        emitterDesc.motion.enabled = true;
        emitterDesc.motion.drag = Vec2{
            max(0.f, Internal::Evaluate_FloatDistributionMin(drag->drag, emitterDesc.motion.drag.x)),
            max(0.f, Internal::Evaluate_FloatDistributionMax(drag->drag, emitterDesc.motion.drag.y))
        };
    }

    if (const auto* velocityOverLife = Internal::Find_ModuleData<VelocityOverLifeModuleData>(emitter, AuthoringModuleType::VelocityOverLife))
    {
        emitterDesc.motion.enabled = true;
        emitterDesc.motion.velocityScaleByLife = Vec2{ velocityOverLife->scaleStart, velocityOverLife->scaleEnd };
    }

    if (const auto* initialMeshRotation = Internal::Find_ModuleData<InitialMeshRotationModuleData>(emitter, AuthoringModuleType::InitialMeshRotation))
    {
        emitterDesc.meshTransform.rotationEnabled = true;
        const Vec3 meshMinRotation = Internal::Evaluate_Vector3DistributionMin(initialMeshRotation->rotationDegrees, Vec3{});
        const Vec3 meshMaxRotation = Internal::Evaluate_Vector3DistributionMax(initialMeshRotation->rotationDegrees, Vec3{});
        emitterDesc.meshTransform.initialRotationDegreesMin = Vec3{
            min(meshMinRotation.x, meshMaxRotation.x),
            min(meshMinRotation.y, meshMaxRotation.y),
            min(meshMinRotation.z, meshMaxRotation.z)
        };
        emitterDesc.meshTransform.initialRotationDegreesMax = Vec3{
            max(meshMinRotation.x, meshMaxRotation.x),
            max(meshMinRotation.y, meshMaxRotation.y),
            max(meshMinRotation.z, meshMaxRotation.z)
        };
    }

    if (const auto* meshRotationOverLife = Internal::Find_ModuleData<MeshRotationOverLifeModuleData>(emitter, AuthoringModuleType::MeshRotationOverLife))
    {
        emitterDesc.meshTransform.rotationEnabled = true;
        Internal::Fill_MeshVector3CurvePayload(emitterDesc.meshTransform.rotationByLife, meshRotationOverLife->rotationOverLife, Vec3{});
    }

    if (const auto* initialMeshRotationRate = Internal::Find_ModuleData<InitialMeshRotationRateModuleData>(emitter, AuthoringModuleType::InitialMeshRotationRate))
    {
        emitterDesc.meshTransform.rotationEnabled = true;
        const Vec3 meshMinRate = Internal::Evaluate_Vector3DistributionMin(initialMeshRotationRate->rotationRateDegrees, Vec3{});
        const Vec3 meshMaxRate = Internal::Evaluate_Vector3DistributionMax(initialMeshRotationRate->rotationRateDegrees, Vec3{});
        emitterDesc.meshTransform.initialAngularVelocityDegreesMin = Vec3{
            min(meshMinRate.x, meshMaxRate.x),
            min(meshMinRate.y, meshMaxRate.y),
            min(meshMinRate.z, meshMaxRate.z)
        };
        emitterDesc.meshTransform.initialAngularVelocityDegreesMax = Vec3{
            max(meshMinRate.x, meshMaxRate.x),
            max(meshMinRate.y, meshMaxRate.y),
            max(meshMinRate.z, meshMaxRate.z)
        };
    }

    if (const auto* meshRotationRateScaleByLife = Internal::Find_ModuleData<MeshRotationRateScaleByLifeModuleData>(
        emitter,
        AuthoringModuleType::MeshRotationRateScaleByLife
    ))
    {
        emitterDesc.meshTransform.rotationEnabled = true;
        Internal::Fill_MeshVector3CurvePayload(
            emitterDesc.meshTransform.angularVelocityScaleByLife,
            meshRotationRateScaleByLife->scaleOverLife,
            Vec3{ 1.f, 1.f, 1.f }
        );
    }

    definition.concreteDesc = emitterDesc;
    return definition;
}

HRESULT Level_EffectEditor::Ready_Lights()
{
    LIGHT_DESC lightDesc{};
    lightDesc.type = LightType::Directional;
    lightDesc.direction = Vec4(1.f, -1.f, 1.f, 0.f);
    lightDesc.direction.Normalize();
    lightDesc.diffuse = Vec4(1.f, 1.f, 1.f, 1.f);
    lightDesc.ambient = Vec4(0.6f, 0.6f, 0.6f, 1.f);
    lightDesc.specular = Vec4(1.f, 1.f, 1.f, 1.f);

    CHECK_FAILED(GAME->Add_Light(lightDesc), E_FAIL);

    return S_OK;
}

HRESULT Level_EffectEditor::Ready_Layer_Camera(const wstring& layerTag)
{
    EffectEditorCamera::EffectEditorCameraDesc cameraDesc{};
    cameraDesc.speedPerSec = 5.f; //!! 초기 카메라 속도
    cameraDesc.degreePerSec = 180.f;
    cameraDesc.eye = Vec4(0.f, 3.f, -3.5f, 1.f); //!! 초기 카메라 거리
    cameraDesc.at = Vec4(0.f, 0.f, 0.f, 1.f);
    cameraDesc.fovy = XMConvertToRadians(60.f);
    cameraDesc.nearPlane = 0.1f;
    cameraDesc.farPlane = 500.f;
    cameraDesc.mouseSensor = 0.065f;
    cameraDesc.yaw = 0.f;
    cameraDesc.pitch = 0.f;
    cameraDesc.pivot = Vec3(0.f, 0.f, 0.f);

    const Shared<EffectEditorCamera> previewCamera =
        EffectEditorCamera::Create(_device, _context);
    CHECK_NULL(previewCamera, E_FAIL);
    CHECK_FAILED(previewCamera->Initialize(&cameraDesc), E_FAIL);
    EDITOR->Register_PreviewCamera(previewCamera);
    CHECK_FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), layerTag, previewCamera), E_FAIL);

    GAME->Change_ActiveCamera(previewCamera);

    return S_OK;
}

HRESULT Level_EffectEditor::Ready_Layer_Grid(const wstring& layerTag)
{
    if (layerTag.empty())
        return E_FAIL;

    const Shared<EffectEditorSceneGrid> sceneGrid =
        EffectEditorSceneGrid::Create(_device, _context);
    CHECK_NULL(sceneGrid, E_FAIL);
    CHECK_FAILED(sceneGrid->Initialize(nullptr), E_FAIL);
    CHECK_FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), layerTag, sceneGrid), E_FAIL);

    return S_OK;
}

HRESULT Level_EffectEditor::Ready_Layer_Sky(const wstring& layerTag)
{
    if (layerTag.empty())
        return E_FAIL;

    const Shared<EffectEditorSkybox> skybox =
        EffectEditorSkybox::Create(_device, _context);
    CHECK_NULL(skybox, E_FAIL);
    CHECK_FAILED(skybox->Initialize(nullptr), E_FAIL);
    CHECK_FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), layerTag, skybox), E_FAIL);

    return S_OK;
}

HRESULT Level_EffectEditor::Ready_Layer_Preview(const wstring& layerTag)
{
    if (layerTag.empty())
        return E_FAIL;

    const Shared<TrailPreviewPlayerObject> trailPreviewPlayer =
        TrailPreviewPlayerObject::Create(_device, _context);
    CHECK_NULL(trailPreviewPlayer, E_FAIL);
    CHECK_FAILED(trailPreviewPlayer->Initialize(nullptr), E_FAIL);
    CHECK_FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), layerTag, trailPreviewPlayer), E_FAIL);

    const Shared<TrailPreviewWeaponObject> trailPreviewWeapon = trailPreviewPlayer->Get_PreviewWeapon();
    if (trailPreviewWeapon == nullptr)
    {
        LOG_ERROR("[TrailPreview] TrailPreviewPlayerObject did not create Part_Weapon.");
        return E_FAIL;
    }

    _trailPreviewSampleProvider = trailPreviewWeapon;

    TrailPreviewController::PreviewDesc controllerDesc{};
    controllerDesc.player = trailPreviewPlayer;

    const Shared<TrailPreviewController> trailPreviewController =
        TrailPreviewController::Create(_device, _context);
    CHECK_NULL(trailPreviewController, E_FAIL);
    CHECK_FAILED(trailPreviewController->Initialize(&controllerDesc), E_FAIL);
    CHECK_FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), layerTag, trailPreviewController), E_FAIL);

    return S_OK;
}

void Level_EffectEditor::Clear_PreviewObjects()
{
    EffectInstancePool* effectPool = CLIENT ? CLIENT->Get_EffectInstancePool() : nullptr;
    if (nullptr == effectPool)
        return;

    effectPool->Release(_previewPoolHandle);
    _previewPoolHandle = {};
    effectPool->Discard_Inactive(Internal::kPreviewPoolKey);
}

Shared<Level_EffectEditor> Level_EffectEditor::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Level_EffectEditor>(device, context);

    if (FAILED(instance->Initialize()))
    {
        LOG_CRITICAL("Failed to Create : Level_EffectEditor");
        return nullptr;
    }

    return instance;
}

void Level_EffectEditor::Free()
{
    Clear_PreviewObjects();
    __super::Free();
}

NS_END
