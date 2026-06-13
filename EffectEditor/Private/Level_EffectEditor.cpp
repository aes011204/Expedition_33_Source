#include "Level_EffectEditor.h"

#include "Camera.h"
#include "ClientInstance.h"
#include "EffectEditorCamera.h"
#include "EffectEditorInstance.h"
#include "EffectEditorPreviewPlane.h"
#include "EffectEditorSceneGrid.h"
#include "EffectEditorSkybox.h"
#include "EffectMaterialScalarModulationPreview.h"
#include "EffectMaterialPresetReader.h"
#include "EffectRuntime_Lowering.h"
#include "Emitter_View.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "TrailPreviewController.h"
#include "TrailPreviewPlayerObject.h"
#include "TrailPreviewWeaponObject.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    constexpr auto kPreviewPoolKey = L"EffectEditorPreview";
    constexpr auto kPreviewTrailStrokePoolKey = L"EffectEditorPreviewTrailStroke";
    constexpr uint32 kRibbonAutoHistoryMaxCount{ 4096 };
    constexpr float kRibbonAutoHistoryFrameStep{ 0.016f };
    constexpr uint32 kRibbonAutoHistoryMargin{ 8 };

    class TrailPreviewStrokeSampleProvider final : public IEffectTrailSampleProvider
    {
    public:
        TrailPreviewStrokeSampleProvider(
            const Weak<IEffectTrailSampleProvider>& sourceProvider,
            const Shared<bool>& sampleEnabled)
            : _sourceProvider(sourceProvider)
            , _sampleEnabled(sampleEnabled)
        {
        }

        bool Try_GetTrailSample(EffectTrailSample& outSample) const override
        {
            if (_sampleEnabled == nullptr || !*_sampleEnabled)
                return false;

            const Shared<IEffectTrailSampleProvider> sourceProvider = _sourceProvider.lock();
            if (sourceProvider == nullptr)
                return false;

            return sourceProvider->Try_GetTrailSample(outSample);
        }

    private:
        Weak<IEffectTrailSampleProvider> _sourceProvider{};
        Shared<bool> _sampleEnabled{};
    };

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

    float Resolve_RequiredDuration(const AuthoringEmitter& emitter)
    {
        const RequiredModuleData* required = Find_ModuleData<RequiredModuleData>(emitter, AuthoringModuleType::Required);
        if (required == nullptr)
            return 0.f;

        if (required->useDurationRange)
            return max(required->duration, required->durationLow);

        return required->duration;
    }

    float Resolve_TrailPreviewGateDuration(const vector<AuthoringEmitter>& emitters)
    {
        float trailDuration = 0.f;
        float anyDuration = 0.f;

        for (const AuthoringEmitter& emitter : emitters)
        {
            if (!emitter.enabled)
                continue;

            const float duration = max(0.f, Resolve_RequiredDuration(emitter));
            if (duration <= 0.f)
                continue;

            anyDuration = max(anyDuration, duration);
            if (emitter.typeData.kind == AuthoringTypeDataKind::Trail)
                trailDuration = max(trailDuration, duration);
        }

        constexpr float fallbackDuration = 1.f;
        return trailDuration > 0.f ? trailDuration : max(fallbackDuration, anyDuration);
    }

    float Resolve_TrailStrokeDrainDuration(const Shared<const EffectDefinition>& definition)
    {
        if (definition == nullptr)
            return 0.25f;

        float duration = 0.f;
        for (const EffectEmitterDefinition& emitter : definition->emitters)
        {
            if (!emitter.enabled || emitter.kind != EffectEmitterKind::Trail)
                continue;

            const auto* desc = get_if<ComputeTrailEmitterDesc>(&emitter.concreteDesc);
            if (desc == nullptr)
                continue;

            duration = max(duration, desc->segmentLifetime);
        }

        return duration > 0.f ? duration : 0.25f;
    }

    uint32 Resolve_RibbonAutoHistoryCount(
        uint32 authoredCount,
        float sampleLifetime,
        float sampleSpacing,
        float maxLength)
    {
        const uint32 authored = max(2u, authoredCount);
        const uint32 timeBased =
            static_cast<uint32>(ceilf(max(0.0001f, sampleLifetime) / kRibbonAutoHistoryFrameStep)) + kRibbonAutoHistoryMargin;
        uint32 required = max(authored, timeBased);

        if (maxLength > 0.f)
        {
            const uint32 distanceBased =
                static_cast<uint32>(ceilf(maxLength / max(0.001f, sampleSpacing))) + kRibbonAutoHistoryMargin;
            required = max(required, distanceBased);
        }

        return clamp(required, 2u, kRibbonAutoHistoryMaxCount);
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

    PointParticleRandomSeedRuntimeDesc Build_RandomSeedDesc(const DistributionRandomSeedData& data)
    {
        return PointParticleRandomSeedRuntimeDesc{
            .manualSeedEnabled = data.mode == DistributionRandomSeedMode::Manual,
            .seed = data.mode == DistributionRandomSeedMode::Manual ? data.manualSeed : 0u,
            .useInstanceSeed = data.useInstanceSeed
        };
    }

    PointParticleRandomSeedRuntimeDesc Build_RandomSeedDesc(const FloatDistributionData& data)
    {
        const auto* uniform = get_if<UniformFloatDistributionData>(&data.payload);
        return data.mode == DistributionMode::Uniform && uniform != nullptr ? Build_RandomSeedDesc(uniform->randomSeed) : PointParticleRandomSeedRuntimeDesc{};
    }

    PointParticleRandomSeedRuntimeDesc Build_RandomSeedDesc(const Vector2DistributionData& data)
    {
        const auto* uniform = get_if<UniformVector2DistributionData>(&data.payload);
        return data.mode == DistributionMode::Uniform && uniform != nullptr ? Build_RandomSeedDesc(uniform->randomSeed) : PointParticleRandomSeedRuntimeDesc{};
    }

    PointParticleRandomSeedRuntimeDesc Build_RandomSeedDesc(const Vector3DistributionData& data)
    {
        const auto* uniform = get_if<UniformVector3DistributionData>(&data.payload);
        return data.mode == DistributionMode::Uniform && uniform != nullptr ? Build_RandomSeedDesc(uniform->randomSeed) : PointParticleRandomSeedRuntimeDesc{};
    }

    PointParticleRandomSeedRuntimeDesc Build_RandomSeedDesc(const ColorDistributionData& data)
    {
        const auto* uniform = get_if<UniformColorDistributionData>(&data.payload);
        return data.mode == DistributionMode::Uniform && uniform != nullptr ? Build_RandomSeedDesc(uniform->randomSeed) : PointParticleRandomSeedRuntimeDesc{};
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

    EffectSortPolicy Resolve_EffectSortPolicy(EmitterSortPolicy value)
    {
        switch (value)
        {
        case EmitterSortPolicy::EmitterDepth:
            return EffectSortPolicy::EmitterDepth;
        case EmitterSortPolicy::None:
        default:
            return EffectSortPolicy::None;
        }
    }

    EffectSourceHistoryPreviewMotionPreset Resolve_SourceHistoryPreviewMotionPreset(SourceHistoryPreviewMotionPreset value)
    {
        switch (value)
        {
        case SourceHistoryPreviewMotionPreset::Line:
            return EffectSourceHistoryPreviewMotionPreset::Line;
        case SourceHistoryPreviewMotionPreset::Arc:
            return EffectSourceHistoryPreviewMotionPreset::Arc;
        case SourceHistoryPreviewMotionPreset::Loop:
            return EffectSourceHistoryPreviewMotionPreset::Loop;
        case SourceHistoryPreviewMotionPreset::Off:
        default:
            return EffectSourceHistoryPreviewMotionPreset::Off;
        }
    }

    EffectSourceHistoryPreviewMotionAxis Resolve_SourceHistoryPreviewMotionAxis(SourceHistoryPreviewMotionAxis value)
    {
        switch (value)
        {
        case SourceHistoryPreviewMotionAxis::Y:
            return EffectSourceHistoryPreviewMotionAxis::Y;
        case SourceHistoryPreviewMotionAxis::Z:
            return EffectSourceHistoryPreviewMotionAxis::Z;
        case SourceHistoryPreviewMotionAxis::X:
        default:
            return EffectSourceHistoryPreviewMotionAxis::X;
        }
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

    PointParticleSphereLocationPlacementMode Resolve_PointParticleSphereLocationPlacementMode(SphereLocationPlacementMode mode)
    {
        switch (mode)
        {
        case SphereLocationPlacementMode::EvenByParticleIndex:
            return PointParticleSphereLocationPlacementMode::EvenByParticleIndex;

        case SphereLocationPlacementMode::Random:
        default:
            return PointParticleSphereLocationPlacementMode::Random;
        }
    }

    PointParticlePlaneRadialLocationPlane Resolve_PointParticlePlaneRadialLocationPlane(PlaneRadialLocationPlane plane)
    {
        switch (plane)
        {
        case PlaneRadialLocationPlane::XZ:
            return PointParticlePlaneRadialLocationPlane::XZ;

        case PlaneRadialLocationPlane::YZ:
            return PointParticlePlaneRadialLocationPlane::YZ;

        case PlaneRadialLocationPlane::CameraFacing:
            return PointParticlePlaneRadialLocationPlane::CameraFacing;

        case PlaneRadialLocationPlane::XY:
        default:
            return PointParticlePlaneRadialLocationPlane::XY;
        }
    }

    PointParticlePlaneRadialLocationShape Resolve_PointParticlePlaneRadialLocationShape(PlaneRadialLocationShape shape)
    {
        switch (shape)
        {
        case PlaneRadialLocationShape::Rectangle:
            return PointParticlePlaneRadialLocationShape::Rectangle;

        case PlaneRadialLocationShape::Ring:
            return PointParticlePlaneRadialLocationShape::Ring;

        case PlaneRadialLocationShape::Arc:
            return PointParticlePlaneRadialLocationShape::Arc;

        case PlaneRadialLocationShape::Disc:
        default:
            return PointParticlePlaneRadialLocationShape::Disc;
        }
    }

    PointParticlePlaneRadialLocationPlacementMode Resolve_PointParticlePlaneRadialLocationPlacementMode(PlaneRadialLocationPlacementMode mode)
    {
        switch (mode)
        {
        case PlaneRadialLocationPlacementMode::EvenByParticleIndex:
            return PointParticlePlaneRadialLocationPlacementMode::EvenByParticleIndex;

        case PlaneRadialLocationPlacementMode::Random:
        default:
            return PointParticlePlaneRadialLocationPlacementMode::Random;
        }
    }

    PointParticleInitialRadialVelocityCenterDirectionMode Resolve_PointParticleInitialRadialVelocityCenterDirectionMode(
        InitialRadialVelocityCenterDirectionMode mode)
    {
        switch (mode)
        {
        case InitialRadialVelocityCenterDirectionMode::PlaneRadial:
            return PointParticleInitialRadialVelocityCenterDirectionMode::PlaneRadial;

        case InitialRadialVelocityCenterDirectionMode::RandomUpward:
        default:
            return PointParticleInitialRadialVelocityCenterDirectionMode::RandomUpward;
        }
    }

    PointParticlePlaneRadialOrientationTargetKind Resolve_PointParticlePlaneRadialOrientationTargetKind(PlaneRadialOrientationTargetKind targetKind)
    {
        switch (targetKind)
        {
        case PlaneRadialOrientationTargetKind::Sprite2D:
            return PointParticlePlaneRadialOrientationTargetKind::Sprite2D;

        case PlaneRadialOrientationTargetKind::Mesh3D:
            return PointParticlePlaneRadialOrientationTargetKind::Mesh3D;

        case PlaneRadialOrientationTargetKind::Auto:
        default:
            return PointParticlePlaneRadialOrientationTargetKind::Auto;
        }
    }

    PointParticlePlaneRadialOrientationMode Resolve_PointParticlePlaneRadialOrientationMode(PlaneRadialOrientationMode mode)
    {
        switch (mode)
        {
        case PlaneRadialOrientationMode::FaceRadialOut:
            return PointParticlePlaneRadialOrientationMode::FaceRadialOut;

        case PlaneRadialOrientationMode::FaceRadialIn:
            return PointParticlePlaneRadialOrientationMode::FaceRadialIn;

        case PlaneRadialOrientationMode::FaceTangentCW:
            return PointParticlePlaneRadialOrientationMode::FaceTangentCW;

        case PlaneRadialOrientationMode::FaceTangentCCW:
            return PointParticlePlaneRadialOrientationMode::FaceTangentCCW;

        case PlaneRadialOrientationMode::FacePlaneNormal:
            return PointParticlePlaneRadialOrientationMode::FacePlaneNormal;

        case PlaneRadialOrientationMode::None:
        default:
            return PointParticlePlaneRadialOrientationMode::None;
        }
    }

    PointParticleSphereRadialOrientationMode Resolve_PointParticleSphereRadialOrientationMode(SphereRadialOrientationMode mode)
    {
        switch (mode)
        {
        case SphereRadialOrientationMode::FaceRadialOut:
            return PointParticleSphereRadialOrientationMode::FaceRadialOut;

        case SphereRadialOrientationMode::FaceRadialIn:
            return PointParticleSphereRadialOrientationMode::FaceRadialIn;

        case SphereRadialOrientationMode::None:
        default:
            return PointParticleSphereRadialOrientationMode::None;
        }
    }

    PointParticlePlaneRadialOrientationAxis Resolve_PointParticlePlaneRadialOrientationAxis(PlaneRadialOrientationAxis axis)
    {
        switch (axis)
        {
        case PlaneRadialOrientationAxis::NegativeX:
            return PointParticlePlaneRadialOrientationAxis::NegativeX;

        case PlaneRadialOrientationAxis::PositiveY:
            return PointParticlePlaneRadialOrientationAxis::PositiveY;

        case PlaneRadialOrientationAxis::NegativeY:
            return PointParticlePlaneRadialOrientationAxis::NegativeY;

        case PlaneRadialOrientationAxis::PositiveZ:
            return PointParticlePlaneRadialOrientationAxis::PositiveZ;

        case PlaneRadialOrientationAxis::NegativeZ:
            return PointParticlePlaneRadialOrientationAxis::NegativeZ;

        case PlaneRadialOrientationAxis::PositiveX:
        default:
            return PointParticlePlaneRadialOrientationAxis::PositiveX;
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

    Vec2 Build_NormalizedFloatRange(const FloatDistributionData& data, Vec2 fallbackValue)
    {
        const float minValue = Evaluate_FloatDistributionMin(data, fallbackValue.x);
        const float maxValue = Evaluate_FloatDistributionMax(data, fallbackValue.y);
        return Vec2{ min(minValue, maxValue), max(minValue, maxValue) };
    }

    Vec2 Build_NonNegativeFloatRange(const FloatDistributionData& data, Vec2 fallbackValue)
    {
        const Vec2 range = Build_NormalizedFloatRange(data, fallbackValue);
        return Vec2{ max(0.f, range.x), max(0.f, range.y) };
    }

    PointParticlePlaneRadialLocationDesc Build_PointParticlePlaneRadialLocationDesc(const PlaneRadialLocationModuleData& data)
    {
        PointParticlePlaneRadialLocationDesc desc{};
        desc.enabled = true;
        desc.plane = Resolve_PointParticlePlaneRadialLocationPlane(data.plane);
        desc.shape = Resolve_PointParticlePlaneRadialLocationShape(data.shape);
        desc.placementMode = Resolve_PointParticlePlaneRadialLocationPlacementMode(data.placementMode);
        desc.offset = data.offset;
        desc.uRange = Build_NormalizedFloatRange(data.uDistribution, desc.uRange);
        desc.vRange = Build_NormalizedFloatRange(data.vDistribution, desc.vRange);
        desc.radiusRange = Build_NonNegativeFloatRange(data.radiusDistribution, desc.radiusRange);
        desc.angleDegreesRange = Build_NormalizedFloatRange(data.angleDegreesDistribution, desc.angleDegreesRange);
        desc.uSeed = Build_RandomSeedDesc(data.uDistribution);
        desc.vSeed = Build_RandomSeedDesc(data.vDistribution);
        desc.radiusSeed = Build_RandomSeedDesc(data.radiusDistribution);
        desc.angleDegreesSeed = Build_RandomSeedDesc(data.angleDegreesDistribution);
        desc.thickness = max(0.f, data.thickness);
        return desc;
    }

    PointParticlePlaneRadialOrientationDesc Build_PointParticlePlaneRadialOrientationDesc(const PlaneRadialOrientationModuleData& data)
    {
        PointParticlePlaneRadialOrientationDesc desc{};
        desc.enabled = true;
        desc.targetKind = Resolve_PointParticlePlaneRadialOrientationTargetKind(data.targetKind);
        desc.orientationMode = Resolve_PointParticlePlaneRadialOrientationMode(data.orientationMode);
        desc.meshForwardAxis = Resolve_PointParticlePlaneRadialOrientationAxis(data.meshForwardAxis);
        desc.meshUpAxis = Resolve_PointParticlePlaneRadialOrientationAxis(data.meshUpAxis);
        desc.tiltDegrees = data.tiltDegrees;
        desc.rollOffsetDegrees = data.rollOffsetDegrees;
        return desc;
    }

    PointParticleSphereRadialOrientationDesc Build_PointParticleSphereRadialOrientationDesc(const SphereRadialOrientationModuleData& data)
    {
        PointParticleSphereRadialOrientationDesc desc{};
        desc.enabled = true;
        desc.orientationMode = Resolve_PointParticleSphereRadialOrientationMode(data.orientationMode);
        desc.meshForwardAxis = Resolve_PointParticlePlaneRadialOrientationAxis(data.meshForwardAxis);
        desc.meshUpAxis = Resolve_PointParticlePlaneRadialOrientationAxis(data.meshUpAxis);
        desc.tiltDegrees = data.tiltDegrees;
        desc.rollOffsetDegrees = data.rollOffsetDegrees;
        return desc;
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

    static void Set_CurveComponent(Vec4& valuesBlock0, Vec4& valuesBlock1, uint32 index, float value)
    {
        if (index < 4u)
        {
            Set_SubUVFrameCurveComponent(valuesBlock0, index, value);
            return;
        }

        Set_SubUVFrameCurveComponent(valuesBlock1, index - 4u, value);
    }

    void Fill_SizeByLifeCurveComponent(PointParticleSizeByLifeDesc& desc, uint32 index, float time, const Vec2& value)
    {
        Set_CurveComponent(desc.curveKeyTimes, desc.curveKeyTimesBlock1, index, clamp(time, 0.f, 1.f));
        Set_CurveComponent(desc.curveKeyValuesX, desc.curveKeyValuesXBlock1, index, value.x);
        Set_CurveComponent(desc.curveKeyValuesY, desc.curveKeyValuesYBlock1, index, value.y);
    }

    void Fill_RotationOverLifeCurveComponent(PointParticleRotationDesc& desc, uint32 index, float time, float value)
    {
        Set_CurveComponent(desc.rotationOverLifeCurveTimes, desc.rotationOverLifeCurveTimesBlock1, index, clamp(time, 0.f, 1.f));
        Set_CurveComponent(desc.rotationOverLifeCurveValues, desc.rotationOverLifeCurveValuesBlock1, index, value);
    }

    void Fill_RotationRateScaleByLifeCurveComponent(PointParticleRotationDesc& desc, uint32 index, float time, float value)
    {
        Set_CurveComponent(desc.rotationRateScaleByLifeCurveTimes, desc.rotationRateScaleByLifeCurveTimesBlock1, index, clamp(time, 0.f, 1.f));
        Set_CurveComponent(desc.rotationRateScaleByLifeCurveValues, desc.rotationRateScaleByLifeCurveValuesBlock1, index, value);
    }

    void Fill_VelocityScaleByLifeCurveComponent(PointParticleMotionDesc& desc, uint32 index, float time, float value)
    {
        Set_CurveComponent(desc.velocityScaleByLifeCurveTimes, desc.velocityScaleByLifeCurveTimesBlock1, index, clamp(time, 0.f, 1.f));
        Set_CurveComponent(desc.velocityScaleByLifeCurveValues, desc.velocityScaleByLifeCurveValuesBlock1, index, value);
    }

    void Fill_OrbitFloatCurveComponent(Vec4& times, Vec4& timesBlock1, Vec4& values, Vec4& valuesBlock1, uint32 index, float time, float value)
    {
        Set_CurveComponent(times, timesBlock1, index, clamp(time, 0.f, 1.f));
        Set_CurveComponent(values, valuesBlock1, index, value);
    }

    void Fill_MeshVector3CurveComponent(MeshVector3CurveRuntimeDesc& desc, uint32 index, float time, const Vec3& value)
    {
        Set_CurveComponent(desc.curveKeyTimes, desc.curveKeyTimesBlock1, index, clamp(time, 0.f, 1.f));
        Set_CurveComponent(desc.curveKeyValuesX, desc.curveKeyValuesXBlock1, index, value.x);
        Set_CurveComponent(desc.curveKeyValuesY, desc.curveKeyValuesYBlock1, index, value.y);
        Set_CurveComponent(desc.curveKeyValuesZ, desc.curveKeyValuesZBlock1, index, value.z);
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

    void Fill_FloatCurvePayload(PointParticleFloatCurveRuntimeDesc& desc, const FloatDistributionData& distribution, float fallbackValue)
    {
        desc.enabled = false;
        desc.curveKeyCount = 2u;
        desc.curveKeyTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
        desc.curveKeyTimesBlock1 = Vec4{};
        desc.curveKeyValues = Vec4{ fallbackValue, fallbackValue, 0.f, 0.f };
        desc.curveKeyValuesBlock1 = Vec4{};
        desc.curveKeyArriveTangents = Vec4{};
        desc.curveKeyArriveTangentsBlock1 = Vec4{};
        desc.curveKeyLeaveTangents = Vec4{};
        desc.curveKeyLeaveTangentsBlock1 = Vec4{};
        desc.curveKeyModes = Vec4{ 1.f, 1.f, 0.f, 0.f };
        desc.curveKeyModesBlock1 = Vec4{};

        const auto* curve = get_if<ConstantCurveFloatDistributionData>(&distribution.payload);
        if (curve == nullptr)
            return;

        vector<FloatCurveKeyData> keys = curve->keys;
        ranges::sort(keys, {}, &FloatCurveKeyData::time);
        const uint32 keyCount = min(kEffectDistributionCurveMaxKeys, static_cast<uint32>(keys.size()));
        if (keyCount == 0u)
            return;

        desc.enabled = true;
        desc.curveKeyCount = keyCount;
        desc.curveKeyTimes = Vec4{};
        desc.curveKeyTimesBlock1 = Vec4{};
        desc.curveKeyValues = Vec4{};
        desc.curveKeyValuesBlock1 = Vec4{};
        desc.curveKeyArriveTangents = Vec4{};
        desc.curveKeyArriveTangentsBlock1 = Vec4{};
        desc.curveKeyLeaveTangents = Vec4{};
        desc.curveKeyLeaveTangentsBlock1 = Vec4{};
        desc.curveKeyModes = Vec4{};
        desc.curveKeyModesBlock1 = Vec4{};
        for (uint32 index = 0; index < keyCount; ++index)
        {
            const FloatCurveKeyData& key = keys[index];
            Set_CurveComponent(desc.curveKeyTimes, desc.curveKeyTimesBlock1, index, clamp(key.time, 0.f, 1.f));
            Set_CurveComponent(desc.curveKeyValues, desc.curveKeyValuesBlock1, index, key.value);
            Set_CurveComponent(desc.curveKeyArriveTangents, desc.curveKeyArriveTangentsBlock1, index, key.arriveTangent);
            Set_CurveComponent(desc.curveKeyLeaveTangents, desc.curveKeyLeaveTangentsBlock1, index, key.leaveTangent);
            Set_CurveComponent(desc.curveKeyModes, desc.curveKeyModesBlock1, index, To_RuntimeCurveInterpolationMode(key.interpolationMode));
        }
    }

    void Fill_ColorOverLifeColorCurveComponent(
        PointParticleColorOverLifeCurveDesc& desc,
        uint32 index,
        const ColorCurveKeyData& key)
    {
        Set_CurveComponent(desc.colorCurveTimes, desc.colorCurveTimesBlock1, index, clamp(key.time, 0.f, 1.f));
        Set_CurveComponent(desc.colorCurveValuesR, desc.colorCurveValuesRBlock1, index, key.value.R());
        Set_CurveComponent(desc.colorCurveValuesG, desc.colorCurveValuesGBlock1, index, key.value.G());
        Set_CurveComponent(desc.colorCurveValuesB, desc.colorCurveValuesBBlock1, index, key.value.B());
        Set_CurveComponent(desc.colorCurveArriveR, desc.colorCurveArriveRBlock1, index, key.arriveTangent.R());
        Set_CurveComponent(desc.colorCurveArriveG, desc.colorCurveArriveGBlock1, index, key.arriveTangent.G());
        Set_CurveComponent(desc.colorCurveArriveB, desc.colorCurveArriveBBlock1, index, key.arriveTangent.B());
        Set_CurveComponent(desc.colorCurveLeaveR, desc.colorCurveLeaveRBlock1, index, key.leaveTangent.R());
        Set_CurveComponent(desc.colorCurveLeaveG, desc.colorCurveLeaveGBlock1, index, key.leaveTangent.G());
        Set_CurveComponent(desc.colorCurveLeaveB, desc.colorCurveLeaveBBlock1, index, key.leaveTangent.B());
        Set_CurveComponent(desc.colorCurveModes, desc.colorCurveModesBlock1, index, To_RuntimeCurveInterpolationMode(key.interpolationMode));
    }

    void Fill_ColorOverLifeAlphaCurveComponent(
        PointParticleColorOverLifeCurveDesc& desc,
        uint32 index,
        const FloatCurveKeyData& key)
    {
        Set_CurveComponent(desc.alphaCurveTimes, desc.alphaCurveTimesBlock1, index, clamp(key.time, 0.f, 1.f));
        Set_CurveComponent(desc.alphaCurveValues, desc.alphaCurveValuesBlock1, index, key.value);
        Set_CurveComponent(desc.alphaCurveArrive, desc.alphaCurveArriveBlock1, index, key.arriveTangent);
        Set_CurveComponent(desc.alphaCurveLeave, desc.alphaCurveLeaveBlock1, index, key.leaveTangent);
        Set_CurveComponent(desc.alphaCurveModes, desc.alphaCurveModesBlock1, index, To_RuntimeCurveInterpolationMode(key.interpolationMode));
    }

    void Fill_ColorOverLifeCurvePayload(
        PointParticleColorOverLifeCurveDesc& desc,
        const ColorOverLifeModuleData& data)
    {
        constexpr uint32 maxColorOverLifeCurveKeys{ kEffectDistributionCurveMaxKeys };

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
                desc.colorCurveTimesBlock1 = Vec4{};
                desc.colorCurveValuesR = Vec4{};
                desc.colorCurveValuesRBlock1 = Vec4{};
                desc.colorCurveValuesG = Vec4{};
                desc.colorCurveValuesGBlock1 = Vec4{};
                desc.colorCurveValuesB = Vec4{};
                desc.colorCurveValuesBBlock1 = Vec4{};
                desc.colorCurveArriveR = Vec4{};
                desc.colorCurveArriveRBlock1 = Vec4{};
                desc.colorCurveArriveG = Vec4{};
                desc.colorCurveArriveGBlock1 = Vec4{};
                desc.colorCurveArriveB = Vec4{};
                desc.colorCurveArriveBBlock1 = Vec4{};
                desc.colorCurveLeaveR = Vec4{};
                desc.colorCurveLeaveRBlock1 = Vec4{};
                desc.colorCurveLeaveG = Vec4{};
                desc.colorCurveLeaveGBlock1 = Vec4{};
                desc.colorCurveLeaveB = Vec4{};
                desc.colorCurveLeaveBBlock1 = Vec4{};
                desc.colorCurveModes = Vec4{};
                desc.colorCurveModesBlock1 = Vec4{};
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
                desc.alphaCurveTimesBlock1 = Vec4{};
                desc.alphaCurveValues = Vec4{};
                desc.alphaCurveValuesBlock1 = Vec4{};
                desc.alphaCurveArrive = Vec4{};
                desc.alphaCurveArriveBlock1 = Vec4{};
                desc.alphaCurveLeave = Vec4{};
                desc.alphaCurveLeaveBlock1 = Vec4{};
                desc.alphaCurveModes = Vec4{};
                desc.alphaCurveModesBlock1 = Vec4{};
                for (uint32 index = 0; index < keyCount; ++index)
                    Fill_ColorOverLifeAlphaCurveComponent(desc, index, keys[index]);
            }
        }
    }

    void Fill_SubUVFrameCurvePayload(PointParticleSubUVFrameOverLifeDesc& desc, const FloatDistributionData& frameIndex)
    {
        desc.frameCurveKeyCount = 1;
        desc.frameCurveTimes = Vec4{};
        desc.frameCurveTimesBlock1 = Vec4{};
        desc.frameCurveValues = Vec4{};
        desc.frameCurveValuesBlock1 = Vec4{};

        if (const auto* constant = get_if<ConstantFloatDistributionData>(&frameIndex.payload))
        {
            desc.frameCurveValues.x = constant->value;
            return;
        }

        const auto* curve = get_if<ConstantCurveFloatDistributionData>(&frameIndex.payload);
        if (nullptr == curve || curve->keys.empty())
            return;

        constexpr uint32 maxSubUVFrameCurveKeys{ kEffectDistributionCurveMaxKeys };
        const uint32 keyCount = min(maxSubUVFrameCurveKeys, static_cast<uint32>(curve->keys.size()));
        desc.frameCurveKeyCount = keyCount;

        for (uint32 index = 0; index < keyCount; ++index)
        {
            const FloatCurveKeyData& key = curve->keys[index];
            Set_CurveComponent(desc.frameCurveTimes, desc.frameCurveTimesBlock1, index, clamp(key.time, 0.f, 1.f));
            Set_CurveComponent(desc.frameCurveValues, desc.frameCurveValuesBlock1, index, key.value);
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
        desc.curveKeyTimesBlock1 = Vec4{};
        desc.curveKeyValuesX = Vec4{ startValue.x, endValue.x, 0.f, 0.f };
        desc.curveKeyValuesXBlock1 = Vec4{};
        desc.curveKeyValuesY = Vec4{ startValue.y, endValue.y, 0.f, 0.f };
        desc.curveKeyValuesYBlock1 = Vec4{};

        if (const auto* curve = get_if<ConstantCurveVector2DistributionData>(&data.scaleOverLife.payload))
        {
            vector<Vector2CurveKeyData> keys = curve->keys;
            ranges::sort(keys, {}, &Vector2CurveKeyData::time);
            constexpr uint32 maxSizeByLifeCurveKeys{ kEffectDistributionCurveMaxKeys };
            const uint32 keyCount = min(maxSizeByLifeCurveKeys, static_cast<uint32>(keys.size()));
            if (keyCount == 0)
                return;

            desc.curveKeyCount = keyCount;
            desc.curveKeyTimes = Vec4{};
            desc.curveKeyTimesBlock1 = Vec4{};
            desc.curveKeyValuesX = Vec4{};
            desc.curveKeyValuesXBlock1 = Vec4{};
            desc.curveKeyValuesY = Vec4{};
            desc.curveKeyValuesYBlock1 = Vec4{};
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
        desc.rotationOverLifeCurveTimesBlock1 = Vec4{};
        desc.rotationOverLifeCurveValues = Vec4{ startValue, endValue, 0.f, 0.f };
        desc.rotationOverLifeCurveValuesBlock1 = Vec4{};

        if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&data.rotationOverLife.payload))
        {
            vector<FloatCurveKeyData> keys = curve->keys;
            ranges::sort(keys, {}, &FloatCurveKeyData::time);
            constexpr uint32 maxRotationOverLifeCurveKeys{ kEffectDistributionCurveMaxKeys };
            const uint32 keyCount = min(maxRotationOverLifeCurveKeys, static_cast<uint32>(keys.size()));
            if (keyCount == 0)
                return;

            desc.rotationOverLifeCurveKeyCount = keyCount;
            desc.rotationOverLifeCurveTimes = Vec4{};
            desc.rotationOverLifeCurveTimesBlock1 = Vec4{};
            desc.rotationOverLifeCurveValues = Vec4{};
            desc.rotationOverLifeCurveValuesBlock1 = Vec4{};
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
        desc.rotationRateScaleByLifeCurveTimesBlock1 = Vec4{};
        desc.rotationRateScaleByLifeCurveValues = Vec4{ startValue, endValue, 0.f, 0.f };
        desc.rotationRateScaleByLifeCurveValuesBlock1 = Vec4{};

        if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&data.scaleOverLife.payload))
        {
            vector<FloatCurveKeyData> keys = curve->keys;
            ranges::sort(keys, {}, &FloatCurveKeyData::time);
            constexpr uint32 maxRotationRateScaleByLifeCurveKeys{ kEffectDistributionCurveMaxKeys };
            const uint32 keyCount = min(maxRotationRateScaleByLifeCurveKeys, static_cast<uint32>(keys.size()));
            if (keyCount == 0)
                return;

            desc.rotationRateScaleByLifeCurveKeyCount = keyCount;
            desc.rotationRateScaleByLifeCurveTimes = Vec4{};
            desc.rotationRateScaleByLifeCurveTimesBlock1 = Vec4{};
            desc.rotationRateScaleByLifeCurveValues = Vec4{};
            desc.rotationRateScaleByLifeCurveValuesBlock1 = Vec4{};
            for (uint32 index = 0; index < keyCount; ++index)
                Fill_RotationRateScaleByLifeCurveComponent(desc, index, keys[index].time, keys[index].value);
        }
    }

    Engine::EffectOrbitPlane Resolve_EffectOrbitPlane(EffectOrbitPlane plane)
    {
        switch (plane)
        {
        case EffectOrbitPlane::XZ:
            return Engine::EffectOrbitPlane::XZ;
        case EffectOrbitPlane::YZ:
            return Engine::EffectOrbitPlane::YZ;
        case EffectOrbitPlane::XY:
        default:
            return Engine::EffectOrbitPlane::XY;
        }
    }

    void Fill_OrbitFloatCurvePayload(
        Vec2& range,
        uint32& keyCount,
        Vec4& times,
        Vec4& timesBlock1,
        Vec4& values,
        Vec4& valuesBlock1,
        const FloatDistributionData& distribution,
        float fallbackValue)
    {
        const float startValue = Evaluate_FloatDistributionMin(distribution, fallbackValue);
        const float endValue = Evaluate_FloatDistributionMax(distribution, fallbackValue);
        range = Vec2{ startValue, endValue };
        keyCount = 2;
        times = Vec4{ 0.f, 1.f, 0.f, 0.f };
        timesBlock1 = Vec4{};
        values = Vec4{ startValue, endValue, 0.f, 0.f };
        valuesBlock1 = Vec4{};

        if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&distribution.payload))
        {
            vector<FloatCurveKeyData> keys = curve->keys;
            ranges::sort(keys, {}, &FloatCurveKeyData::time);
            constexpr uint32 maxOrbitCurveKeys{ kEffectDistributionCurveMaxKeys };
            const uint32 clampedKeyCount = min(maxOrbitCurveKeys, static_cast<uint32>(keys.size()));
            if (clampedKeyCount == 0)
                return;

            keyCount = clampedKeyCount;
            times = Vec4{};
            timesBlock1 = Vec4{};
            values = Vec4{};
            valuesBlock1 = Vec4{};
            for (uint32 index = 0; index < clampedKeyCount; ++index)
                Fill_OrbitFloatCurveComponent(times, timesBlock1, values, valuesBlock1, index, keys[index].time, keys[index].value);
        }
    }

    EffectOrbitOverLifeRuntimeDesc Build_OrbitOverLifeDesc(const OrbitOverLifeModuleData& data)
    {
        EffectOrbitOverLifeRuntimeDesc desc{};
        desc.enabled = true;
        desc.plane = Resolve_EffectOrbitPlane(data.plane);
        Fill_OrbitFloatCurvePayload(
            desc.angleDegreesOverLife,
            desc.angleCurveKeyCount,
            desc.angleCurveTimes,
            desc.angleCurveTimesBlock1,
            desc.angleCurveValues,
            desc.angleCurveValuesBlock1,
            data.angleDegreesOverLife,
            0.f
        );
        Fill_OrbitFloatCurvePayload(
            desc.radiusScaleOverLife,
            desc.radiusScaleCurveKeyCount,
            desc.radiusScaleCurveTimes,
            desc.radiusScaleCurveTimesBlock1,
            desc.radiusScaleCurveValues,
            desc.radiusScaleCurveValuesBlock1,
            data.radiusScaleOverLife,
            1.f
        );
        return desc;
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
        desc.curveKeyTimesBlock1 = Vec4{};
        desc.curveKeyValuesX = Vec4{ startValue.x, endValue.x, 0.f, 0.f };
        desc.curveKeyValuesXBlock1 = Vec4{};
        desc.curveKeyValuesY = Vec4{ startValue.y, endValue.y, 0.f, 0.f };
        desc.curveKeyValuesYBlock1 = Vec4{};
        desc.curveKeyValuesZ = Vec4{ startValue.z, endValue.z, 0.f, 0.f };
        desc.curveKeyValuesZBlock1 = Vec4{};

        if (const auto* curve = get_if<ConstantCurveVector3DistributionData>(&data.payload))
        {
            vector<Vector3CurveKeyData> keys = curve->keys;
            ranges::sort(keys, {}, &Vector3CurveKeyData::time);
            constexpr uint32 maxMeshCurveKeys{ kEffectDistributionCurveMaxKeys };
            const uint32 keyCount = min(maxMeshCurveKeys, static_cast<uint32>(keys.size()));
            if (keyCount == 0)
                return;

            desc.curveKeyCount = keyCount;
            desc.curveKeyTimes = Vec4{};
            desc.curveKeyTimesBlock1 = Vec4{};
            desc.curveKeyValuesX = Vec4{};
            desc.curveKeyValuesXBlock1 = Vec4{};
            desc.curveKeyValuesY = Vec4{};
            desc.curveKeyValuesYBlock1 = Vec4{};
            desc.curveKeyValuesZ = Vec4{};
            desc.curveKeyValuesZBlock1 = Vec4{};
            for (uint32 index = 0; index < keyCount; ++index)
                Fill_MeshVector3CurveComponent(desc, index, keys[index].time, keys[index].value);
        }
    }

    void Fill_AccelerationCurvePayload(PointParticleMotionDesc& desc, const Vector3DistributionData& data)
    {
        desc.accelerationCurveEnabled = false;
        desc.accelerationCurveKeyCount = 2;
        desc.accelerationCurveTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
        desc.accelerationCurveTimesBlock1 = Vec4{};
        desc.accelerationCurveValuesX = Vec4{ desc.accelerationMin.x, desc.accelerationMax.x, 0.f, 0.f };
        desc.accelerationCurveValuesXBlock1 = Vec4{};
        desc.accelerationCurveValuesY = Vec4{ desc.accelerationMin.y, desc.accelerationMax.y, 0.f, 0.f };
        desc.accelerationCurveValuesYBlock1 = Vec4{};
        desc.accelerationCurveValuesZ = Vec4{ desc.accelerationMin.z, desc.accelerationMax.z, 0.f, 0.f };
        desc.accelerationCurveValuesZBlock1 = Vec4{};

        const auto* curve = get_if<ConstantCurveVector3DistributionData>(&data.payload);
        if (curve == nullptr)
            return;

        vector<Vector3CurveKeyData> keys = curve->keys;
        ranges::sort(keys, {}, &Vector3CurveKeyData::time);
        constexpr uint32 maxAccelerationCurveKeys{ kEffectDistributionCurveMaxKeys };
        const uint32 keyCount = min(maxAccelerationCurveKeys, static_cast<uint32>(keys.size()));
        if (keyCount == 0)
            return;

        desc.accelerationCurveEnabled = true;
        desc.accelerationCurveKeyCount = keyCount;
        desc.accelerationCurveTimes = Vec4{};
        desc.accelerationCurveTimesBlock1 = Vec4{};
        desc.accelerationCurveValuesX = Vec4{};
        desc.accelerationCurveValuesXBlock1 = Vec4{};
        desc.accelerationCurveValuesY = Vec4{};
        desc.accelerationCurveValuesYBlock1 = Vec4{};
        desc.accelerationCurveValuesZ = Vec4{};
        desc.accelerationCurveValuesZBlock1 = Vec4{};
        for (uint32 index = 0; index < keyCount; ++index)
        {
            Set_CurveComponent(desc.accelerationCurveTimes, desc.accelerationCurveTimesBlock1, index, clamp(keys[index].time, 0.f, 1.f));
            Set_CurveComponent(desc.accelerationCurveValuesX, desc.accelerationCurveValuesXBlock1, index, keys[index].value.x);
            Set_CurveComponent(desc.accelerationCurveValuesY, desc.accelerationCurveValuesYBlock1, index, keys[index].value.y);
            Set_CurveComponent(desc.accelerationCurveValuesZ, desc.accelerationCurveValuesZBlock1, index, keys[index].value.z);
        }
    }

    void Fill_VelocityScaleByLifePayload(PointParticleMotionDesc& desc, const FloatDistributionData& data)
    {
        const float startValue = Evaluate_FloatDistributionMin(data, desc.velocityScaleByLife.x);
        const float endValue = Evaluate_FloatDistributionMax(data, desc.velocityScaleByLife.y);
        desc.velocityScaleByLife = Vec2{ startValue, endValue };
        desc.velocityScaleByLifeCurveKeyCount = 2;
        desc.velocityScaleByLifeCurveTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
        desc.velocityScaleByLifeCurveTimesBlock1 = Vec4{};
        desc.velocityScaleByLifeCurveValues = Vec4{ startValue, endValue, 0.f, 0.f };
        desc.velocityScaleByLifeCurveValuesBlock1 = Vec4{};

        if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&data.payload))
        {
            vector<FloatCurveKeyData> keys = curve->keys;
            ranges::sort(keys, {}, &FloatCurveKeyData::time);
            constexpr uint32 maxVelocityScaleByLifeCurveKeys{ kEffectDistributionCurveMaxKeys };
            const uint32 keyCount = min(maxVelocityScaleByLifeCurveKeys, static_cast<uint32>(keys.size()));
            if (keyCount == 0)
                return;

            desc.velocityScaleByLifeCurveKeyCount = keyCount;
            desc.velocityScaleByLifeCurveTimes = Vec4{};
            desc.velocityScaleByLifeCurveTimesBlock1 = Vec4{};
            desc.velocityScaleByLifeCurveValues = Vec4{};
            desc.velocityScaleByLifeCurveValuesBlock1 = Vec4{};
            for (uint32 index = 0; index < keyCount; ++index)
                Fill_VelocityScaleByLifeCurveComponent(desc, index, keys[index].time, keys[index].value);
        }
    }

    void Fill_VelocityConePayload(PointParticleMotionDesc& desc, const VelocityConeModuleData& data)
    {
        desc.enabled = true;
        desc.velocityConeEnabled = true;
        desc.velocityConeInWorldSpace = data.inWorldSpace;
        desc.velocityConeAxis = data.axis;
        desc.velocityConeAngleDegrees = clamp(data.angleDegrees, 0.f, 180.f);
        desc.velocityConeSpeed = Vec2{
            Evaluate_FloatDistributionMin(data.speed, desc.velocityConeSpeed.x),
            Evaluate_FloatDistributionMax(data.speed, desc.velocityConeSpeed.y)
        };
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
    Update_PreviewTrailStrokes(timeDelta);

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

    Clear_PreviewTrailStrokes();

    const bool splitTrailPreview =
        EDITOR->Get_TrailPreviewGateMode() == TrailPreviewGateMode::Window;
    const Shared<const EffectDefinition> previewDefinition =
        Build_PreviewEffectDefinition(emitterView->Get_Emitters(), !splitTrailPreview);
    CHECK_NULL(previewDefinition, E_FAIL);
    const Shared<const EffectDefinition> previewTrailDefinition =
        splitTrailPreview ? Build_PreviewTrailEffectDefinition(emitterView->Get_Emitters()) : nullptr;

    const EffectInstancePool* effectPool = CLIENT ? CLIENT->Get_EffectInstancePool() : nullptr;
    CHECK_NULL(effectPool, E_FAIL);

    CHECK_FAILED(Restart_PreviewFromDefinition(previewDefinition), E_FAIL);
    _currentPreviewDefinition = previewDefinition;
    _currentPreviewTrailDefinition = previewTrailDefinition;
    _wasTrailPreviewGateOpen = false;
    _lastTrailPreviewSourceRatio = 0.f;
    _hasTrailPreviewSourceRatio = false;
    EDITOR->Set_TrailPreviewGateDuration(Internal::Resolve_TrailPreviewGateDuration(emitterView->Get_Emitters()));
    EDITOR->Reset_TrailPreviewGate();

    for (const AuthoringEmitter& emitter : emitterView->Get_Emitters())
        emitterView->Clear_PreviewDirty(emitter.id);

    return S_OK;
}

HRESULT Level_EffectEditor::Reset_PreviewRuntime()
{
    if (_currentPreviewDefinition == nullptr)
        return S_OK;

    Clear_PreviewTrailStrokes();
    _wasTrailPreviewGateOpen = false;
    _lastTrailPreviewSourceRatio = 0.f;
    _hasTrailPreviewSourceRatio = false;
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

Shared<const EffectDefinition> Level_EffectEditor::Build_PreviewEffectDefinition(
    const vector<AuthoringEmitter>& emitters,
    bool includeTrail) const
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
            if (!includeTrail)
                break;

            definition->emitters.push_back(Build_PreviewTrailEmitterDefinition(emitter));
            break;

        case AuthoringTypeDataKind::Mesh:
            definition->emitters.push_back(Build_PreviewMeshEmitterDefinition(emitter));
            break;

        case AuthoringTypeDataKind::Ribbon:
            definition->emitters.push_back(Build_PreviewSourceHistoryRibbonEmitterDefinition(emitter));
            break;

        case AuthoringTypeDataKind::SourceHistorySpriteTrail:
            definition->emitters.push_back(Build_PreviewSourceHistorySpriteTrailEmitterDefinition(emitter));
            break;

        case AuthoringTypeDataKind::Beam:
            definition->emitters.push_back(Build_PreviewBeamEmitterDefinition(emitter));
            break;

        case AuthoringTypeDataKind::None:
        default:
            definition->emitters.push_back(Build_PreviewEmitterDefinition(emitter));
            break;
        }
    }

    return definition;
}

Shared<const EffectDefinition> Level_EffectEditor::Build_PreviewTrailEffectDefinition(const vector<AuthoringEmitter>& emitters) const
{
    auto definition = make_shared<EffectDefinition>();
    definition->name = "EffectEditorPreviewTrailStroke";

    for (const AuthoringEmitter& emitter : emitters)
    {
        if (!emitter.enabled || emitter.typeData.kind != AuthoringTypeDataKind::Trail)
            continue;

        definition->emitters.push_back(Build_PreviewTrailEmitterDefinition(emitter));
    }

    return definition->emitters.empty() ? nullptr : definition;
}

EffectEmitterDefinition Level_EffectEditor::Build_PreviewEmitterDefinition(const AuthoringEmitter& emitter) const
{
    EffectEmitterDefinition definition{};
    definition.id = emitter.id;
    definition.name = emitter.name;
    definition.kind = EffectEmitterKind::Sprite;
    definition.enabled = emitter.enabled;
    definition.renderLayerOverride = Internal::Resolve_EffectRenderLayerOverride(emitter.renderLayerOverride);

    ComputeSpriteEmitterDesc desc{};
    desc.drawMode = PointParticleDrawMode::DrawIndexedInstancedIndirect;
    desc.renderLayerOverride = definition.renderLayerOverride;

    if (const auto* required = Internal::Find_ModuleData<RequiredModuleData>(emitter, AuthoringModuleType::Required))
    {
        EffectRuntime::Copy_EffectMaterialInstanceToRuntimeDesc(required->material, desc.required.material);

        desc.required.sort.sortPolicy = Internal::Resolve_EffectSortPolicy(required->sortPolicy);
        desc.required.sort.sortLayer = required->sortLayer;
        desc.required.sort.artistSortBias = required->sortBias;

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
        desc.required.material.scalarModulation =
            Build_MaterialScalarModulationRuntimeDesc(*materialScalarModulation);
        desc.required.material.coreColorRgbModulation =
            Build_CoreColorRgbModulationRuntimeDesc(*materialScalarModulation);
    }

    if (const auto* spawn = Internal::Find_ModuleData<SpawnModuleData>(emitter, AuthoringModuleType::Spawn))
    {
        desc.spawn.instanceCount = max(1u, spawn->maxParticleCount);
        desc.spawn.particleSpawn.processSpawnRate = spawn->processSpawnRate;
        desc.spawn.particleSpawn.spawnRateRange = Vec2{
            max(0.f, Internal::Evaluate_FloatDistributionMin(spawn->spawnRate, desc.spawn.particleSpawn.spawnRate)),
            max(0.f, Internal::Evaluate_FloatDistributionMax(spawn->spawnRate, desc.spawn.particleSpawn.spawnRate))
        };
        desc.spawn.particleSpawn.spawnRate = desc.spawn.particleSpawn.spawnRateRange.y;
        desc.spawn.particleSpawn.spawnRateSeed = Internal::Build_RandomSeedDesc(spawn->spawnRate);
        desc.spawn.particleSpawn.spawnRateScaleRange = Vec2{
            max(0.f, Internal::Evaluate_FloatDistributionMin(spawn->spawnRateScale, desc.spawn.particleSpawn.spawnRateScale)),
            max(0.f, Internal::Evaluate_FloatDistributionMax(spawn->spawnRateScale, desc.spawn.particleSpawn.spawnRateScale))
        };
        desc.spawn.particleSpawn.spawnRateScale = desc.spawn.particleSpawn.spawnRateScaleRange.y;
        desc.spawn.particleSpawn.spawnRateScaleSeed = Internal::Build_RandomSeedDesc(spawn->spawnRateScale);
        desc.spawn.particleSpawn.processBurstList = spawn->processBurstList;
        desc.spawn.particleSpawn.burstScaleRange = Vec2{
            max(0.f, Internal::Evaluate_FloatDistributionMin(spawn->burstScale, desc.spawn.particleSpawn.burstScale)),
            max(0.f, Internal::Evaluate_FloatDistributionMax(spawn->burstScale, desc.spawn.particleSpawn.burstScale))
        };
        desc.spawn.particleSpawn.burstScale = desc.spawn.particleSpawn.burstScaleRange.y;
        desc.spawn.particleSpawn.burstScaleSeed = Internal::Build_RandomSeedDesc(spawn->burstScale);
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
        desc.initialSize.sizeSeed = Internal::Build_RandomSeedDesc(initialSize->size);
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
        desc.initialLocationSeed = Internal::Build_RandomSeedDesc(initialLocation->location);
    }

    if (const auto* sphereLocation = Internal::Find_ModuleData<SphereLocationModuleData>(emitter, AuthoringModuleType::SphereLocation))
    {
        desc.sphereLocation.enabled = true;
        desc.sphereLocation.offset = sphereLocation->offset;
        desc.sphereLocation.radius = max(0.f, sphereLocation->radius);
        desc.sphereLocation.mode = Internal::Resolve_PointParticleSphereLocationMode(sphereLocation->spawnMode);
        desc.sphereLocation.placementMode = Internal::Resolve_PointParticleSphereLocationPlacementMode(sphereLocation->placementMode);
        desc.sphereLocationSeed = Internal::Build_RandomSeedDesc(sphereLocation->randomSeed);
    }

    if (const auto* planeRadialLocation = Internal::Find_ModuleData<PlaneRadialLocationModuleData>(emitter, AuthoringModuleType::PlaneRadialLocation))
        desc.planeRadialLocation = Internal::Build_PointParticlePlaneRadialLocationDesc(*planeRadialLocation);

    if (const auto* lifetime = Internal::Find_ModuleData<LifetimeModuleData>(emitter, AuthoringModuleType::Lifetime))
    {
        desc.lifetime.lifeTime = Vec2{
            Internal::Evaluate_FloatDistributionMin(lifetime->lifeTime, desc.lifetime.lifeTime.x),
            Internal::Evaluate_FloatDistributionMax(lifetime->lifeTime, desc.lifetime.lifeTime.y)
        };
        Internal::Fill_FloatCurvePayload(desc.lifetime.lifeTimeCurve, lifetime->lifeTime, desc.lifetime.lifeTime.y);
        desc.lifetimeSeed = Internal::Build_RandomSeedDesc(lifetime->lifeTime);
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
        desc.initialColor.colorSeed = Internal::Build_RandomSeedDesc(initialColor->color);
        desc.initialColor.alphaSeed = Internal::Build_RandomSeedDesc(initialColor->alpha);
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
        desc.colorOverLife.colorSeed = Internal::Build_RandomSeedDesc(colorOverLife->colorOverLife);
        desc.colorOverLife.alphaSeed = Internal::Build_RandomSeedDesc(colorOverLife->alphaOverLife);
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
        if (subUvFrameOverLife->playbackMode == SubUVFramePlaybackMode::RandomFrame)
            desc.subUVRandomFrameSeed = Internal::Build_RandomSeedDesc(subUvFrameOverLife->randomSeed);

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
        desc.motion.initialVelocitySeed = Internal::Build_RandomSeedDesc(initialVelocity->velocity);
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
        desc.motion.initialRadialVelocityCenterDirectionMode =
            Internal::Resolve_PointParticleInitialRadialVelocityCenterDirectionMode(initialRadialVelocity->centerDirectionMode);
        desc.motion.radialSpeed = Vec2{
            Internal::Evaluate_FloatDistributionMin(initialRadialVelocity->speed, desc.motion.radialSpeed.x),
            Internal::Evaluate_FloatDistributionMax(initialRadialVelocity->speed, desc.motion.radialSpeed.y)
        };
        desc.motion.initialRadialVelocitySeed = Internal::Build_RandomSeedDesc(initialRadialVelocity->speed);
    }

    if (const auto* velocityCone = Internal::Find_ModuleData<VelocityConeModuleData>(emitter, AuthoringModuleType::VelocityCone))
    {
        Internal::Fill_VelocityConePayload(desc.motion, *velocityCone);
        desc.motion.velocityConeSeed = Internal::Build_RandomSeedDesc(velocityCone->speed);
    }

    if (const auto* acceleration = Internal::Find_ModuleData<AccelerationModuleData>(emitter, AuthoringModuleType::Acceleration))
    {
        desc.motion.enabled = true;
        desc.motion.accelerationMin = Internal::Evaluate_Vector3DistributionMin(acceleration->acceleration, desc.motion.accelerationMin);
        desc.motion.accelerationMax = Internal::Evaluate_Vector3DistributionMax(acceleration->acceleration, desc.motion.accelerationMax);
        Internal::Fill_AccelerationCurvePayload(desc.motion, acceleration->acceleration);
        desc.motion.accelerationSeed = Internal::Build_RandomSeedDesc(acceleration->acceleration);
    }

    if (const auto* drag = Internal::Find_ModuleData<DragModuleData>(emitter, AuthoringModuleType::Drag))
    {
        desc.motion.enabled = true;
        desc.motion.drag = Vec2{
            max(0.f, Internal::Evaluate_FloatDistributionMin(drag->drag, desc.motion.drag.x)),
            max(0.f, Internal::Evaluate_FloatDistributionMax(drag->drag, desc.motion.drag.y))
        };
        desc.motion.dragSeed = Internal::Build_RandomSeedDesc(drag->drag);
    }

    if (const auto* velocityOverLife = Internal::Find_ModuleData<VelocityOverLifeModuleData>(emitter, AuthoringModuleType::VelocityOverLife))
    {
        desc.motion.enabled = true;
        Internal::Fill_VelocityScaleByLifePayload(desc.motion, velocityOverLife->scaleOverLife);
    }

    if (const auto* orbitOverLife = Internal::Find_ModuleData<OrbitOverLifeModuleData>(emitter, AuthoringModuleType::OrbitOverLife))
        desc.orbitOverLife = Internal::Build_OrbitOverLifeDesc(*orbitOverLife);

    if (const auto* initialRotation = Internal::Find_ModuleData<InitialRotationModuleData>(emitter, AuthoringModuleType::InitialRotation))
    {
        desc.rotation.enabled = true;
        desc.rotation.initialRotationDegrees = Vec2{
            Internal::Evaluate_FloatDistributionMin(initialRotation->rotationDegrees, desc.rotation.initialRotationDegrees.x),
            Internal::Evaluate_FloatDistributionMax(initialRotation->rotationDegrees, desc.rotation.initialRotationDegrees.y)
        };
        desc.rotation.initialRotationSeed = Internal::Build_RandomSeedDesc(initialRotation->rotationDegrees);
    }

    if (const auto* planeRadialOrientation = Internal::Find_ModuleData<PlaneRadialOrientationModuleData>(emitter, AuthoringModuleType::PlaneRadialOrientation))
        desc.planeRadialOrientation = Internal::Build_PointParticlePlaneRadialOrientationDesc(*planeRadialOrientation);

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
        desc.rotation.initialRotationRateSeed = Internal::Build_RandomSeedDesc(initialRotationRate->rotationRateDegrees);
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
    definition.kind = EffectEmitterKind::Trail;
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
        emitterDesc.autoLifeFade = trailData->autoLifeFade;
        emitterDesc.sampleSpacing = max(0.001f, trailData->sampleSpacing);
        emitterDesc.curveSubdivision = max(0u, trailData->curveSubdivision);
        emitterDesc.smoothTangent = trailData->smoothTangent;
        emitterDesc.sideFade = clamp(trailData->sideFade, 0.001f, 0.49f);
    }

    if (const auto* required = Internal::Find_ModuleData<RequiredModuleData>(emitter, AuthoringModuleType::Required))
    {
        EffectRuntime::Copy_EffectMaterialInstanceToRuntimeDesc(required->material, emitterDesc.material);
        EffectRuntime::Apply_TrailMaterialRuntimePolicy(emitterDesc.material);

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
        emitterDesc.material.scalarModulation =
            Build_MaterialScalarModulationRuntimeDesc(*materialScalarModulation);
        emitterDesc.material.coreColorRgbModulation =
            Build_CoreColorRgbModulationRuntimeDesc(*materialScalarModulation);
    }

    if (const auto* lifetime = Internal::Find_ModuleData<LifetimeModuleData>(emitter, AuthoringModuleType::Lifetime))
    {
        emitterDesc.useLifetimeSegmentLifetime = true;
        emitterDesc.lifetime.lifeTime = Vec2{
            Internal::Evaluate_FloatDistributionMin(lifetime->lifeTime, emitterDesc.lifetime.lifeTime.x),
            Internal::Evaluate_FloatDistributionMax(lifetime->lifeTime, emitterDesc.lifetime.lifeTime.y)
        };
        emitterDesc.lifetimeSeed = Internal::Build_RandomSeedDesc(lifetime->lifeTime);
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
        emitterDesc.spawnPerUnit.enabled = true;
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

EffectEmitterDefinition Level_EffectEditor::Build_PreviewSourceHistoryRibbonEmitterDefinition(const AuthoringEmitter& emitter) const
{
    EffectEmitterDefinition definition{};
    definition.id = emitter.id;
    definition.name = emitter.name;
    definition.kind = EffectEmitterKind::Ribbon;
    definition.enabled = emitter.enabled;
    definition.renderLayerOverride = Internal::Resolve_EffectRenderLayerOverride(emitter.renderLayerOverride);

    ComputeRibbonEmitterDesc emitterDesc{};
    emitterDesc.renderLayerOverride = definition.renderLayerOverride;

    const RibbonTypeData* sourceHistoryData = get_if<RibbonTypeData>(&emitter.typeData.payload);
    const RibbonTypeData defaultSourceHistoryData{};
    const RibbonTypeData& sourceHistoryPolicy =
        sourceHistoryData != nullptr ? *sourceHistoryData : defaultSourceHistoryData;

    emitterDesc.sourceMode = sourceHistoryPolicy.sourceMode == SourceHistoryRibbonSourceMode::ParticleEmitter
                             ? EffectSourceHistoryRibbonSourceMode::ParticleEmitter
                             : EffectSourceHistoryRibbonSourceMode::SelfRoot;
    emitterDesc.sourceEmitterId =
        emitterDesc.sourceMode == EffectSourceHistoryRibbonSourceMode::ParticleEmitter
        ? sourceHistoryPolicy.sourceEmitterId
        : 0u;
    emitterDesc.followerLaneCount = max(1u, sourceHistoryPolicy.followerLaneCount);
    emitterDesc.maxSampleCount = max(2u, sourceHistoryPolicy.maxSampleCount);
    emitterDesc.sampleSpacing = max(0.001f, sourceHistoryPolicy.sampleSpacing);
    emitterDesc.sampleInterval = max(0.f, sourceHistoryPolicy.sampleInterval);
    emitterDesc.maxLength = max(0.f, sourceHistoryPolicy.maxLength);
    emitterDesc.tailFadeLength = max(0.f, sourceHistoryPolicy.tailFadeLength);
    emitterDesc.laneSpawnFadeInEnabled = sourceHistoryPolicy.laneSpawnFadeInEnabled;
    emitterDesc.laneSpawnFadeInDuration = max(0.f, sourceHistoryPolicy.laneSpawnFadeInDuration);
    emitterDesc.baseWidth = max(0.001f, sourceHistoryPolicy.baseWidth);
    emitterDesc.tilingDistance = max(0.f, sourceHistoryPolicy.tilingDistance);
    emitterDesc.autoLifeFade = sourceHistoryPolicy.autoLifeFade;
    emitterDesc.tailCollapseOnIdle = sourceHistoryPolicy.tailCollapseOnIdle;
    emitterDesc.tailCollapseSpeed = max(0.f, sourceHistoryPolicy.tailCollapseSpeed);
    emitterDesc.previewMotionPreset = Internal::Resolve_SourceHistoryPreviewMotionPreset(sourceHistoryPolicy.previewMotionPreset);
    emitterDesc.previewMotionAxis = Internal::Resolve_SourceHistoryPreviewMotionAxis(sourceHistoryPolicy.previewMotionAxis);
    emitterDesc.previewMotionAmplitude = max(0.f, sourceHistoryPolicy.previewMotionAmplitude);
    emitterDesc.previewMotionSpeed = max(0.f, sourceHistoryPolicy.previewMotionSpeed);

    if (const auto* required = Internal::Find_ModuleData<RequiredModuleData>(emitter, AuthoringModuleType::Required))
    {
        EffectRuntime::Copy_EffectMaterialInstanceToRuntimeDesc(required->material, emitterDesc.material);

        emitterDesc.sort.sortPolicy = Internal::Resolve_EffectSortPolicy(required->sortPolicy);
        emitterDesc.sort.sortLayer = required->sortLayer;
        emitterDesc.sort.artistSortBias = required->sortBias;

        emitterDesc.playback.duration = max(0.0001f, required->duration);
        emitterDesc.playback.loopCount = required->loopCount;
        emitterDesc.playback.delay = max(0.f, required->delay);
        emitterDesc.playback.delayFirstLoopOnly = required->delayFirstLoopOnly;
        emitterDesc.playback.killOnDeactivate = required->killOnDeactivate;
        emitterDesc.playback.killOnCompleted = required->killOnCompleted;
        definition.localPosition = required->emitterOrigin;
        definition.localRotationDegrees = required->emitterRotationDegrees;
        definition.useLocalSpace = required->useLocalSpace;
    }

    if (const auto* materialScalarModulation = Internal::Find_ModuleData<MaterialScalarModulationModuleData>(
        emitter,
        AuthoringModuleType::MaterialScalarModulation
    ))
    {
        emitterDesc.material.scalarModulation =
            Build_MaterialScalarModulationRuntimeDesc(*materialScalarModulation);
        emitterDesc.material.coreColorRgbModulation =
            Build_CoreColorRgbModulationRuntimeDesc(*materialScalarModulation);
    }

    if (const auto* lifetime = Internal::Find_ModuleData<LifetimeModuleData>(emitter, AuthoringModuleType::Lifetime))
    {
        emitterDesc.useLifetimeSampleLifetime = true;
        emitterDesc.lifetime.lifeTime = Vec2{
            Internal::Evaluate_FloatDistributionMin(lifetime->lifeTime, emitterDesc.lifetime.lifeTime.x),
            Internal::Evaluate_FloatDistributionMax(lifetime->lifeTime, emitterDesc.lifetime.lifeTime.y)
        };
        emitterDesc.lifetimeSeed = Internal::Build_RandomSeedDesc(lifetime->lifeTime);
        emitterDesc.sampleLifetime = max(0.0001f, emitterDesc.lifetime.lifeTime.y);
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
        emitterDesc.initialColor.colorSeed = Internal::Build_RandomSeedDesc(initialColor->color);
        emitterDesc.initialColor.alphaSeed = Internal::Build_RandomSeedDesc(initialColor->alpha);
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
        emitterDesc.colorOverLife.colorSeed = Internal::Build_RandomSeedDesc(colorOverLife->colorOverLife);
        emitterDesc.colorOverLife.alphaSeed = Internal::Build_RandomSeedDesc(colorOverLife->alphaOverLife);
        Internal::Fill_ColorOverLifeCurvePayload(emitterDesc.colorOverLife.curve, *colorOverLife);
    }

    if (const auto* subUvFrameOverLife = Internal::Find_ModuleData<SubUVFrameOverLifeModuleData>(emitter, AuthoringModuleType::SubUVFrameOverLife))
    {
        emitterDesc.subUVFrameOverLife.enabled = true;
        emitterDesc.subUVFrameOverLife.startFrame = subUvFrameOverLife->startFrame;
        emitterDesc.subUVFrameOverLife.endFrame = subUvFrameOverLife->endFrame;
        emitterDesc.subUVFrameOverLife.loop = subUvFrameOverLife->loop;
        emitterDesc.subUVFrameOverLife.playbackMode = subUvFrameOverLife->playbackMode;
        emitterDesc.subUVFrameOverLife.framesPerSecond = max(0.f, subUvFrameOverLife->framesPerSecond);
        Internal::Fill_SubUVFrameCurvePayload(emitterDesc.subUVFrameOverLife, subUvFrameOverLife->frameIndex);
        if (subUvFrameOverLife->playbackMode == SubUVFramePlaybackMode::RandomFrame)
            emitterDesc.subUVRandomFrameSeed = Internal::Build_RandomSeedDesc(subUvFrameOverLife->randomSeed);

        if (const auto* required = Internal::Find_ModuleData<RequiredModuleData>(emitter, AuthoringModuleType::Required))
        {
            const uint32 frameCount = Internal::Resolve_SubUVFrameCount(*required);
            const uint32 lastFrame = frameCount > 0 ? frameCount - 1 : 0;
            emitterDesc.subUVFrameOverLife.startFrame = min(emitterDesc.subUVFrameOverLife.startFrame, lastFrame);
            emitterDesc.subUVFrameOverLife.endFrame = min(emitterDesc.subUVFrameOverLife.endFrame, lastFrame);

            if (0 == subUvFrameOverLife->startFrame && 0 == subUvFrameOverLife->endFrame && frameCount > 1)
                emitterDesc.subUVFrameOverLife.endFrame = lastFrame;
        }
    }

    if (const auto* sizeByLife = Internal::Find_ModuleData<SizeByLifeModuleData>(emitter, AuthoringModuleType::SizeByLife))
        Internal::Fill_SizeByLifePayload(emitterDesc.sizeByLife, *sizeByLife);

    emitterDesc.maxSampleCount = Internal::Resolve_RibbonAutoHistoryCount(
        emitterDesc.maxSampleCount,
        emitterDesc.sampleLifetime,
        emitterDesc.sampleSpacing,
        emitterDesc.maxLength
    );

    definition.concreteDesc = emitterDesc;
    return definition;
}

EffectEmitterDefinition Level_EffectEditor::Build_PreviewSourceHistorySpriteTrailEmitterDefinition(const AuthoringEmitter& emitter) const
{
    EffectEmitterDefinition definition{};
    definition.id = emitter.id;
    definition.name = emitter.name;
    definition.kind = EffectEmitterKind::SourceHistorySpriteTrail;
    definition.enabled = emitter.enabled;
    definition.renderLayerOverride = Internal::Resolve_EffectRenderLayerOverride(emitter.renderLayerOverride);

    ComputeSourceHistorySpriteTrailEmitterDesc emitterDesc{};
    emitterDesc.renderLayerOverride = definition.renderLayerOverride;

    const SourceHistorySpriteTrailTypeData* typeData = get_if<SourceHistorySpriteTrailTypeData>(&emitter.typeData.payload);
    const SourceHistorySpriteTrailTypeData defaultTypeData{};
    const SourceHistorySpriteTrailTypeData& policy = typeData != nullptr ? *typeData : defaultTypeData;

    emitterDesc.sourceMode = policy.sourceMode == SourceHistoryRibbonSourceMode::ParticleEmitter
                             ? EffectSourceHistoryRibbonSourceMode::ParticleEmitter
                             : EffectSourceHistoryRibbonSourceMode::SelfRoot;
    emitterDesc.sourceEmitterId =
        emitterDesc.sourceMode == EffectSourceHistoryRibbonSourceMode::ParticleEmitter
        ? policy.sourceEmitterId
        : 0u;
    emitterDesc.followerLaneCount = max(1u, policy.followerLaneCount);
    emitterDesc.sampleLifetime = max(0.0001f, policy.sampleLifetime);
    emitterDesc.sampleSpacing = max(0.001f, policy.sampleSpacing);
    emitterDesc.maxLength = max(0.f, policy.maxLength);
    emitterDesc.stampSpawnMode =
        policy.stampSpawnMode == SourceHistorySpriteTrailStampSpawnMode::Time
        ? EffectSourceHistorySpriteTrailStampSpawnMode::Time
        : EffectSourceHistorySpriteTrailStampSpawnMode::Distance;
    emitterDesc.stampSpacing = max(0.001f, policy.stampSpacing);
    emitterDesc.stampInterval = max(0.001f, policy.stampInterval);
    emitterDesc.stampLifetime = max(0.0001f, policy.stampLifetime);
    emitterDesc.maxStampCount = max(1u, policy.maxStampCount);
    emitterDesc.cardLength = max(0.001f, policy.cardLength);
    emitterDesc.cardWidth = max(0.001f, policy.cardWidth);
    emitterDesc.flipU = policy.flipU;
    emitterDesc.flipV = policy.flipV;
    emitterDesc.rotationOffsetDegrees = policy.rotationOffsetDegrees;
    emitterDesc.spawnJitter = max(0.f, policy.spawnJitter);
    emitterDesc.previewMotionPreset = Internal::Resolve_SourceHistoryPreviewMotionPreset(policy.previewMotionPreset);
    emitterDesc.previewMotionAxis = Internal::Resolve_SourceHistoryPreviewMotionAxis(policy.previewMotionAxis);
    emitterDesc.previewMotionAmplitude = max(0.f, policy.previewMotionAmplitude);
    emitterDesc.previewMotionSpeed = max(0.f, policy.previewMotionSpeed);

    if (const auto* required = Internal::Find_ModuleData<RequiredModuleData>(emitter, AuthoringModuleType::Required))
    {
        EffectRuntime::Copy_EffectMaterialInstanceToRuntimeDesc(required->material, emitterDesc.material);
        emitterDesc.spriteRender.screenAlignment = Internal::Resolve_EffectScreenAlignment(required->screenAlignment);
        emitterDesc.spriteRender.directionalAlignmentMode = Internal::Resolve_EffectSpriteDirectionalAlignmentMode(required->directionalAlignmentMode);
        emitterDesc.spriteRender.spriteTextureAxis = Internal::Resolve_EffectSpriteTextureAxis(required->spriteTextureAxis);
        emitterDesc.spriteRender.spriteRollOffsetDegrees = required->spriteRollOffsetDegrees;

        emitterDesc.sort.sortPolicy = Internal::Resolve_EffectSortPolicy(required->sortPolicy);
        emitterDesc.sort.sortLayer = required->sortLayer;
        emitterDesc.sort.artistSortBias = required->sortBias;

        emitterDesc.playback.duration = max(0.0001f, required->duration);
        emitterDesc.playback.loopCount = required->loopCount;
        emitterDesc.playback.delay = max(0.f, required->delay);
        emitterDesc.playback.delayFirstLoopOnly = required->delayFirstLoopOnly;
        emitterDesc.playback.killOnDeactivate = required->killOnDeactivate;
        emitterDesc.playback.killOnCompleted = required->killOnCompleted;
        emitterDesc.drawLimit.useMaxDrawCount = required->useMaxDrawCount;
        emitterDesc.drawLimit.maxDrawCount = max(1u, required->maxDrawCount);
        definition.localPosition = required->emitterOrigin;
        definition.localRotationDegrees = required->emitterRotationDegrees;
        definition.useLocalSpace = required->useLocalSpace;
    }

    if (const auto* materialScalarModulation = Internal::Find_ModuleData<MaterialScalarModulationModuleData>(
        emitter,
        AuthoringModuleType::MaterialScalarModulation
    ))
    {
        emitterDesc.material.scalarModulation =
            Build_MaterialScalarModulationRuntimeDesc(*materialScalarModulation);
        emitterDesc.material.coreColorRgbModulation =
            Build_CoreColorRgbModulationRuntimeDesc(*materialScalarModulation);
    }

    if (const auto* initialSize = Internal::Find_ModuleData<InitialSizeModuleData>(emitter, AuthoringModuleType::InitialSize))
    {
        emitterDesc.initialSize.enabled = true;
        const Vec2 minSize = Internal::Evaluate_Vector2DistributionMin(initialSize->size, emitterDesc.initialSize.sizeMin);
        const Vec2 maxSize = Internal::Evaluate_Vector2DistributionMax(initialSize->size, emitterDesc.initialSize.sizeMax);
        emitterDesc.initialSize.sizeMin = Vec2{ min(minSize.x, maxSize.x), min(minSize.y, maxSize.y) };
        emitterDesc.initialSize.sizeMax = Vec2{ max(minSize.x, maxSize.x), max(minSize.y, maxSize.y) };
        emitterDesc.initialSize.sizeSeed = Internal::Build_RandomSeedDesc(initialSize->size);
    }

    if (const auto* lifetime = Internal::Find_ModuleData<LifetimeModuleData>(emitter, AuthoringModuleType::Lifetime))
    {
        emitterDesc.useLifetimeStampLifetime = true;
        emitterDesc.lifetime.lifeTime = Vec2{
            Internal::Evaluate_FloatDistributionMin(lifetime->lifeTime, emitterDesc.lifetime.lifeTime.x),
            Internal::Evaluate_FloatDistributionMax(lifetime->lifeTime, emitterDesc.lifetime.lifeTime.y)
        };
        emitterDesc.lifetimeSeed = Internal::Build_RandomSeedDesc(lifetime->lifeTime);
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
        emitterDesc.initialColor.colorSeed = Internal::Build_RandomSeedDesc(initialColor->color);
        emitterDesc.initialColor.alphaSeed = Internal::Build_RandomSeedDesc(initialColor->alpha);
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
        emitterDesc.colorOverLife.colorSeed = Internal::Build_RandomSeedDesc(colorOverLife->colorOverLife);
        emitterDesc.colorOverLife.alphaSeed = Internal::Build_RandomSeedDesc(colorOverLife->alphaOverLife);
        Internal::Fill_ColorOverLifeCurvePayload(emitterDesc.colorOverLife.curve, *colorOverLife);
    }

    if (const auto* subUvFrameOverLife = Internal::Find_ModuleData<SubUVFrameOverLifeModuleData>(emitter, AuthoringModuleType::SubUVFrameOverLife))
    {
        emitterDesc.subUVFrameOverLife.enabled = true;
        emitterDesc.subUVFrameOverLife.startFrame = subUvFrameOverLife->startFrame;
        emitterDesc.subUVFrameOverLife.endFrame = subUvFrameOverLife->endFrame;
        emitterDesc.subUVFrameOverLife.loop = subUvFrameOverLife->loop;
        emitterDesc.subUVFrameOverLife.playbackMode = subUvFrameOverLife->playbackMode;
        emitterDesc.subUVFrameOverLife.framesPerSecond = max(0.f, subUvFrameOverLife->framesPerSecond);
        Internal::Fill_SubUVFrameCurvePayload(emitterDesc.subUVFrameOverLife, subUvFrameOverLife->frameIndex);
        if (subUvFrameOverLife->playbackMode == SubUVFramePlaybackMode::RandomFrame)
            emitterDesc.subUVRandomFrameSeed = Internal::Build_RandomSeedDesc(subUvFrameOverLife->randomSeed);

        if (const auto* required = Internal::Find_ModuleData<RequiredModuleData>(emitter, AuthoringModuleType::Required))
        {
            const uint32 frameCount = Internal::Resolve_SubUVFrameCount(*required);
            const uint32 lastFrame = frameCount > 0 ? frameCount - 1 : 0;
            emitterDesc.subUVFrameOverLife.startFrame = min(emitterDesc.subUVFrameOverLife.startFrame, lastFrame);
            emitterDesc.subUVFrameOverLife.endFrame = min(emitterDesc.subUVFrameOverLife.endFrame, lastFrame);

            if (0 == subUvFrameOverLife->startFrame && 0 == subUvFrameOverLife->endFrame && frameCount > 1)
                emitterDesc.subUVFrameOverLife.endFrame = lastFrame;
        }
    }

    if (const auto* sizeByLife = Internal::Find_ModuleData<SizeByLifeModuleData>(emitter, AuthoringModuleType::SizeByLife))
        Internal::Fill_SizeByLifePayload(emitterDesc.sizeByLife, *sizeByLife);

    if (const auto* initialVelocity = Internal::Find_ModuleData<InitialVelocityModuleData>(emitter, AuthoringModuleType::InitialVelocity))
    {
        emitterDesc.motion.enabled = true;
        emitterDesc.motion.initialVelocityEnabled = true;
        emitterDesc.motion.initialVelocityInWorldSpace = initialVelocity->inWorldSpace;
        emitterDesc.motion.initialVelocityMin =
            Internal::Evaluate_Vector3DistributionMin(initialVelocity->velocity, emitterDesc.motion.initialVelocityMin);
        emitterDesc.motion.initialVelocityMax =
            Internal::Evaluate_Vector3DistributionMax(initialVelocity->velocity, emitterDesc.motion.initialVelocityMax);
        emitterDesc.motion.initialVelocitySeed = Internal::Build_RandomSeedDesc(initialVelocity->velocity);
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
        emitterDesc.motion.initialRadialVelocityCenterDirectionMode =
            Internal::Resolve_PointParticleInitialRadialVelocityCenterDirectionMode(initialRadialVelocity->centerDirectionMode);
        emitterDesc.motion.radialSpeed = Vec2{
            Internal::Evaluate_FloatDistributionMin(initialRadialVelocity->speed, emitterDesc.motion.radialSpeed.x),
            Internal::Evaluate_FloatDistributionMax(initialRadialVelocity->speed, emitterDesc.motion.radialSpeed.y)
        };
        emitterDesc.motion.initialRadialVelocitySeed = Internal::Build_RandomSeedDesc(initialRadialVelocity->speed);
    }

    if (const auto* velocityCone = Internal::Find_ModuleData<VelocityConeModuleData>(emitter, AuthoringModuleType::VelocityCone))
    {
        Internal::Fill_VelocityConePayload(emitterDesc.motion, *velocityCone);
        emitterDesc.motion.velocityConeSeed = Internal::Build_RandomSeedDesc(velocityCone->speed);
    }

    if (const auto* acceleration = Internal::Find_ModuleData<AccelerationModuleData>(emitter, AuthoringModuleType::Acceleration))
    {
        emitterDesc.motion.enabled = true;
        emitterDesc.motion.accelerationMin = Internal::Evaluate_Vector3DistributionMin(acceleration->acceleration, emitterDesc.motion.accelerationMin);
        emitterDesc.motion.accelerationMax = Internal::Evaluate_Vector3DistributionMax(acceleration->acceleration, emitterDesc.motion.accelerationMax);
        Internal::Fill_AccelerationCurvePayload(emitterDesc.motion, acceleration->acceleration);
        emitterDesc.motion.accelerationSeed = Internal::Build_RandomSeedDesc(acceleration->acceleration);
    }

    if (const auto* drag = Internal::Find_ModuleData<DragModuleData>(emitter, AuthoringModuleType::Drag))
    {
        emitterDesc.motion.enabled = true;
        emitterDesc.motion.drag = Vec2{
            max(0.f, Internal::Evaluate_FloatDistributionMin(drag->drag, emitterDesc.motion.drag.x)),
            max(0.f, Internal::Evaluate_FloatDistributionMax(drag->drag, emitterDesc.motion.drag.y))
        };
        emitterDesc.motion.dragSeed = Internal::Build_RandomSeedDesc(drag->drag);
    }

    if (const auto* velocityOverLife = Internal::Find_ModuleData<VelocityOverLifeModuleData>(emitter, AuthoringModuleType::VelocityOverLife))
    {
        emitterDesc.motion.enabled = true;
        Internal::Fill_VelocityScaleByLifePayload(emitterDesc.motion, velocityOverLife->scaleOverLife);
    }

    if (const auto* initialRotation = Internal::Find_ModuleData<InitialRotationModuleData>(emitter, AuthoringModuleType::InitialRotation))
    {
        emitterDesc.rotation.enabled = true;
        emitterDesc.rotation.initialRotationDegrees = Vec2{
            Internal::Evaluate_FloatDistributionMin(initialRotation->rotationDegrees, emitterDesc.rotation.initialRotationDegrees.x),
            Internal::Evaluate_FloatDistributionMax(initialRotation->rotationDegrees, emitterDesc.rotation.initialRotationDegrees.y)
        };
        emitterDesc.rotation.initialRotationSeed = Internal::Build_RandomSeedDesc(initialRotation->rotationDegrees);
    }

    if (const auto* rotationOverLife = Internal::Find_ModuleData<RotationOverLifeModuleData>(emitter, AuthoringModuleType::RotationOverLife))
    {
        emitterDesc.rotation.enabled = true;
        Internal::Fill_RotationOverLifePayload(emitterDesc.rotation, *rotationOverLife);
    }

    if (const auto* initialRotationRate = Internal::Find_ModuleData<InitialRotationRateModuleData>(emitter, AuthoringModuleType::InitialRotationRate))
    {
        emitterDesc.rotation.enabled = true;
        emitterDesc.rotation.initialRotationRateDegrees = Vec2{
            Internal::Evaluate_FloatDistributionMin(initialRotationRate->rotationRateDegrees, emitterDesc.rotation.initialRotationRateDegrees.x),
            Internal::Evaluate_FloatDistributionMax(initialRotationRate->rotationRateDegrees, emitterDesc.rotation.initialRotationRateDegrees.y)
        };
        emitterDesc.rotation.initialRotationRateSeed = Internal::Build_RandomSeedDesc(initialRotationRate->rotationRateDegrees);
    }

    if (const auto* rotationRateScaleByLife = Internal::Find_ModuleData<RotationRateScaleByLifeModuleData>(
        emitter,
        AuthoringModuleType::RotationRateScaleByLife
    ))
    {
        emitterDesc.rotation.enabled = true;
        Internal::Fill_RotationRateScaleByLifePayload(emitterDesc.rotation, *rotationRateScaleByLife);
    }

    definition.concreteDesc = emitterDesc;
    return definition;
}

EffectEmitterDefinition Level_EffectEditor::Build_PreviewBeamEmitterDefinition(const AuthoringEmitter& emitter) const
{
    EffectEmitterDefinition definition{};
    definition.id = emitter.id;
    definition.name = emitter.name;
    definition.kind = EffectEmitterKind::Beam;
    definition.enabled = emitter.enabled;
    definition.renderLayerOverride = Internal::Resolve_EffectRenderLayerOverride(emitter.renderLayerOverride);

    ComputeBeamEmitterDesc emitterDesc{};
    emitterDesc.renderLayerOverride = definition.renderLayerOverride;

    const BeamTypeData* beamData = get_if<BeamTypeData>(&emitter.typeData.payload);
    const BeamTypeData defaultBeamData{};
    const BeamTypeData& beamPolicy = beamData != nullptr ? *beamData : defaultBeamData;

    emitterDesc.endpointMode = beamPolicy.endpointMode == BeamEndpointMode::DirectionLength
                               ? EffectBeamEndpointMode::DirectionLength
                               : EffectBeamEndpointMode::StartEnd;
    emitterDesc.localStart = beamPolicy.localStart;
    emitterDesc.localEnd = beamPolicy.localEnd;
    emitterDesc.localDirection = beamPolicy.localDirection;
    emitterDesc.length = max(0.f, beamPolicy.length);
    emitterDesc.segmentCount = max(1u, beamPolicy.segmentCount);
    emitterDesc.noiseAmplitude = max(0.f, beamPolicy.noiseAmplitude);
    emitterDesc.seed = beamPolicy.seed;
    emitterDesc.stripCount = max(1u, beamPolicy.stripCount);
    emitterDesc.endSpreadRadius = max(0.f, beamPolicy.endSpreadRadius);
    emitterDesc.lengthVariance = max(0.f, beamPolicy.lengthVariance);
    switch (beamPolicy.branchPreset)
    {
    case BeamBranchPreset::DownStrike:
        emitterDesc.branchPreset = EffectBeamBranchPreset::DownStrike;
        break;
    case BeamBranchPreset::Entangle:
        emitterDesc.branchPreset = EffectBeamBranchPreset::Entangle;
        break;
    case BeamBranchPreset::ShortCrack:
        emitterDesc.branchPreset = EffectBeamBranchPreset::ShortCrack;
        break;
    case BeamBranchPreset::EndGuided:
    default:
        emitterDesc.branchPreset = EffectBeamBranchPreset::EndGuided;
        break;
    }
    emitterDesc.branchEnabled = beamPolicy.branchEnabled;
    emitterDesc.branchCount = beamPolicy.branchCount;
    emitterDesc.branchChance = clamp(beamPolicy.branchChance, 0.f, 1.f);
    emitterDesc.branchSegmentCount = max(1u, beamPolicy.branchSegmentCount);
    emitterDesc.branchLength = max(0.f, beamPolicy.branchLength);
    emitterDesc.branchLengthVariance = max(0.f, beamPolicy.branchLengthVariance);
    emitterDesc.branchStartMin = clamp(beamPolicy.branchStartMin, 0.f, 1.f);
    emitterDesc.branchStartMax = clamp(beamPolicy.branchStartMax, 0.f, 1.f);
    if (emitterDesc.branchStartMin > emitterDesc.branchStartMax)
        swap(emitterDesc.branchStartMin, emitterDesc.branchStartMax);
    emitterDesc.branchSpreadRadius = max(0.f, beamPolicy.branchSpreadRadius);
    emitterDesc.branchEndSpreadRadius = max(0.f, beamPolicy.branchEndSpreadRadius);
    emitterDesc.branchOutwardAmount = max(0.f, beamPolicy.branchOutwardAmount);
    emitterDesc.branchCurveAmount = max(0.f, beamPolicy.branchCurveAmount);
    emitterDesc.branchDownLength = max(0.f, beamPolicy.branchDownLength);
    emitterDesc.branchEntangleRadius = max(0.f, beamPolicy.branchEntangleRadius);
    emitterDesc.branchEntangleAdvance = max(0.f, beamPolicy.branchEntangleAdvance);
    emitterDesc.branchCrackLength = max(0.f, beamPolicy.branchCrackLength);
    emitterDesc.branchCrackSpreadRadius = max(0.f, beamPolicy.branchCrackSpreadRadius);
    emitterDesc.branchWidthScale = max(0.f, beamPolicy.branchWidthScale);
    emitterDesc.branchSeedOffset = beamPolicy.branchSeedOffset;
    emitterDesc.baseWidth = max(0.001f, beamPolicy.baseWidth);
    emitterDesc.tilingDistance = max(0.f, beamPolicy.tilingDistance);

    if (const auto* required = Internal::Find_ModuleData<RequiredModuleData>(emitter, AuthoringModuleType::Required))
    {
        EffectRuntime::Copy_EffectMaterialInstanceToRuntimeDesc(required->material, emitterDesc.material);
        emitterDesc.sort.sortPolicy = Internal::Resolve_EffectSortPolicy(required->sortPolicy);
        emitterDesc.sort.sortLayer = required->sortLayer;
        emitterDesc.sort.artistSortBias = required->sortBias;
        emitterDesc.playback.duration = max(0.0001f, required->duration);
        emitterDesc.playback.loopCount = required->loopCount;
        emitterDesc.playback.delay = max(0.f, required->delay);
        emitterDesc.playback.delayFirstLoopOnly = required->delayFirstLoopOnly;
        emitterDesc.playback.killOnDeactivate = required->killOnDeactivate;
        emitterDesc.playback.killOnCompleted = required->killOnCompleted;
        definition.localPosition = required->emitterOrigin;
        definition.localRotationDegrees = required->emitterRotationDegrees;
        definition.useLocalSpace = required->useLocalSpace;
    }

    if (const auto* materialScalarModulation = Internal::Find_ModuleData<MaterialScalarModulationModuleData>(
        emitter,
        AuthoringModuleType::MaterialScalarModulation
    ))
    {
        emitterDesc.material.scalarModulation =
            Build_MaterialScalarModulationRuntimeDesc(*materialScalarModulation);
        emitterDesc.material.coreColorRgbModulation =
            Build_CoreColorRgbModulationRuntimeDesc(*materialScalarModulation);
    }

    if (const auto* lifetime = Internal::Find_ModuleData<LifetimeModuleData>(emitter, AuthoringModuleType::Lifetime))
    {
        emitterDesc.useLifetimeVisualLife = true;
        emitterDesc.lifetime.lifeTime = Vec2{
            Internal::Evaluate_FloatDistributionMin(lifetime->lifeTime, emitterDesc.lifetime.lifeTime.x),
            Internal::Evaluate_FloatDistributionMax(lifetime->lifeTime, emitterDesc.lifetime.lifeTime.y)
        };
        Internal::Fill_FloatCurvePayload(emitterDesc.lifetime.lifeTimeCurve, lifetime->lifeTime, emitterDesc.lifetime.lifeTime.y);
        emitterDesc.lifetimeSeed = Internal::Build_RandomSeedDesc(lifetime->lifeTime);
    }

    if (const auto* initialColor = Internal::Find_ModuleData<InitialColorModuleData>(emitter, AuthoringModuleType::InitialColor))
    {
        const Color minColor = Internal::Evaluate_ColorDistributionMin(initialColor->color, Color{ 1.f, 1.f, 1.f, 1.f });
        const Color maxColor = Internal::Evaluate_ColorDistributionMax(initialColor->color, Color{ 1.f, 1.f, 1.f, 1.f });
        const float minAlpha = clamp(Internal::Evaluate_FloatDistributionMin(initialColor->alpha, 1.f), 0.f, 1.f);
        const float maxAlpha = clamp(Internal::Evaluate_FloatDistributionMax(initialColor->alpha, 1.f), 0.f, 1.f);
        emitterDesc.initialColor.startColorMin = Vec4{ minColor.R(), minColor.G(), minColor.B(), minAlpha };
        emitterDesc.initialColor.startColorMax = Vec4{ maxColor.R(), maxColor.G(), maxColor.B(), maxAlpha };
        emitterDesc.initialColor.colorSeed = Internal::Build_RandomSeedDesc(initialColor->color);
        emitterDesc.initialColor.alphaSeed = Internal::Build_RandomSeedDesc(initialColor->alpha);
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
        emitterDesc.colorOverLife.colorSeed = Internal::Build_RandomSeedDesc(colorOverLife->colorOverLife);
        emitterDesc.colorOverLife.alphaSeed = Internal::Build_RandomSeedDesc(colorOverLife->alphaOverLife);
        Internal::Fill_ColorOverLifeCurvePayload(emitterDesc.colorOverLife.curve, *colorOverLife);
    }

    if (const auto* subUvFrameOverLife = Internal::Find_ModuleData<SubUVFrameOverLifeModuleData>(emitter, AuthoringModuleType::SubUVFrameOverLife))
    {
        emitterDesc.subUVFrameOverLife.enabled = true;
        emitterDesc.subUVFrameOverLife.startFrame = subUvFrameOverLife->startFrame;
        emitterDesc.subUVFrameOverLife.endFrame = subUvFrameOverLife->endFrame;
        emitterDesc.subUVFrameOverLife.loop = subUvFrameOverLife->loop;
        emitterDesc.subUVFrameOverLife.playbackMode = subUvFrameOverLife->playbackMode;
        emitterDesc.subUVFrameOverLife.framesPerSecond = max(0.f, subUvFrameOverLife->framesPerSecond);
        emitterDesc.usePerStripSubUVVariation = subUvFrameOverLife->perStripSubUVVariation;
        Internal::Fill_SubUVFrameCurvePayload(emitterDesc.subUVFrameOverLife, subUvFrameOverLife->frameIndex);
        if (subUvFrameOverLife->playbackMode == SubUVFramePlaybackMode::RandomFrame ||
            (subUvFrameOverLife->playbackMode == SubUVFramePlaybackMode::FramesPerSecond && subUvFrameOverLife->perStripSubUVVariation))
            emitterDesc.subUVRandomFrameSeed = Internal::Build_RandomSeedDesc(subUvFrameOverLife->randomSeed);

        if (const auto* required = Internal::Find_ModuleData<RequiredModuleData>(emitter, AuthoringModuleType::Required))
        {
            const uint32 frameCount = Internal::Resolve_SubUVFrameCount(*required);
            const uint32 lastFrame = frameCount > 0 ? frameCount - 1 : 0;
            emitterDesc.subUVFrameOverLife.startFrame = min(emitterDesc.subUVFrameOverLife.startFrame, lastFrame);
            emitterDesc.subUVFrameOverLife.endFrame = min(emitterDesc.subUVFrameOverLife.endFrame, lastFrame);

            if (0 == subUvFrameOverLife->startFrame && 0 == subUvFrameOverLife->endFrame && frameCount > 1)
                emitterDesc.subUVFrameOverLife.endFrame = lastFrame;
        }
    }

    if (const auto* sizeByLife = Internal::Find_ModuleData<SizeByLifeModuleData>(emitter, AuthoringModuleType::SizeByLife))
        Internal::Fill_SizeByLifePayload(emitterDesc.sizeByLife, *sizeByLife);

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
    }

    const auto* required = Internal::Find_ModuleData<RequiredModuleData>(emitter, AuthoringModuleType::Required);
    if (required != nullptr)
    {
        EffectRuntime::Copy_EffectMaterialInstanceToRuntimeDesc(required->material, emitterDesc.material);

        emitterDesc.playback.duration = max(0.0001f, required->duration);
        emitterDesc.playback.loopCount = required->loopCount;
        emitterDesc.playback.delay = max(0.f, required->delay);
        emitterDesc.playback.delayFirstLoopOnly = required->delayFirstLoopOnly;
        emitterDesc.playback.killOnDeactivate = required->killOnDeactivate;
        emitterDesc.playback.killOnCompleted = required->killOnCompleted;

        emitterDesc.drawLimit.useMaxDrawCount = required->useMaxDrawCount;
        emitterDesc.drawLimit.maxDrawCount = max(1u, required->maxDrawCount);
        emitterDesc.meshTransform.alignment = Internal::Resolve_EffectScreenAlignment(required->screenAlignment);
        emitterDesc.sort.sortPolicy = Internal::Resolve_EffectSortPolicy(required->sortPolicy);
        emitterDesc.sort.sortLayer = required->sortLayer;
        emitterDesc.sort.artistSortBias = required->sortBias;

        definition.localPosition = required->emitterOrigin;
        definition.localRotationDegrees = required->emitterRotationDegrees;
        definition.useLocalSpace = required->useLocalSpace;
    }

    if (const auto* materialScalarModulation = Internal::Find_ModuleData<MaterialScalarModulationModuleData>(
        emitter,
        AuthoringModuleType::MaterialScalarModulation
    ))
    {
        emitterDesc.material.scalarModulation =
            Build_MaterialScalarModulationRuntimeDesc(*materialScalarModulation);
        emitterDesc.material.coreColorRgbModulation =
            Build_CoreColorRgbModulationRuntimeDesc(*materialScalarModulation);
    }

    if (const auto* meshData = get_if<MeshTypeData>(&emitter.typeData.payload))
    {
        if (meshData->hasAssignedMaterialInstance)
            EffectRuntime::Copy_EffectMaterialInstanceToRuntimeDesc(meshData->assignedMaterial, emitterDesc.material);
        else
        {
            const fs::path assignedMaterialPath = Internal::Resolve_AuthoredAssetPath(
                meshData->assignedEffectMaterialGuid,
                meshData->assignedEffectMaterialPath
            );
            EffectMaterialInstanceData assignedMaterial{};
            if (!assignedMaterialPath.empty() && EffectMaterialPresetReader::Read(assignedMaterialPath, assignedMaterial))
                EffectRuntime::Copy_EffectMaterialInstanceToRuntimeDesc(assignedMaterial, emitterDesc.material);
        }
    }

    if (required != nullptr)
    {
        emitterDesc.material.blendMode = required->material.blendMode;
        emitterDesc.material.alphaCutoff = required->material.alphaCutoff;
    }

    if (const auto* spawn = Internal::Find_ModuleData<SpawnModuleData>(emitter, AuthoringModuleType::Spawn))
    {
        emitterDesc.spawn.instanceCount = max(1u, spawn->maxParticleCount);
        emitterDesc.spawn.particleSpawn.processSpawnRate = spawn->processSpawnRate;
        emitterDesc.spawn.particleSpawn.spawnRateRange = Vec2{
            max(0.f, Internal::Evaluate_FloatDistributionMin(spawn->spawnRate, emitterDesc.spawn.particleSpawn.spawnRate)),
            max(0.f, Internal::Evaluate_FloatDistributionMax(spawn->spawnRate, emitterDesc.spawn.particleSpawn.spawnRate))
        };
        emitterDesc.spawn.particleSpawn.spawnRate = emitterDesc.spawn.particleSpawn.spawnRateRange.y;
        emitterDesc.spawn.particleSpawn.spawnRateSeed = Internal::Build_RandomSeedDesc(spawn->spawnRate);
        emitterDesc.spawn.particleSpawn.spawnRateScaleRange = Vec2{
            max(0.f, Internal::Evaluate_FloatDistributionMin(spawn->spawnRateScale, emitterDesc.spawn.particleSpawn.spawnRateScale)),
            max(0.f, Internal::Evaluate_FloatDistributionMax(spawn->spawnRateScale, emitterDesc.spawn.particleSpawn.spawnRateScale))
        };
        emitterDesc.spawn.particleSpawn.spawnRateScale = emitterDesc.spawn.particleSpawn.spawnRateScaleRange.y;
        emitterDesc.spawn.particleSpawn.spawnRateScaleSeed = Internal::Build_RandomSeedDesc(spawn->spawnRateScale);
        emitterDesc.spawn.particleSpawn.processBurstList = spawn->processBurstList;
        emitterDesc.spawn.particleSpawn.burstScaleRange = Vec2{
            max(0.f, Internal::Evaluate_FloatDistributionMin(spawn->burstScale, emitterDesc.spawn.particleSpawn.burstScale)),
            max(0.f, Internal::Evaluate_FloatDistributionMax(spawn->burstScale, emitterDesc.spawn.particleSpawn.burstScale))
        };
        emitterDesc.spawn.particleSpawn.burstScale = emitterDesc.spawn.particleSpawn.burstScaleRange.y;
        emitterDesc.spawn.particleSpawn.burstScaleSeed = Internal::Build_RandomSeedDesc(spawn->burstScale);
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
        emitterDesc.sphereLocation.placementMode = Internal::Resolve_PointParticleSphereLocationPlacementMode(sphereLocation->placementMode);
        emitterDesc.sphereLocationSeed = Internal::Build_RandomSeedDesc(sphereLocation->randomSeed);
    }

    if (const auto* planeRadialLocation = Internal::Find_ModuleData<PlaneRadialLocationModuleData>(emitter, AuthoringModuleType::PlaneRadialLocation))
        emitterDesc.planeRadialLocation = Internal::Build_PointParticlePlaneRadialLocationDesc(*planeRadialLocation);

    if (const auto* lifetime = Internal::Find_ModuleData<LifetimeModuleData>(emitter, AuthoringModuleType::Lifetime))
    {
        emitterDesc.lifetime.lifeTime = Vec2{
            Internal::Evaluate_FloatDistributionMin(lifetime->lifeTime, emitterDesc.lifetime.lifeTime.x),
            Internal::Evaluate_FloatDistributionMax(lifetime->lifeTime, emitterDesc.lifetime.lifeTime.y)
        };
        Internal::Fill_FloatCurvePayload(emitterDesc.lifetime.lifeTimeCurve, lifetime->lifeTime, emitterDesc.lifetime.lifeTime.y);
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
            emitterDesc.meshTransform.scaleByLife.curveKeyValuesXBlock1 = Vec4{ 1.f, 1.f, 1.f, 1.f };
        }
        if (!meshSizeByLife->multiplyY)
        {
            emitterDesc.meshTransform.scaleByLife.start.y = 1.f;
            emitterDesc.meshTransform.scaleByLife.end.y = 1.f;
            emitterDesc.meshTransform.scaleByLife.curveKeyValuesY = Vec4{ 1.f, 1.f, 1.f, 1.f };
            emitterDesc.meshTransform.scaleByLife.curveKeyValuesYBlock1 = Vec4{ 1.f, 1.f, 1.f, 1.f };
        }
        if (!meshSizeByLife->multiplyZ)
        {
            emitterDesc.meshTransform.scaleByLife.start.z = 1.f;
            emitterDesc.meshTransform.scaleByLife.end.z = 1.f;
            emitterDesc.meshTransform.scaleByLife.curveKeyValuesZ = Vec4{ 1.f, 1.f, 1.f, 1.f };
            emitterDesc.meshTransform.scaleByLife.curveKeyValuesZBlock1 = Vec4{ 1.f, 1.f, 1.f, 1.f };
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
        emitterDesc.motion.initialRadialVelocityCenterDirectionMode =
            Internal::Resolve_PointParticleInitialRadialVelocityCenterDirectionMode(initialRadialVelocity->centerDirectionMode);
        emitterDesc.motion.radialSpeed = Vec2{
            Internal::Evaluate_FloatDistributionMin(initialRadialVelocity->speed, emitterDesc.motion.radialSpeed.x),
            Internal::Evaluate_FloatDistributionMax(initialRadialVelocity->speed, emitterDesc.motion.radialSpeed.y)
        };
    }

    if (const auto* velocityCone = Internal::Find_ModuleData<VelocityConeModuleData>(emitter, AuthoringModuleType::VelocityCone))
        Internal::Fill_VelocityConePayload(emitterDesc.motion, *velocityCone);

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
        Internal::Fill_VelocityScaleByLifePayload(emitterDesc.motion, velocityOverLife->scaleOverLife);
    }

    if (const auto* orbitOverLife = Internal::Find_ModuleData<OrbitOverLifeModuleData>(emitter, AuthoringModuleType::OrbitOverLife))
        emitterDesc.orbitOverLife = Internal::Build_OrbitOverLifeDesc(*orbitOverLife);

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

    if (const auto* sphereRadialOrientation = Internal::Find_ModuleData<SphereRadialOrientationModuleData>(
        emitter,
        AuthoringModuleType::SphereRadialOrientation
    ))
        emitterDesc.sphereRadialOrientation = Internal::Build_PointParticleSphereRadialOrientationDesc(*sphereRadialOrientation);
    if (const auto* planeRadialOrientation = Internal::Find_ModuleData<PlaneRadialOrientationModuleData>(emitter, AuthoringModuleType::PlaneRadialOrientation))
        emitterDesc.planeRadialOrientation = Internal::Build_PointParticlePlaneRadialOrientationDesc(*planeRadialOrientation);

    if (const auto* meshRotationOverLife = Internal::Find_ModuleData<MeshRotationOverLifeModuleData>(emitter, AuthoringModuleType::MeshRotationOverLife))
    {
        emitterDesc.meshTransform.rotationEnabled = true;
        Internal::Fill_MeshVector3CurvePayload(emitterDesc.meshTransform.rotationByLife, meshRotationOverLife->rotationOverLife, Vec3{});
    }

    if (const auto* initialMeshRotationRate = Internal::Find_ModuleData<InitialMeshRotationRateModuleData>(
        emitter,
        AuthoringModuleType::InitialMeshRotationRate
    ))
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

    const Shared<EffectEditorPreviewPlane> previewPlane =
        EffectEditorPreviewPlane::Create(_device, _context);
    CHECK_NULL(previewPlane, E_FAIL);
    CHECK_FAILED(previewPlane->Initialize(nullptr), E_FAIL);
    CHECK_FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), layerTag, previewPlane), E_FAIL);

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
    _trailPreviewController = trailPreviewController;

    return S_OK;
}

void Level_EffectEditor::Update_PreviewTrailStrokes(float timeDelta)
{
    if (EDITOR->Get_TrailPreviewGateMode() != TrailPreviewGateMode::Window ||
        !EDITOR->Is_TrailPreviewVisible() ||
        _currentPreviewTrailDefinition == nullptr)
    {
        Clear_PreviewTrailStrokes();
        _wasTrailPreviewGateOpen = false;
        _hasTrailPreviewSourceRatio = false;
        return;
    }

    const float gateStartRatio = EDITOR->Get_TrailPreviewGateStartRatio();
    const float gateEndRatio = EDITOR->Get_TrailPreviewGateEndRatio();
    if (fabs(gateEndRatio - gateStartRatio) <= 0.0001f)
    {
        Drain_ActivePreviewTrailStrokes();
        _wasTrailPreviewGateOpen = false;
        _hasTrailPreviewSourceRatio = false;
    }
    else
    {
        float sourceRatio = 0.f;
        const Shared<TrailPreviewController> trailPreviewController = _trailPreviewController.lock();
        const bool hasSourceRatio =
            trailPreviewController != nullptr &&
            trailPreviewController->Try_GetAttackAnimationRatio(sourceRatio);

        if (!hasSourceRatio)
        {
            Drain_ActivePreviewTrailStrokes();
            _wasTrailPreviewGateOpen = false;
            _hasTrailPreviewSourceRatio = false;
        }
        else
        {
            const bool gateOpen =
                gateStartRatio < gateEndRatio
                ? sourceRatio >= gateStartRatio && sourceRatio < gateEndRatio
                : sourceRatio >= gateStartRatio || sourceRatio < gateEndRatio;
            if (gateOpen && !_wasTrailPreviewGateOpen)
            {
                if (FAILED(Start_PreviewTrailStroke()))
                    LOG_WARN("[EffectEditor] Failed to start trail preview stroke.");
            }
            else if (!gateOpen && _wasTrailPreviewGateOpen)
                Drain_ActivePreviewTrailStrokes();

            _wasTrailPreviewGateOpen = gateOpen;
            _lastTrailPreviewSourceRatio = sourceRatio;
            _hasTrailPreviewSourceRatio = true;
        }
    }

    const float safeTimeDelta = max(0.f, timeDelta);
    for (int32 runtimeIndex = static_cast<int32>(_previewTrailStrokes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
    {
        PreviewTrailStrokeRuntime& runtime = _previewTrailStrokes[runtimeIndex];
        if (!runtime.isDraining)
            continue;

        runtime.drainElapsed += safeTimeDelta;
        if (runtime.drainElapsed < runtime.drainDuration)
            continue;

        EffectInstancePool* effectPool = CLIENT ? CLIENT->Get_EffectInstancePool() : nullptr;
        if (effectPool != nullptr)
            effectPool->Release(runtime.poolHandle);

        _previewTrailStrokes.erase(_previewTrailStrokes.begin() + runtimeIndex);
    }
}

HRESULT Level_EffectEditor::Start_PreviewTrailStroke()
{
    CHECK_NULL(_currentPreviewTrailDefinition, E_FAIL);

    EffectInstancePool* effectPool = CLIENT ? CLIENT->Get_EffectInstancePool() : nullptr;
    CHECK_NULL(effectPool, E_FAIL);

    PreviewTrailStrokeRuntime runtime{};
    runtime.sampleEnabled = make_shared<bool>(true);
    runtime.sampleProvider = make_shared<Internal::TrailPreviewStrokeSampleProvider>(
        _trailPreviewSampleProvider,
        runtime.sampleEnabled
    );
    CHECK_NULL(runtime.sampleProvider, E_FAIL);
    runtime.drainDuration = Internal::Resolve_TrailStrokeDrainDuration(_currentPreviewTrailDefinition);

    EffectTrailSample initialSample{};
    if (runtime.sampleProvider->Try_GetTrailSample(initialSample))
    {
        EffectInstancePool::AcquireDesc acquireDesc{};
        acquireDesc.worldPosition = initialSample.baseWorldPosition;
        acquireDesc.effectName = Internal::kPreviewTrailStrokePoolKey;
        acquireDesc.definition = _currentPreviewTrailDefinition;
        acquireDesc.layerLevelIndex = ETOI(LevelType::Static);
        acquireDesc.layerTag = kPreviewLayerTag;
        acquireDesc.trailSampleProvider = runtime.sampleProvider;

        CHECK_FAILED(effectPool->Acquire(acquireDesc, runtime.poolHandle), E_FAIL);
        _previewTrailStrokes.push_back(std::move(runtime));
        return S_OK;
    }

    EffectInstancePool::AcquireDesc acquireDesc{};
    acquireDesc.effectName = Internal::kPreviewTrailStrokePoolKey;
    acquireDesc.definition = _currentPreviewTrailDefinition;
    acquireDesc.layerLevelIndex = ETOI(LevelType::Static);
    acquireDesc.layerTag = kPreviewLayerTag;
    acquireDesc.trailSampleProvider = runtime.sampleProvider;

    CHECK_FAILED(effectPool->Acquire(acquireDesc, runtime.poolHandle), E_FAIL);
    _previewTrailStrokes.push_back(std::move(runtime));
    return S_OK;
}

void Level_EffectEditor::Begin_PreviewTrailStrokeDrain(PreviewTrailStrokeRuntime& runtime)
{
    if (runtime.isDraining)
        return;

    if (runtime.sampleEnabled != nullptr)
        *runtime.sampleEnabled = false;

    runtime.isDraining = true;
    runtime.drainElapsed = 0.f;
}

void Level_EffectEditor::Drain_ActivePreviewTrailStrokes()
{
    for (PreviewTrailStrokeRuntime& runtime : _previewTrailStrokes)
        Begin_PreviewTrailStrokeDrain(runtime);
}

void Level_EffectEditor::Clear_PreviewTrailStrokes()
{
    EffectInstancePool* effectPool = CLIENT ? CLIENT->Get_EffectInstancePool() : nullptr;

    if (effectPool != nullptr)
    {
        for (const PreviewTrailStrokeRuntime& runtime : _previewTrailStrokes)
            effectPool->Release(runtime.poolHandle);
    }

    _previewTrailStrokes.clear();
    _wasTrailPreviewGateOpen = false;
    _lastTrailPreviewSourceRatio = 0.f;
    _hasTrailPreviewSourceRatio = false;
}

void Level_EffectEditor::Clear_PreviewObjects()
{
    EffectInstancePool* effectPool = CLIENT ? CLIENT->Get_EffectInstancePool() : nullptr;
    if (nullptr == effectPool)
        return;

    Clear_PreviewTrailStrokes();
    effectPool->Release(_previewPoolHandle);
    _previewPoolHandle = {};
    effectPool->Discard_Inactive(Internal::kPreviewPoolKey);
    effectPool->Discard_Inactive(Internal::kPreviewTrailStrokePoolKey);
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
