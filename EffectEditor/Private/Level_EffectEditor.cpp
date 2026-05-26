#include "Level_EffectEditor.h"

#include "Camera.h"
#include "EffectEditorCamera.h"
#include "EffectEditorInstance.h"
#include "EffectEditorSceneGrid.h"
#include "EffectEditorSkybox.h"
#include "EffectInstance.h"
#include "Emitter_View.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "TrailPreviewController.h"
#include "TrailPreviewPlayerObject.h"
#include "TrailPreviewWeaponObject.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
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

    EffectSpriteScreenAlignment Resolve_EffectSpriteScreenAlignment(EmitterScreenAlignment alignment)
    {
        switch (alignment)
        {
        case EmitterScreenAlignment::Rectangle:
            return EffectSpriteScreenAlignment::Rectangle;

        case EmitterScreenAlignment::Square:
            return EffectSpriteScreenAlignment::Square;

        case EmitterScreenAlignment::AwayFromCenter:
            return EffectSpriteScreenAlignment::AwayFromCenter;

        case EmitterScreenAlignment::Velocity:
            return EffectSpriteScreenAlignment::Velocity;

        case EmitterScreenAlignment::FacingCameraPosition:
        case EmitterScreenAlignment::TypeSpecific:
        case EmitterScreenAlignment::FacingCameraDistanceBlend:
        default:
            return EffectSpriteScreenAlignment::FacingCameraPosition;
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

        return fallbackValue;
    }

    Vec2 Evaluate_Vector2DistributionMax(const Vector2DistributionData& data, const Vec2& fallbackValue)
    {
        if (const auto* constant = get_if<ConstantVector2DistributionData>(&data.payload))
            return constant->value;

        if (const auto* uniform = get_if<UniformVector2DistributionData>(&data.payload))
            return uniform->maxValue;

        return fallbackValue;
    }

    Vec3 Evaluate_Vector3DistributionMin(const Vector3DistributionData& data, const Vec3& fallbackValue)
    {
        if (const auto* constant = get_if<ConstantVector3DistributionData>(&data.payload))
            return constant->value;

        if (const auto* uniform = get_if<UniformVector3DistributionData>(&data.payload))
            return uniform->minValue;

        return fallbackValue;
    }

    Vec3 Evaluate_Vector3DistributionMax(const Vector3DistributionData& data, const Vec3& fallbackValue)
    {
        if (const auto* constant = get_if<ConstantVector3DistributionData>(&data.payload))
            return constant->value;

        if (const auto* uniform = get_if<UniformVector3DistributionData>(&data.payload))
            return uniform->maxValue;

        return fallbackValue;
    }

    Color Evaluate_ColorDistributionMin(const ColorDistributionData& data, const Color& fallbackValue)
    {
        if (const auto* constant = get_if<ConstantColorDistributionData>(&data.payload))
            return Clamp_PreviewColor(constant->value);

        if (const auto* uniform = get_if<UniformColorDistributionData>(&data.payload))
            return Clamp_PreviewColor(uniform->minValue);

        return Clamp_PreviewColor(fallbackValue);
    }

    Color Evaluate_ColorDistributionMax(const ColorDistributionData& data, const Color& fallbackValue)
    {
        if (const auto* constant = get_if<ConstantColorDistributionData>(&data.payload))
            return Clamp_PreviewColor(constant->value);

        if (const auto* uniform = get_if<UniformColorDistributionData>(&data.payload))
            return Clamp_PreviewColor(uniform->maxValue);

        return Clamp_PreviewColor(fallbackValue);
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

    CHECK_FAILED(Restart_PreviewFromDefinition(previewDefinition), E_FAIL);
    _currentPreviewDefinition = previewDefinition;

    for (const AuthoringEmitter& emitter : emitterView->Get_Emitters())
    {
        if (emitter.enabled)
            emitterView->Clear_PreviewDirty(emitter.id);
    }

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

    Clear_PreviewObjects();

    EffectInstanceDesc previewDesc{};
    previewDesc.definition = previewDefinition;

    const Shared<EffectInstance> previewPrototype = EffectInstance::Create(_device, _context);
    CHECK_NULL(previewPrototype, E_FAIL);

    const Shared<GameObject> previewObject = previewPrototype->Clone(&previewDesc);
    CHECK_NULL(previewObject, E_FAIL);

    CHECK_FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), kPreviewLayerTag, previewObject), E_FAIL);

    vector<Shared<GameObject>> restartedPreviewObjects{};
    restartedPreviewObjects.push_back(previewObject);

    _previewObjects = move(restartedPreviewObjects);

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

    ComputeSpriteEmitterDesc desc{};
    desc.drawMode = PointParticleDrawMode::DrawIndexedInstancedIndirect;

    if (const auto* required = Internal::Find_ModuleData<RequiredModuleData>(emitter, AuthoringModuleType::Required))
    {
        const EffectMaterialInstanceData& material = required->material;

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
        desc.required.material.noiseStrength = material.noiseStrength;
        desc.required.material.alphaCutoff = material.alphaCutoff;
        desc.required.material.alphaErosion = material.alphaErosion;
        desc.required.material.mainUVScale = material.mainUVScale;
        desc.required.material.mainUVScrollSpeed = material.mainUVScrollSpeed;
        desc.required.material.mainUVTilingMode = material.mainUVTilingMode;
        desc.required.material.noiseUVScale = material.noiseUVScale;
        desc.required.material.noiseUVScrollSpeed = material.noiseUVScrollSpeed;
        desc.required.material.noiseUVTilingMode = material.noiseUVTilingMode;
        desc.required.material.maskUVScale = material.maskUVScale;
        desc.required.material.maskUVScrollSpeed = material.maskUVScrollSpeed;
        desc.required.material.maskUVTilingMode = material.maskUVTilingMode;
        desc.required.material.twoSided = material.twoSided;
        desc.required.material.subUVRows = max(1u, material.subUVRows);
        desc.required.material.subUVCols = max(1u, material.subUVCols);

        desc.required.spriteRender.screenAlignment = Internal::Resolve_EffectSpriteScreenAlignment(required->screenAlignment);
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
        const Color minEndColor = Internal::Evaluate_ColorDistributionMin(
            colorOverLife->colorOverLife,
            Color{ desc.colorOverLife.endColorMin.x, desc.colorOverLife.endColorMin.y, desc.colorOverLife.endColorMin.z, desc.colorOverLife.endColorMin.w }
        );
        const Color maxEndColor = Internal::Evaluate_ColorDistributionMax(
            colorOverLife->colorOverLife,
            Color{ desc.colorOverLife.endColorMax.x, desc.colorOverLife.endColorMax.y, desc.colorOverLife.endColorMax.z, desc.colorOverLife.endColorMax.w }
        );
        const float minEndAlpha = clamp(
            Internal::Evaluate_FloatDistributionMin(colorOverLife->alphaOverLife, desc.colorOverLife.endColorMin.w),
            0.f,
            1.f
        );
        const float maxEndAlpha = clamp(
            Internal::Evaluate_FloatDistributionMax(colorOverLife->alphaOverLife, desc.colorOverLife.endColorMax.w),
            0.f,
            1.f
        );

        desc.colorOverLife.endColorMin = Vec4{ minEndColor.R(), minEndColor.G(), minEndColor.B(), minEndAlpha };
        desc.colorOverLife.endColorMax = Vec4{ maxEndColor.R(), maxEndColor.G(), maxEndColor.B(), maxEndAlpha };
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
    {
        desc.sizeByLife.enabled = true;
        desc.sizeByLife.multiplyXStart = sizeByLife->multiplyXStart;
        desc.sizeByLife.multiplyXEnd = sizeByLife->multiplyXEnd;
        desc.sizeByLife.multiplyYStart = sizeByLife->multiplyYStart;
        desc.sizeByLife.multiplyYEnd = sizeByLife->multiplyYEnd;
    }

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
        desc.rotation.rotationOverLifeDegrees = Vec2{ rotationOverLife->rotationStartDegrees, rotationOverLife->rotationEndDegrees };
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
        desc.rotation.rotationRateScaleByLife = Vec2{ rotationRateScaleByLife->scaleStart, rotationRateScaleByLife->scaleEnd };
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

    ComputeTrailEmitterDesc emitterDesc{};

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
        emitterDesc.material.mainTextureGuid = material.mainTextureGuid;
        emitterDesc.material.mainTexturePath = material.mainTexturePath;
        emitterDesc.material.noiseTextureGuid = material.noiseTextureGuid;
        emitterDesc.material.noiseTexturePath = material.noiseTexturePath;
        emitterDesc.material.maskTextureGuid = material.maskTextureGuid;
        emitterDesc.material.maskTexturePath = material.maskTexturePath;
        emitterDesc.material.blendMode = material.blendMode;
        emitterDesc.material.opacitySource = material.opacitySource.empty() ? "Alpha" : material.opacitySource;
        emitterDesc.material.tint = material.tint;
        emitterDesc.material.intensity = material.intensity;
        emitterDesc.material.opacityPower = material.opacityPower;
        emitterDesc.material.noiseStrength = material.noiseStrength;
        emitterDesc.material.alphaCutoff = material.alphaCutoff;
        emitterDesc.material.alphaErosion = material.alphaErosion;
        emitterDesc.material.mainUVScale = material.mainUVScale;
        emitterDesc.material.mainUVScrollSpeed = material.mainUVScrollSpeed;
        emitterDesc.material.mainUVTilingMode = material.mainUVTilingMode;
        emitterDesc.material.noiseUVScale = material.noiseUVScale;
        emitterDesc.material.noiseUVScrollSpeed = material.noiseUVScrollSpeed;
        emitterDesc.material.noiseUVTilingMode = material.noiseUVTilingMode;
        emitterDesc.material.maskUVScale = material.maskUVScale;
        emitterDesc.material.maskUVScrollSpeed = material.maskUVScrollSpeed;
        emitterDesc.material.maskUVTilingMode = material.maskUVTilingMode;
        emitterDesc.material.twoSided = material.twoSided;

        emitterDesc.playback.duration = max(0.0001f, required->duration);
        emitterDesc.playback.loopCount = required->loopCount;
        emitterDesc.playback.delay = max(0.f, required->delay);
        emitterDesc.playback.delayFirstLoopOnly = required->delayFirstLoopOnly;
        definition.localPosition = required->emitterOrigin;
        definition.localRotationDegrees = required->emitterRotationDegrees;
        definition.useLocalSpace = required->useLocalSpace;
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
        const Color minEndColor = Internal::Evaluate_ColorDistributionMin(
            colorOverLife->colorOverLife,
            Color{
                emitterDesc.colorOverLife.endColorMin.x,
                emitterDesc.colorOverLife.endColorMin.y,
                emitterDesc.colorOverLife.endColorMin.z,
                emitterDesc.colorOverLife.endColorMin.w
            }
        );
        const Color maxEndColor = Internal::Evaluate_ColorDistributionMax(
            colorOverLife->colorOverLife,
            Color{
                emitterDesc.colorOverLife.endColorMax.x,
                emitterDesc.colorOverLife.endColorMax.y,
                emitterDesc.colorOverLife.endColorMax.z,
                emitterDesc.colorOverLife.endColorMax.w
            }
        );
        const float minEndAlpha = clamp(
            Internal::Evaluate_FloatDistributionMin(colorOverLife->alphaOverLife, emitterDesc.colorOverLife.endColorMin.w),
            0.f,
            1.f
        );
        const float maxEndAlpha = clamp(
            Internal::Evaluate_FloatDistributionMax(colorOverLife->alphaOverLife, emitterDesc.colorOverLife.endColorMax.w),
            0.f,
            1.f
        );

        emitterDesc.colorOverLife.endColorMin = Vec4{ minEndColor.R(), minEndColor.G(), minEndColor.B(), minEndAlpha };
        emitterDesc.colorOverLife.endColorMax = Vec4{ maxEndColor.R(), maxEndColor.G(), maxEndColor.B(), maxEndAlpha };
    }

    if (const auto* sizeByLife = Internal::Find_ModuleData<SizeByLifeModuleData>(emitter, AuthoringModuleType::SizeByLife))
    {
        emitterDesc.sizeByLife.enabled = true;
        emitterDesc.sizeByLife.multiplyXStart = sizeByLife->multiplyXStart;
        emitterDesc.sizeByLife.multiplyXEnd = sizeByLife->multiplyXEnd;
        emitterDesc.sizeByLife.multiplyYStart = sizeByLife->multiplyYStart;
        emitterDesc.sizeByLife.multiplyYEnd = sizeByLife->multiplyYEnd;
    }

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
    CHECK_FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), layerTag, previewCamera), E_FAIL);

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
    CHECK_FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), layerTag, sceneGrid), E_FAIL);

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
    CHECK_FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), layerTag, skybox), E_FAIL);

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
    CHECK_FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), layerTag, trailPreviewPlayer), E_FAIL);

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
    CHECK_FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), layerTag, trailPreviewController), E_FAIL);

    return S_OK;
}

void Level_EffectEditor::Clear_PreviewObjects()
{
    for (Shared<GameObject>& previewObject : _previewObjects)
    {
        if (previewObject != nullptr)
            GAME->Remove_GameObject(ETOI(LevelType::GamePlay), previewObject);
    }

    _previewObjects.clear();
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
