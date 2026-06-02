#include "MeshEmitter.h"

#include "EffectMaterialScalarModulationRuntime.h"
#include "GameInstance.h"
#include "ModelCom.h"
#include "Renderer_Define.h"
#include "ShaderCom.h"
#include "Texture.h"

namespace Internal
{
constexpr double kMeshEmitterInitializeInfoThresholdMs = 8.0;
constexpr double kMeshEmitterReloadReuseInfoThresholdMs = 4.0;

float Read_Vec4Component(const Vec4& values, uint32 index)
{
    switch (index)
    {
    case 0: return values.x;
    case 1: return values.y;
    case 2: return values.z;
    case 3: return values.w;
    default: return values.x;
    }
}

bool Has_NonZeroVector(const Vec3& value)
{
    return fabsf(value.x) > 0.0001f || fabsf(value.y) > 0.0001f || fabsf(value.z) > 0.0001f;
}

double Elapsed_Milliseconds(const std::chrono::steady_clock::time_point& begin, const std::chrono::steady_clock::time_point& end)
{
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

Matrix Resolve_MeshPlaneAlignmentMatrix(EffectScreenAlignment alignment)
{
    switch (alignment)
    {
    case EffectScreenAlignment::WorldPlaneXZ:
        return Matrix::CreateRotationX(-XM_PIDIV2);

    case EffectScreenAlignment::WorldPlaneXY:
    default:
        return Matrix::Identity;
    }
}
}

IMPLEMENT_REFLECTION(MeshEmitter)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "MeshEmitter";
    info.category = "EffectEmitter";
    return true;
}

MeshEmitter::MeshEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : EffectEmitter{ device, context }
{
}

MeshEmitter::MeshEmitter(const MeshEmitter& prototype)
    : EffectEmitter{ prototype }
{
}

HRESULT MeshEmitter::Initialize_Prototype()
{
    return S_OK;
}

HRESULT MeshEmitter::Initialize(void* arg)
{
    if (nullptr != arg)
        _desc = *static_cast<MeshEmitterDesc*>(arg);

    CHECK_FAILED(__super::Initialize(&_desc), E_FAIL);
    Log_DescPayload("initialize");
    Reset_PlaybackRuntime();

    if (FAILED(Ready_Components()))
    {
        LOG_WARN(
            "MeshEmitter disabled because model resources are not ready. emitter='{}', modelGuid='{}', modelPath='{}'",
            _emitterName,
            _desc.modelGuid,
            _desc.modelPath
        );
        _isReady = false;
        return S_OK;
    }

    _isReady = true;
    const uint32 instanceCapacity = max(1u, _desc.spawn.instanceCount);
    _particles.assign(instanceCapacity, MeshParticleState{});
    _instancePayload.resize(instanceCapacity);
    Update_InstanceBuffer();
    return S_OK;
}

void MeshEmitter::Update(float timeDelta)
{
    if (!_isReady)
        return;

    Sync_FromEffectOwner();
    Advance_Playback(timeDelta);
}

void MeshEmitter::Late_Update(float)
{
    if (!Can_SubmitRender())
        return;

    GAME->Add_RenderGroup(Resolve_RenderGroup(), GetSharedPtr<GameObject>());
}

HRESULT MeshEmitter::Render()
{
    if (!Can_SubmitRender())
        return S_OK;

    if (FAILED(GAME->Bind_CameraCB(_shader)))
    {
        LOG_WARN_THROTTLED(
            60,
            "MeshEmitter render failed: camera binding failed. emitter='{}', modelGuid='{}', modelPath='{}'",
            _emitterName,
            _desc.modelGuid,
            _desc.modelPath
        );
        return E_FAIL;
    }

    const Matrix identity = Matrix::Identity;
    if (FAILED(_shader->Bind_Matrix("g_PreTransformMatrix", &identity)))
    {
        LOG_WARN_THROTTLED(
            60,
            "MeshEmitter render failed: pre-transform binding failed. emitter='{}', modelGuid='{}', modelPath='{}'",
            _emitterName,
            _desc.modelGuid,
            _desc.modelPath
        );
        return E_FAIL;
    }

    if (Is_DistortionFamily())
        CHECK_FAILED_THROTTLED(Bind_DistortionResources(), 60, E_FAIL);
    else
        CHECK_FAILED_THROTTLED(Bind_EffectMaterialResources(), 60, E_FAIL);

    const uint32 passIndex = Resolve_ShaderPassIndex();
    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
    {
        const uint32 materialIndex = _model->Get_MeshMaterialIndex(meshIndex);

        if (FAILED(_shader->Begin(passIndex)))
        {
            LOG_WARN_THROTTLED(
                60,
                "MeshEmitter draw failed: shader pass begin failed. emitter='{}', meshIndex={}, materialIndex={}, passIndex={}",
                _emitterName,
                meshIndex,
                materialIndex,
                passIndex
            );
            return E_FAIL;
        }

        if (FAILED(_model->RenderInstanced(meshIndex, _instanceBuffer.Get(), sizeof(MeshInstanceVertex), _activeInstanceCount)))
        {
            LOG_WARN_THROTTLED(
                60,
                "MeshEmitter draw failed: instanced mesh render failed. emitter='{}', meshIndex={}, materialIndex={}, passIndex={}",
                _emitterName,
                meshIndex,
                materialIndex,
                passIndex
            );
            return E_FAIL;
        }
    }

    return S_OK;
}

HRESULT MeshEmitter::Reset_ForEffectReplay()
{
    Reset_PlaybackRuntime();
    for (MeshParticleState& particle : _particles)
        particle = {};
    Update_InstanceBuffer();
    return S_OK;
}

bool MeshEmitter::Can_ReloadDefinition(const EffectEmitterDefinition& emitterDefinition) const
{
    if (emitterDefinition.kind != EffectEmitterKind::Mesh)
        return false;

    const MeshEmitterDesc* desc = get_if<MeshEmitterDesc>(&emitterDefinition.concreteDesc);
    return desc != nullptr && Is_CompatibleReloadDesc(*desc);
}

HRESULT MeshEmitter::Reload_Definition(const EffectEmitterDefinition& emitterDefinition)
{
    if (!Can_ReloadDefinition(emitterDefinition))
        return E_FAIL;

    const MeshEmitterDesc* desc = get_if<MeshEmitterDesc>(&emitterDefinition.concreteDesc);
    CHECK_NULL(desc, E_FAIL);

    _emitterId = emitterDefinition.id;
    _emitterName = emitterDefinition.name;
    _localPosition = emitterDefinition.localPosition;
    _localRotationDegrees = emitterDefinition.localRotationDegrees;
    _useLocalSpace = emitterDefinition.useLocalSpace;
    _enabled = emitterDefinition.enabled;

    CHECK_FAILED(Apply_DescForReload(*desc), E_FAIL);
    Sync_FromEffectOwner();

    LOG_INFO(
        "MeshEmitter reused for definition reload. emitter='{}', modelGuid='{}', modelPath='{}', capacity={}",
        _emitterName,
        _desc.modelGuid,
        _desc.modelPath,
        _instanceCapacity
    );
    return S_OK;
}

bool MeshEmitter::Is_EffectFinished() const
{
    return !_isReady || (PlaybackState::Completed == _playbackState && _activeInstanceCount == 0u);
}

bool MeshEmitter::Try_Get_BlendSortWorldPosition(Vec3& outWorldPosition) const
{
    if (_transformCom == nullptr)
        return false;

    bool hasActiveParticle = false;
    Vec3 accumulatedPosition{};
    uint32 activeParticleCount = 0u;

    for (const MeshParticleState& particle : _particles)
    {
        if (!particle.active)
            continue;

        accumulatedPosition += particle.position;
        ++activeParticleCount;
        hasActiveParticle = true;
    }

    if (hasActiveParticle && activeParticleCount > 0u)
    {
        outWorldPosition = accumulatedPosition / static_cast<float>(activeParticleCount);
        return true;
    }

    outWorldPosition = _transformCom->Get_WorldPosition();
    return true;
}

void MeshEmitter::On_EffectTransformSynced()
{
    if (_activeInstanceCount > 0u)
        Update_InstanceBuffer();
}

void MeshEmitter::Log_DescPayload(const char* context) const
{
    LOG_DEBUG(
        "MeshEmitter desc payload. context={}, emitter='{}', spawnCount={}, spawnRate={}, bursts={}, life=({}, {}), meshAlignment={}, meshInitialScale={}, meshScaleByLife={}, motion={}, acceleration={}, drag=({}, {}), velocityScale=({}, {}), meshRotation={}, meshRotationMin=({}, {}, {}), meshRotationMax=({}, {}, {}), colorCurve={}, alphaCurve={}",
        context,
        _emitterName,
        _desc.spawn.instanceCount,
        _desc.spawn.particleSpawn.spawnRate,
        static_cast<uint32>(_desc.spawn.particleSpawn.bursts.size()),
        _desc.lifetime.lifeTime.x,
        _desc.lifetime.lifeTime.y,
        static_cast<uint32>(_desc.meshTransform.alignment),
        _desc.meshTransform.initialScaleEnabled,
        _desc.meshTransform.scaleByLife.enabled,
        _desc.motion.enabled,
        Internal::Has_NonZeroVector(_desc.motion.accelerationMin) ||
        Internal::Has_NonZeroVector(_desc.motion.accelerationMax),
        _desc.motion.drag.x,
        _desc.motion.drag.y,
        _desc.motion.velocityScaleByLife.x,
        _desc.motion.velocityScaleByLife.y,
        _desc.meshTransform.rotationEnabled,
        _desc.meshTransform.initialRotationDegreesMin.x,
        _desc.meshTransform.initialRotationDegreesMin.y,
        _desc.meshTransform.initialRotationDegreesMin.z,
        _desc.meshTransform.initialRotationDegreesMax.x,
        _desc.meshTransform.initialRotationDegreesMax.y,
        _desc.meshTransform.initialRotationDegreesMax.z,
        _desc.colorOverLife.curve.colorCurveEnabled,
        _desc.colorOverLife.curve.alphaCurveEnabled
    );
}

void MeshEmitter::Log_InitializeTiming(
    const char* context,
    double shaderMs,
    double resolveModelMs,
    double modelMs,
    double materialMs,
    double bufferMs,
    double totalMs) const
{
    const bool isReloadReuse = std::strcmp(context, "reload_reuse") == 0;
    const double infoThresholdMs = isReloadReuse
                                   ? Internal::kMeshEmitterReloadReuseInfoThresholdMs
                                   : Internal::kMeshEmitterInitializeInfoThresholdMs;

    if (totalMs >= infoThresholdMs)
    {
        LOG_INFO(
            "MeshEmitter initialize timing. context={}, emitter='{}', shader_ms={:.3f}, resolve_model_ms={:.3f}, model_ms={:.3f}, material_ms={:.3f}, buffer_ms={:.3f}, total_ms={:.3f}, modelGuid='{}', modelPath='{}'",
            context,
            _emitterName,
            shaderMs,
            resolveModelMs,
            modelMs,
            materialMs,
            bufferMs,
            totalMs,
            _desc.modelGuid,
            _desc.modelPath
        );
    }
    else
    {
        LOG_DEBUG(
            "MeshEmitter initialize timing. context={}, emitter='{}', shader_ms={:.3f}, resolve_model_ms={:.3f}, model_ms={:.3f}, material_ms={:.3f}, buffer_ms={:.3f}, total_ms={:.3f}, modelGuid='{}', modelPath='{}'",
            context,
            _emitterName,
            shaderMs,
            resolveModelMs,
            modelMs,
            materialMs,
            bufferMs,
            totalMs,
            _desc.modelGuid,
            _desc.modelPath
        );
    }
}

bool MeshEmitter::Is_CompatibleReloadDesc(const MeshEmitterDesc& desc) const
{
    if (!_isReady || _model == nullptr || _shader == nullptr)
        return false;
    if (desc.modelGuid != _desc.modelGuid || desc.modelPath != _desc.modelPath)
        return false;
    if (desc.useModelMaterials != _desc.useModelMaterials)
        return false;
    if (Has_MaterialOverride(_desc) && !Has_MaterialOverride(desc))
        return false;
    if (desc.meshTransform.alignment != _desc.meshTransform.alignment)
        return false;

    return true;
}

bool MeshEmitter::Has_MaterialOverride(const MeshEmitterDesc& desc) const
{
    return
        !desc.material.mainTextureGuid.empty() ||
        !desc.material.mainTexturePath.empty() ||
        !desc.material.noiseTextureGuid.empty() ||
        !desc.material.noiseTexturePath.empty() ||
        !desc.material.maskTextureGuid.empty() ||
        !desc.material.maskTexturePath.empty();
}

HRESULT MeshEmitter::Apply_DescForReload(const MeshEmitterDesc& desc)
{
    const auto totalBegin = std::chrono::steady_clock::now();
    _desc = desc;
    Reset_PlaybackRuntime();

    const auto shaderBegin = std::chrono::steady_clock::now();
    CHECK_FAILED(Ready_Shader(), E_FAIL);
    const auto shaderEnd = std::chrono::steady_clock::now();

    const auto materialBegin = std::chrono::steady_clock::now();
    CHECK_FAILED(Apply_EffectMaterialOverride(), E_FAIL);
    const auto materialEnd = std::chrono::steady_clock::now();

    const auto bufferBegin = std::chrono::steady_clock::now();
    CHECK_FAILED(Ready_InstanceBuffer(), E_FAIL);
    const auto bufferEnd = std::chrono::steady_clock::now();

    const uint32 instanceCapacity = max(1u, _desc.spawn.instanceCount);
    if (_particles.size() != instanceCapacity)
        _particles.assign(instanceCapacity, MeshParticleState{});
    else
    {
        for (MeshParticleState& particle : _particles)
            particle = {};
    }

    _instancePayload.resize(instanceCapacity);
    _isReady = _model != nullptr && _shader != nullptr && _instanceBuffer != nullptr;
    Update_InstanceBuffer();

    const auto totalEnd = std::chrono::steady_clock::now();
    Log_InitializeTiming(
        "reload_reuse",
        Internal::Elapsed_Milliseconds(shaderBegin, shaderEnd),
        0.0,
        0.0,
        Internal::Elapsed_Milliseconds(materialBegin, materialEnd),
        Internal::Elapsed_Milliseconds(bufferBegin, bufferEnd),
        Internal::Elapsed_Milliseconds(totalBegin, totalEnd)
    );
    return _isReady ? S_OK : E_FAIL;
}

HRESULT MeshEmitter::Ready_Components()
{
    const auto totalBegin = std::chrono::steady_clock::now();

    const auto shaderBegin = std::chrono::steady_clock::now();
    CHECK_FAILED(Ready_Shader(), E_FAIL);
    const auto shaderEnd = std::chrono::steady_clock::now();

    const auto resolveModelBegin = std::chrono::steady_clock::now();
    const wstring modelPath = Resolve_ModelPath();
    const auto resolveModelEnd = std::chrono::steady_clock::now();
    if (modelPath.empty())
    {
        LOG_WARN(
            "MeshEmitter model missing: model path could not be resolved. emitter='{}', modelGuid='{}', modelPath='{}'",
            _emitterName,
            _desc.modelGuid,
            _desc.modelPath
        );
        return E_FAIL;
    }

    const auto modelBegin = std::chrono::steady_clock::now();
    _model = ModelCom::Create(
        _device,
        _context,
        ModelType::NonAnim,
        String::ToString(modelPath).c_str(),
        Matrix::CreateScale(0.01f)
    );
    if (!_model)
    {
        LOG_WARN(
            "MeshEmitter model missing: model load failed. emitter='{}', resolvedPath='{}', modelGuid='{}', modelPath='{}'",
            _emitterName,
            String::ToString(modelPath),
            _desc.modelGuid,
            _desc.modelPath
        );
        return E_FAIL;
    }
    const auto modelEnd = std::chrono::steady_clock::now();

    const auto materialBegin = std::chrono::steady_clock::now();
    CHECK_FAILED(Apply_EffectMaterialOverride(), E_FAIL);
    const auto materialEnd = std::chrono::steady_clock::now();

    const auto bufferBegin = std::chrono::steady_clock::now();
    CHECK_FAILED(Ready_InstanceBuffer(), E_FAIL);
    const auto bufferEnd = std::chrono::steady_clock::now();
    const auto totalEnd = std::chrono::steady_clock::now();

    Log_InitializeTiming(
        "initialize",
        Internal::Elapsed_Milliseconds(shaderBegin, shaderEnd),
        Internal::Elapsed_Milliseconds(resolveModelBegin, resolveModelEnd),
        Internal::Elapsed_Milliseconds(modelBegin, modelEnd),
        Internal::Elapsed_Milliseconds(materialBegin, materialEnd),
        Internal::Elapsed_Milliseconds(bufferBegin, bufferEnd),
        Internal::Elapsed_Milliseconds(totalBegin, totalEnd)
    );
    return S_OK;
}

HRESULT MeshEmitter::Ready_Shader()
{
    const wstring shaderId = String::ToWString(Is_DistortionFamily() ? kEffectMeshDistortionShaderId : kEffectMeshShaderId);
    if (nullptr != _shader && _loadedShaderPrototypeTag == shaderId)
        return S_OK;

    if (const Shared<ShaderCom> existingShader = Get_Component<ShaderCom>(shaderId))
    {
        _shader = existingShader;
        _loadedShaderPrototypeTag = shaderId;
        return S_OK;
    }

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), shaderId, _shader), E_FAIL);

    if (nullptr == _shader)
    {
        LOG_WARN(
            "MeshEmitter shader missing: effect mesh shader prototype is missing. emitter='{}', shaderId='{}'",
            _emitterName,
            String::ToString(shaderId)
        );
        return E_FAIL;
    }

    _loadedShaderPrototypeTag = shaderId;
    return S_OK;
}

HRESULT MeshEmitter::Apply_EffectMaterialOverride()
{
    return Ready_EffectMaterialTextures();
}

HRESULT MeshEmitter::Ready_EffectMaterialTextures()
{
    if (Is_DistortionFamily())
    {
        _texture.reset();
        _noiseTexture.reset();
        _maskTexture.reset();
        CHECK_FAILED(Ready_OptionalTexture(_desc.material.flowTextureGuid, _desc.material.flowTexturePath, _flowTexture), E_FAIL);
        _opacitySourceIndex = Resolve_OpacitySourceIndex();
        return S_OK;
    }

    _flowTexture.reset();

    wstring mainTexturePath = Resolve_MaterialTexturePath(_desc.material.mainTextureGuid, _desc.material.mainTexturePath);
    if (mainTexturePath.empty())
    {
        LOG_WARN_THROTTLED(
            60,
            "MeshEmitter assigned effect material has no resolvable main texture. emitter='{}', materialMainGuid='{}', materialMainPath='{}'. fallback='{}'",
            _emitterName,
            _desc.material.mainTextureGuid,
            _desc.material.mainTexturePath,
            kFallbackTexturePath
        );
        mainTexturePath = Resolve_MaterialTexturePath({}, kFallbackTexturePath);
    }

    if (mainTexturePath.empty())
        return E_FAIL;

    _texture = Texture::Create(_device, _context, mainTexturePath.c_str(), 1);
    CHECK_NULL(_texture, E_FAIL);

    CHECK_FAILED(Ready_OptionalTexture(_desc.material.noiseTextureGuid, _desc.material.noiseTexturePath, _noiseTexture), E_FAIL);
    CHECK_FAILED(Ready_OptionalTexture(_desc.material.maskTextureGuid, _desc.material.maskTexturePath, _maskTexture), E_FAIL);
    _opacitySourceIndex = Resolve_OpacitySourceIndex();
    return S_OK;
}

HRESULT MeshEmitter::Ready_OptionalTexture(const string& textureGuid, const string& texturePath, Shared<Texture>& outTexture)
{
    outTexture.reset();
    if (textureGuid.empty() && texturePath.empty())
        return S_OK;

    const wstring resolvedPath = Resolve_MaterialTexturePath(textureGuid, texturePath);
    if (resolvedPath.empty())
    {
        LOG_WARN_THROTTLED(
            60,
            "MeshEmitter optional effect material texture missing. emitter='{}', textureGuid='{}', texturePath='{}'",
            _emitterName,
            textureGuid,
            texturePath
        );
        return S_OK;
    }

    outTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    if (outTexture == nullptr)
    {
        LOG_WARN_THROTTLED(
            60,
            "MeshEmitter optional effect material texture create failed. emitter='{}', textureGuid='{}', texturePath='{}', resolvedPath='{}'",
            _emitterName,
            textureGuid,
            texturePath,
            String::ToString(resolvedPath)
        );
    }
    return S_OK;
}

HRESULT MeshEmitter::Bind_EffectMaterialResources()
{
    const float emitterPhase = Client::Compute_MaterialScalarModulationPhase(_emitterElapsedTime, _desc.playback.duration);
    const EffectRequiredMaterialRuntimeDesc material =
        Client::Resolve_EmitterTimeMaterialScalarModulation(_desc.material, emitterPhase);

    CHECK_NULL(_texture, E_FAIL);

    CHECK_FAILED_THROTTLED(_texture->Bind_ShaderResourceView(_shader.get(), "g_Texture", 0), 60, E_FAIL);

    if (_noiseTexture != nullptr)
        CHECK_FAILED_THROTTLED(_noiseTexture->Bind_ShaderResourceView(_shader.get(), "g_NoiseTexture", 0), 60, E_FAIL);
    else
        CHECK_FAILED_THROTTLED(_shader->Bind_SRV("g_NoiseTexture", nullptr), 60, E_FAIL);

    if (_maskTexture != nullptr)
        CHECK_FAILED_THROTTLED(_maskTexture->Bind_ShaderResourceView(_shader.get(), "g_MaskTexture", 0), 60, E_FAIL);
    else
        CHECK_FAILED_THROTTLED(_shader->Bind_SRV("g_MaskTexture", nullptr), 60, E_FAIL);

    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_Tint", &material.tint, sizeof(material.tint)), 60, E_FAIL);

    const Vec4 effectMeshParams = Vec4(
        material.intensity,
        material.opacityPower,
        material.noiseStrength,
        _noiseTexture != nullptr ? 1.f : 0.f
    );
    const Vec4 effectMeshAlphaParams = Vec4(
        clamp(material.alphaCutoff, 0.f, 1.f),
        clamp(material.alphaErosion, 0.f, 1.f),
        _maskTexture != nullptr ? 1.f : 0.f,
        0.f
    );
    const Vec4 effectMeshMainUVParams = Vec4(material.mainUVScale.x, material.mainUVScale.y, material.mainUVScrollSpeed.x, material.mainUVScrollSpeed.y);
    const Vec4 effectMeshNoiseUVParams = Vec4(material.noiseUVScale.x, material.noiseUVScale.y, material.noiseUVScrollSpeed.x, material.noiseUVScrollSpeed.y);
    const Vec4 effectMeshMaskUVParams = Vec4(material.maskUVScale.x, material.maskUVScale.y, material.maskUVScrollSpeed.x, material.maskUVScrollSpeed.y);
    const Vec4 effectMeshUVOffsetParams = Vec4(material.mainUVOffset.x, material.mainUVOffset.y, material.noiseUVOffset.x, material.noiseUVOffset.y);
    const Vec4 effectMeshMaskUVOffsetParams = Vec4(material.maskUVOffset.x, material.maskUVOffset.y, 0.f, 0.f);
    const Vec4 effectMeshUVModeParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.mainUVTilingMode)),
        static_cast<float>(static_cast<uint32>(material.noiseUVTilingMode)),
        static_cast<float>(static_cast<uint32>(material.maskUVTilingMode)),
        0.f
    );
    const EffectMaterialAdditiveContributionData& additive = material.additive;
    const EffectMaterialCoreEmissiveData& coreEmissive = material.coreEmissive;
    const Vec4 effectMeshAdditiveParams = Vec4(
        static_cast<float>(static_cast<uint32>(additive.colorSource)),
        static_cast<float>(static_cast<uint32>(additive.amountSource)),
        static_cast<float>(static_cast<uint32>(additive.coveragePolicy)),
        additive.intensityScale
    );
    const Vec4 effectMeshAdditiveFlags = Vec4(additive.blackNeutral ? 1.f : 0.f, 0.f, 0.f, 0.f);
    const Vec4 effectMeshCoreEmissiveParams = Vec4(
        coreEmissive.enabled ? 1.f : 0.f,
        coreEmissive.corePower,
        coreEmissive.coreIntensity,
        coreEmissive.outerPower
    );
    const Vec4 effectMeshCoreEmissiveColor = Vec4(
        coreEmissive.coreColor.x,
        coreEmissive.coreColor.y,
        coreEmissive.coreColor.z,
        coreEmissive.outerIntensity
    );

    CHECK_FAILED_THROTTLED(
        _shader->Bind_RawValue("g_EffectMeshParams", &effectMeshParams, sizeof(effectMeshParams)),
        60,
        E_FAIL
    );
    CHECK_FAILED_THROTTLED(
        _shader->Bind_RawValue("g_EffectMeshAlphaParams", &effectMeshAlphaParams, sizeof(effectMeshAlphaParams)),
        60,
        E_FAIL
    );
    CHECK_FAILED_THROTTLED(
        _shader->Bind_RawValue("g_EffectMeshMainUVParams", &effectMeshMainUVParams, sizeof(effectMeshMainUVParams)),
        60,
        E_FAIL
    );
    CHECK_FAILED_THROTTLED(
        _shader->Bind_RawValue("g_EffectMeshNoiseUVParams", &effectMeshNoiseUVParams, sizeof(effectMeshNoiseUVParams)),
        60,
        E_FAIL
    );
    CHECK_FAILED_THROTTLED(
        _shader->Bind_RawValue("g_EffectMeshMaskUVParams", &effectMeshMaskUVParams, sizeof(effectMeshMaskUVParams)),
        60,
        E_FAIL
    );
    CHECK_FAILED_THROTTLED(
        _shader->Bind_RawValue("g_EffectMeshUVOffsetParams", &effectMeshUVOffsetParams, sizeof(effectMeshUVOffsetParams)),
        60,
        E_FAIL
    );
    CHECK_FAILED_THROTTLED(
        _shader->Bind_RawValue("g_EffectMeshMaskUVOffsetParams", &effectMeshMaskUVOffsetParams, sizeof(effectMeshMaskUVOffsetParams)),
        60,
        E_FAIL
    );
    CHECK_FAILED_THROTTLED(
        _shader->Bind_RawValue("g_EffectMeshUVModeParams", &effectMeshUVModeParams, sizeof(effectMeshUVModeParams)),
        60,
        E_FAIL
    );
    CHECK_FAILED_THROTTLED(
        _shader->Bind_RawValue("g_EffectMeshMaterialTime", &_emitterElapsedTime, sizeof(_emitterElapsedTime)),
        60,
        E_FAIL
    );
    CHECK_FAILED_THROTTLED(
        _shader->Bind_RawValue("g_EffectMeshAdditiveParams", &effectMeshAdditiveParams, sizeof(effectMeshAdditiveParams)),
        60,
        E_FAIL
    );
    CHECK_FAILED_THROTTLED(
        _shader->Bind_RawValue("g_EffectMeshAdditiveEmissiveColor", &additive.emissiveColor, sizeof(additive.emissiveColor)),
        60,
        E_FAIL
    );
    CHECK_FAILED_THROTTLED(
        _shader->Bind_RawValue("g_EffectMeshAdditiveConstantColor", &additive.constantColor, sizeof(additive.constantColor)),
        60,
        E_FAIL
    );
    CHECK_FAILED_THROTTLED(
        _shader->Bind_RawValue("g_EffectMeshAdditiveFlags", &effectMeshAdditiveFlags, sizeof(effectMeshAdditiveFlags)),
        60,
        E_FAIL
    );
    CHECK_FAILED_THROTTLED(
        _shader->Bind_RawValue("g_EffectMeshCoreEmissiveParams", &effectMeshCoreEmissiveParams, sizeof(effectMeshCoreEmissiveParams)),
        60,
        E_FAIL
    );
    CHECK_FAILED_THROTTLED(
        _shader->Bind_RawValue("g_EffectMeshCoreEmissiveColor", &effectMeshCoreEmissiveColor, sizeof(effectMeshCoreEmissiveColor)),
        60,
        E_FAIL
    );
    CHECK_FAILED_THROTTLED(
        _shader->Bind_RawValue("g_OpacitySource", &_opacitySourceIndex, sizeof(_opacitySourceIndex)),
        60,
        E_FAIL
    );
    return S_OK;
}

HRESULT MeshEmitter::Bind_DistortionResources()
{
    const EffectRequiredMaterialRuntimeDesc& material = _desc.material;

    if (nullptr != _flowTexture)
        CHECK_FAILED_THROTTLED(_flowTexture->Bind_ShaderResourceView(_shader.get(), "g_FlowTexture", 0), 60, E_FAIL);
    else
        CHECK_FAILED_THROTTLED(_shader->Bind_SRV("g_FlowTexture", nullptr), 60, E_FAIL);

    D3D11_VIEWPORT viewport{};
    uint32 viewportCount = 1u;
    _context->RSGetViewports(&viewportCount, &viewport);

    const Vec4 distortionParams{
        max(0.f, material.refractionIntensity),
        max(0.f, material.refractionPresence),
        nullptr != _flowTexture ? 1.f : 0.f,
        _emitterElapsedTime
    };
    const Vec4 distortionScreenSize{ max(1.f, viewport.Width), max(1.f, viewport.Height), 0.f, 0.f };
    const float flowUVTilingMode = static_cast<float>(static_cast<uint32>(material.flowUVTilingMode));

    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_Tint", &material.tint, sizeof(material.tint)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectDistortionParams", &distortionParams, sizeof(distortionParams)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_DistortionScreenSize", &distortionScreenSize, sizeof(distortionScreenSize)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_FlowUVScale", &material.flowUVScale, sizeof(material.flowUVScale)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_FlowUVOffset", &material.flowUVOffset, sizeof(material.flowUVOffset)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_FlowUVScrollSpeed", &material.flowUVScrollSpeed, sizeof(material.flowUVScrollSpeed)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_FlowUVTilingMode", &flowUVTilingMode, sizeof(flowUVTilingMode)), 60, E_FAIL);
    return S_OK;
}

bool MeshEmitter::Is_DistortionFamily() const
{
    return _desc.material.materialFamily == EffectMaterialFamily::SpriteDistortion;
}

HRESULT MeshEmitter::Ready_InstanceBuffer()
{
    const uint32 instanceCapacity = max(1u, _desc.spawn.instanceCount);
    if (nullptr != _instanceBuffer && _instanceCapacity == instanceCapacity)
        return S_OK;

    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.ByteWidth = sizeof(MeshInstanceVertex) * instanceCapacity;
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    _instanceBuffer.Reset();
    _instanceCapacity = 0u;
    if (FAILED(_device->CreateBuffer(&bufferDesc, nullptr, _instanceBuffer.GetAddressOf())))
        return E_FAIL;

    _instanceCapacity = instanceCapacity;
    return S_OK;
}

void MeshEmitter::Update_InstanceBuffer()
{
    if (!_instanceBuffer || !_context || !_transformCom)
        return;

    _activeInstanceCount = 0u;
    const uint32 drawLimit = _desc.drawLimit.useMaxDrawCount
                             ? min(max(1u, _desc.drawLimit.maxDrawCount), static_cast<uint32>(_instancePayload.size()))
                             : static_cast<uint32>(_instancePayload.size());
    vector<const MeshParticleState*> sortedParticles{};
    sortedParticles.reserve(_particles.size());

    for (const MeshParticleState& particle : _particles)
    {
        if (!particle.active)
            continue;

        sortedParticles.push_back(&particle);
    }

    const Vec4* cameraPosition4 = Resolve_RenderGroup() == RenderGroup::Blend && GAME != nullptr
                                  ? GAME->Get_CamPosition()
                                  : nullptr;
    Vec3 localBoundsMin{};
    Vec3 localBoundsMax{};
    const bool hasLocalBounds = _model != nullptr && _model->Try_Get_LocalBounds(localBoundsMin, localBoundsMax);
    const Vec3 localBoundsExtent = hasLocalBounds ? (localBoundsMax - localBoundsMin) * 0.5f : Vec3{};
    const float localBoundsRadius = hasLocalBounds ? localBoundsExtent.Length() : 0.f;

    if (cameraPosition4 != nullptr && hasLocalBounds)
    {
        const Vec3 cameraPosition{ cameraPosition4->x, cameraPosition4->y, cameraPosition4->z };
        stable_sort(sortedParticles.begin(), sortedParticles.end(),
            [&](const MeshParticleState* lhs, const MeshParticleState* rhs)
            {
                const auto compute_particle_depth_sq = [&](const MeshParticleState& particle) -> float
                {
                    return (particle.position - cameraPosition).LengthSquared();
                };

                return compute_particle_depth_sq(*lhs) > compute_particle_depth_sq(*rhs);
            });
    }

    for (const MeshParticleState* particle : sortedParticles)
    {
        if (particle == nullptr || _activeInstanceCount >= drawLimit)
            continue;

        const float lifeProgress = clamp(particle->age / max(0.0001f, particle->lifeMax), 0.f, 1.f);
        MeshInstanceVertex& instance = _instancePayload[_activeInstanceCount++];
        instance.world = Build_InstanceMatrix(*particle);
        instance.color = Evaluate_ColorOverLife(*particle, lifeProgress);
        instance.lifeTime = Vec2{ particle->lifeMax, particle->age };
        instance.padding = Vec2{};
    }

    if (_activeInstanceCount == 0u)
        return;

    D3D11_MAPPED_SUBRESOURCE mappedResource{};
    if (FAILED(_context->Map(_instanceBuffer.Get(), 0u, D3D11_MAP_WRITE_DISCARD, 0u, &mappedResource)))
        return;

    memcpy(mappedResource.pData, _instancePayload.data(), sizeof(MeshInstanceVertex) * _activeInstanceCount);
    _context->Unmap(_instanceBuffer.Get(), 0u);
}

void MeshEmitter::Reset_PlaybackRuntime()
{
    _burstExecuted.assign(_desc.spawn.particleSpawn.bursts.size(), false);
    _emitterElapsedTime = 0.f;
    _spawnAccumulator = 0.f;
    _loopElapsedTime = 0.f;
    _loopIndex = 0u;
    _completedKillDispatched = false;
    _activeInstanceCount = 0u;
    _nextSpawnCursor = 0u;
    _playbackState = Resolve_CurrentLoopDelay() > 0.f ? PlaybackState::Delayed : PlaybackState::Playing;
}

void MeshEmitter::Advance_Playback(float timeDelta)
{
    const float safeDeltaTime = max(0.f, timeDelta);
    float simulationDeltaTime = 0.f;
    uint32 spawnRequest = 0u;

    if (PlaybackState::Completed == _playbackState)
    {
        if (_desc.playback.killOnCompleted && !_completedKillDispatched)
        {
            for (MeshParticleState& particle : _particles)
                particle.active = false;
            _completedKillDispatched = true;
        }
        else if (!_desc.playback.killOnCompleted)
            simulationDeltaTime = safeDeltaTime;

        Update_Particles(simulationDeltaTime);
        Update_InstanceBuffer();
        return;
    }

    float remainingTime = safeDeltaTime;
    while (remainingTime > 0.f && PlaybackState::Completed != _playbackState)
    {
        const float delay = Resolve_CurrentLoopDelay();
        if (_loopElapsedTime < delay)
        {
            _playbackState = PlaybackState::Delayed;
            const float delayStep = min(remainingTime, delay - _loopElapsedTime);
            _loopElapsedTime += delayStep;
            remainingTime -= delayStep;

            if (_loopElapsedTime < delay || remainingTime <= 0.f)
                break;
        }

        _playbackState = PlaybackState::Playing;

        const float duration = Resolve_Duration();
        const float currentPlaybackTime = max(0.f, _loopElapsedTime - delay);
        const float remainingPlaybackTime = max(0.f, duration - currentPlaybackTime);
        const float playbackStep = min(remainingTime, remainingPlaybackTime);

        if (playbackStep > 0.f)
        {
            simulationDeltaTime += playbackStep;
            spawnRequest += Consume_SpawnRequest(playbackStep);
            _loopElapsedTime += playbackStep;
            remainingTime -= playbackStep;
        }

        if (_loopElapsedTime - delay < duration)
            break;

        if (Has_NextLoop())
        {
            Start_NextLoop();
            continue;
        }

        _playbackState = PlaybackState::Completed;
        if (_desc.playback.killOnCompleted && !_completedKillDispatched)
        {
            for (MeshParticleState& particle : _particles)
                particle.active = false;
            _completedKillDispatched = true;
        }
        break;
    }

    if (PlaybackState::Completed == _playbackState && !_desc.playback.killOnCompleted)
        simulationDeltaTime += remainingTime;

    Spawn_Particles(spawnRequest);
    Update_Particles(simulationDeltaTime);
    Update_InstanceBuffer();
}

void MeshEmitter::Start_NextLoop()
{
    ++_loopIndex;
    _loopElapsedTime = 0.f;
    _emitterElapsedTime = 0.f;
    _spawnAccumulator = 0.f;
    _burstExecuted.assign(_desc.spawn.particleSpawn.bursts.size(), false);
    _playbackState = Resolve_CurrentLoopDelay() > 0.f ? PlaybackState::Delayed : PlaybackState::Playing;
}

float MeshEmitter::Resolve_CurrentLoopDelay() const
{
    if (_desc.playback.delayFirstLoopOnly && _loopIndex > 0u)
        return 0.f;

    return max(0.f, _desc.playback.delay);
}

float MeshEmitter::Resolve_Duration() const
{
    return max(0.0001f, _desc.playback.duration);
}

bool MeshEmitter::Has_NextLoop() const
{
    return _desc.playback.loopCount == 0u || _loopIndex + 1u < _desc.playback.loopCount;
}

bool MeshEmitter::Can_SubmitRender() const
{
    return _isReady && _model && _shader && _instanceBuffer && _activeInstanceCount > 0u &&
           PlaybackState::Delayed != _playbackState;
}

uint32 MeshEmitter::Consume_SpawnRequest(float timeDelta)
{
    const float safeDeltaTime = max(0.f, timeDelta);
    const float previousTime = _emitterElapsedTime;
    _emitterElapsedTime += safeDeltaTime;

    uint32 spawnRequest = 0u;

    if (_desc.spawn.particleSpawn.processSpawnRate)
    {
        const float spawnRate = max(0.f, _desc.spawn.particleSpawn.spawnRate * _desc.spawn.particleSpawn.spawnRateScale);
        _spawnAccumulator += spawnRate * safeDeltaTime;

        const float wholeSpawn = floorf(_spawnAccumulator + 0.0001f);
        if (wholeSpawn > 0.f)
        {
            spawnRequest += static_cast<uint32>(wholeSpawn);
            _spawnAccumulator -= wholeSpawn;
        }
    }

    if (_desc.spawn.particleSpawn.processBurstList)
    {
        if (_burstExecuted.size() != _desc.spawn.particleSpawn.bursts.size())
            _burstExecuted.assign(_desc.spawn.particleSpawn.bursts.size(), false);

        for (size_t index = 0; index < _desc.spawn.particleSpawn.bursts.size(); ++index)
        {
            if (_burstExecuted[index])
                continue;

            const PointParticleBurstDesc& burst = _desc.spawn.particleSpawn.bursts[index];
            if (previousTime > burst.time || _emitterElapsedTime < burst.time)
                continue;

            const float scaledCount = static_cast<float>(burst.count) * max(0.f, _desc.spawn.particleSpawn.burstScale);
            spawnRequest += static_cast<uint32>(floorf(scaledCount));
            _burstExecuted[index] = true;
        }
    }

    return min(spawnRequest, max(1u, _desc.spawn.particleSpawn.maxActiveCount));
}

void MeshEmitter::Spawn_Particles(uint32 spawnRequest)
{
    if (spawnRequest == 0u || _particles.empty())
        return;

    const uint32 maxActiveCount = min(static_cast<uint32>(_particles.size()), max(1u, _desc.spawn.particleSpawn.maxActiveCount));
    uint32 activeCount = 0u;
    for (const MeshParticleState& particle : _particles)
    {
        if (particle.active)
            ++activeCount;
    }

    uint32 remainingRequest = min(spawnRequest, maxActiveCount > activeCount ? maxActiveCount - activeCount : 0u);
    while (remainingRequest > 0u)
    {
        bool activated = false;
        for (uint32 probe = 0; probe < _particles.size(); ++probe)
        {
            const uint32 index = (_nextSpawnCursor + probe) % static_cast<uint32>(_particles.size());
            MeshParticleState& particle = _particles[index];
            if (particle.active)
                continue;

            const uint32 emitterSeed = _emitterId * 104729u;
            Activate_Particle(
                particle,
                index * 9781u + _nextSpawnCursor * 6271u + _loopIndex * 7919u + emitterSeed + 1u);
            _nextSpawnCursor = (index + 1u) % static_cast<uint32>(_particles.size());
            --remainingRequest;
            activated = true;
            break;
        }

        if (!activated)
            break;
    }
}

void MeshEmitter::Activate_Particle(MeshParticleState& particle, uint32 seed)
{
    const float lifeMin = max(0.0001f, min(_desc.lifetime.lifeTime.x, _desc.lifetime.lifeTime.y));
    const float lifeMax = max(lifeMin, max(_desc.lifetime.lifeTime.x, _desc.lifetime.lifeTime.y));
    const Vec3 emitterPosition = _transformCom != nullptr ? _transformCom->Get_WorldPosition() : Vec3{};
    const Vec3 spawnOffset = Sample_InitialOffset(seed);
    const Vec3 spawnPosition = emitterPosition + spawnOffset;

    particle.active = true;
    particle.age = 0.f;
    particle.lifeMax = RandomRange(lifeMin, lifeMax, seed + 11u);
    particle.position = spawnPosition;
    particle.velocity = Sample_InitialVelocity(spawnPosition, seed + 23u);
    particle.acceleration = Sample_Acceleration(seed + 29u);
    particle.drag = Sample_Drag(seed + 31u);
    particle.baseScale = _desc.previewScale * Sample_InitialScale(seed + 37u);
    particle.rotationRadians = Sample_InitialRotation(seed + 41u);
    particle.angularVelocityRadians = Sample_InitialAngularVelocity(seed + 43u);
    particle.startColor = Sample_Color(_desc.initialColor.startColorMin, _desc.initialColor.startColorMax, seed + 47u);
    particle.endColor = Sample_Color(_desc.colorOverLife.endColorMin, _desc.colorOverLife.endColorMax, seed + 53u);
    particle.seed = seed;
}

void MeshEmitter::Update_Particles(float timeDelta)
{
    const float safeDeltaTime = max(0.f, timeDelta);
    for (MeshParticleState& particle : _particles)
    {
        if (!particle.active)
            continue;

        particle.age += safeDeltaTime;
        if (particle.age >= particle.lifeMax)
        {
            particle.active = false;
            continue;
        }

        const float lifeProgress = clamp(particle.age / max(0.0001f, particle.lifeMax), 0.f, 1.f);
        const float velocityScale = Evaluate_VelocityScaleByLife(lifeProgress);
        const Vec3 acceleration = Evaluate_AccelerationByLife(particle, lifeProgress) * velocityScale;
        particle.velocity += acceleration * safeDeltaTime;

        if (particle.drag > 0.f)
            particle.velocity *= expf(-particle.drag * safeDeltaTime);

        particle.position += particle.velocity * velocityScale * safeDeltaTime;

        const Vec3 angularVelocityScale = Evaluate_AngularVelocityScaleByLife(lifeProgress);
        particle.rotationRadians += Vec3{
            particle.angularVelocityRadians.x * angularVelocityScale.x,
            particle.angularVelocityRadians.y * angularVelocityScale.y,
            particle.angularVelocityRadians.z * angularVelocityScale.z
        } * safeDeltaTime;
    }
}

wstring MeshEmitter::Resolve_ModelPath() const
{
    if (nullptr != GAME && !_desc.modelGuid.empty())
    {
        const AssetMeta* assetMeta = GAME->Find_AssetByGUID(_desc.modelGuid);
        if (nullptr != assetMeta && assetMeta->type == "Model")
        {
            const wstring resolvedPath = GAME->Resolve_AssetPath(_desc.modelGuid);
            if (!resolvedPath.empty() && fs::exists(resolvedPath))
                return fs::path(resolvedPath).lexically_normal().wstring();
        }
    }

    if (_desc.modelPath.empty())
        return {};

    fs::path modelPath = String::ToWString(_desc.modelPath);
    if (modelPath.is_relative() && GAME != nullptr)
        modelPath = fs::path(GAME->Get_AssetRoot()) / modelPath;

    modelPath = modelPath.lexically_normal();
    return fs::exists(modelPath) ? modelPath.wstring() : wstring{};
}

wstring MeshEmitter::Resolve_MaterialTexturePath(const string& textureGuid, const string& texturePath) const
{
    if (nullptr != GAME && !textureGuid.empty())
    {
        const AssetMeta* assetMeta = GAME->Find_AssetByGUID(textureGuid);
        if (nullptr != assetMeta && assetMeta->type == "Texture")
        {
            const wstring resolvedPath = GAME->Resolve_AssetPath(textureGuid);
            if (!resolvedPath.empty() && fs::exists(resolvedPath))
                return fs::path(resolvedPath).lexically_normal().wstring();
        }
    }

    if (texturePath.empty())
        return {};

    fs::path resolvedPath = String::ToWString(texturePath);
    if (resolvedPath.is_relative() && GAME != nullptr)
        resolvedPath = fs::path(GAME->Get_AssetRoot()) / resolvedPath;

    resolvedPath = resolvedPath.lexically_normal();
    return fs::exists(resolvedPath) ? resolvedPath.wstring() : wstring{};
}

Matrix MeshEmitter::Build_InstanceMatrix(const MeshParticleState& particle) const
{
    const float lifeProgress = clamp(particle.age / max(0.0001f, particle.lifeMax), 0.f, 1.f);
    const Vec3 sizeByLife = Evaluate_ScaleByLife(lifeProgress);
    const Vec3 scale{
        max(0.0001f, particle.baseScale.x * sizeByLife.x),
        max(0.0001f, particle.baseScale.y * sizeByLife.y),
        max(0.0001f, particle.baseScale.z * sizeByLife.z)
    };
    const Vec3 position = particle.position;
    const Vec3 rotationByLife = Evaluate_RotationByLife(lifeProgress);
    const Vec3 rotationRadians{
        particle.rotationRadians.x + XMConvertToRadians(rotationByLife.x),
        particle.rotationRadians.y + XMConvertToRadians(rotationByLife.y),
        particle.rotationRadians.z + XMConvertToRadians(rotationByLife.z)
    };

    Quat rotation = Quat::CreateFromYawPitchRoll(rotationRadians.y, rotationRadians.x, rotationRadians.z);
    rotation.Normalize();

    return
        Matrix::CreateScale(scale) *
        Matrix::CreateFromQuaternion(rotation) *
        Internal::Resolve_MeshPlaneAlignmentMatrix(_desc.meshTransform.alignment) *
        Matrix::CreateTranslation(position);
}

Vec3 MeshEmitter::Sample_InitialOffset(uint32 seed) const
{
    Vec3 offset{};

    if (_desc.initialLocation.enabled)
    {
        offset += Vec3{
            RandomRange(_desc.initialLocation.minOffset.x, _desc.initialLocation.maxOffset.x, seed + 1u),
            RandomRange(_desc.initialLocation.minOffset.y, _desc.initialLocation.maxOffset.y, seed + 2u),
            RandomRange(_desc.initialLocation.minOffset.z, _desc.initialLocation.maxOffset.z, seed + 3u)
        };
    }

    if (_desc.sphereLocation.enabled && _desc.sphereLocation.radius > 0.f)
    {
        const float z = RandomRange(-1.f, 1.f, seed + 5u);
        const float angle = XM_2PI * Random01(seed + 7u);
        const float radiusOnPlane = sqrtf(max(0.f, 1.f - z * z));
        const Vec3 direction{
            cosf(angle) * radiusOnPlane,
            sinf(angle) * radiusOnPlane,
            z
        };

        float radius = _desc.sphereLocation.radius;
        if (_desc.sphereLocation.mode == PointParticleSphereLocationMode::Volume)
            radius *= cbrtf(Random01(seed + 9u));

        offset += _desc.sphereLocation.offset + direction * radius;
    }

    return Resolve_LocalVector(offset);
}

Vec3 MeshEmitter::Sample_InitialVelocity(const Vec3& spawnPosition, uint32 seed) const
{
    Vec3 velocity{};

    if (_desc.motion.initialVelocityEnabled)
    {
        const Vec3 sampledVelocity{
            RandomRange(_desc.motion.initialVelocityMin.x, _desc.motion.initialVelocityMax.x, seed + 1u),
            RandomRange(_desc.motion.initialVelocityMin.y, _desc.motion.initialVelocityMax.y, seed + 2u),
            RandomRange(_desc.motion.initialVelocityMin.z, _desc.motion.initialVelocityMax.z, seed + 3u)
        };
        velocity += _desc.motion.initialVelocityInWorldSpace ? sampledVelocity : Resolve_LocalVector(sampledVelocity);
    }

    if (_desc.motion.initialRadialVelocityEnabled)
    {
        const Vec3 radialPivotOffset = _desc.motion.initialRadialVelocityInWorldSpace
                                       ? _desc.motion.radialPivot
                                       : Resolve_LocalVector(_desc.motion.radialPivot);
        const Vec3 emitterPosition = _transformCom != nullptr ? _transformCom->Get_WorldPosition() : Vec3{};
        const Vec3 pivotPosition = emitterPosition + radialPivotOffset;
        Vec3 radialDirection = spawnPosition - pivotPosition;
        if (radialDirection.LengthSquared() < 0.0001f)
            radialDirection = _transformCom != nullptr ? _transformCom->Get_WorldForward() : Vec3{ 0.f, 0.f, 1.f };
        radialDirection.Normalize();

        const float radialSpeed = RandomRange(
            min(_desc.motion.radialSpeed.x, _desc.motion.radialSpeed.y),
            max(_desc.motion.radialSpeed.x, _desc.motion.radialSpeed.y),
            seed + 5u
        );
        velocity += radialDirection * radialSpeed;
    }

    return velocity;
}

Vec3 MeshEmitter::Sample_Acceleration(uint32 seed) const
{
    const Vec3 sampledAcceleration{
        RandomRange(_desc.motion.accelerationMin.x, _desc.motion.accelerationMax.x, seed + 1u),
        RandomRange(_desc.motion.accelerationMin.y, _desc.motion.accelerationMax.y, seed + 2u),
        RandomRange(_desc.motion.accelerationMin.z, _desc.motion.accelerationMax.z, seed + 3u)
    };
    return Resolve_LocalVector(sampledAcceleration);
}

float MeshEmitter::Sample_Drag(uint32 seed) const
{
    const float minDrag = min(_desc.motion.drag.x, _desc.motion.drag.y);
    const float maxDrag = max(_desc.motion.drag.x, _desc.motion.drag.y);
    return max(0.f, RandomRange(minDrag, maxDrag, seed));
}

Vec4 MeshEmitter::Sample_Color(const Vec4& minColor, const Vec4& maxColor, uint32 seed) const
{
    return Vec4{
        RandomRange(min(minColor.x, maxColor.x), max(minColor.x, maxColor.x), seed + 1u),
        RandomRange(min(minColor.y, maxColor.y), max(minColor.y, maxColor.y), seed + 2u),
        RandomRange(min(minColor.z, maxColor.z), max(minColor.z, maxColor.z), seed + 3u),
        RandomRange(min(minColor.w, maxColor.w), max(minColor.w, maxColor.w), seed + 4u)
    };
}

Vec3 MeshEmitter::Sample_InitialScale(uint32 seed) const
{
    if (!_desc.meshTransform.initialScaleEnabled)
        return Vec3{ 1.f, 1.f, 1.f };

    return Vec3{
        max(0.0001f, RandomRange(_desc.meshTransform.initialScaleMin.x, _desc.meshTransform.initialScaleMax.x, seed + 1u)),
        max(0.0001f, RandomRange(_desc.meshTransform.initialScaleMin.y, _desc.meshTransform.initialScaleMax.y, seed + 2u)),
        max(0.0001f, RandomRange(_desc.meshTransform.initialScaleMin.z, _desc.meshTransform.initialScaleMax.z, seed + 3u))
    };
}

Vec3 MeshEmitter::Sample_InitialRotation(uint32 seed) const
{
    if (!_desc.meshTransform.rotationEnabled)
        return Vec3{};

    return Vec3{
        XMConvertToRadians(RandomRange(_desc.meshTransform.initialRotationDegreesMin.x, _desc.meshTransform.initialRotationDegreesMax.x, seed + 1u)),
        XMConvertToRadians(RandomRange(_desc.meshTransform.initialRotationDegreesMin.y, _desc.meshTransform.initialRotationDegreesMax.y, seed + 2u)),
        XMConvertToRadians(RandomRange(_desc.meshTransform.initialRotationDegreesMin.z, _desc.meshTransform.initialRotationDegreesMax.z, seed + 3u))
    };
}

Vec3 MeshEmitter::Sample_InitialAngularVelocity(uint32 seed) const
{
    if (!_desc.meshTransform.rotationEnabled)
        return Vec3{};

    return Vec3{
        XMConvertToRadians(
            RandomRange(_desc.meshTransform.initialAngularVelocityDegreesMin.x, _desc.meshTransform.initialAngularVelocityDegreesMax.x, seed + 1u)
        ),
        XMConvertToRadians(
            RandomRange(_desc.meshTransform.initialAngularVelocityDegreesMin.y, _desc.meshTransform.initialAngularVelocityDegreesMax.y, seed + 2u)
        ),
        XMConvertToRadians(
            RandomRange(_desc.meshTransform.initialAngularVelocityDegreesMin.z, _desc.meshTransform.initialAngularVelocityDegreesMax.z, seed + 3u)
        )
    };
}

float MeshEmitter::Evaluate_VelocityScaleByLife(float lifeProgress) const
{
    return max(0.f, lerp(_desc.motion.velocityScaleByLife.x, _desc.motion.velocityScaleByLife.y, lifeProgress));
}

Vec3 MeshEmitter::Evaluate_AccelerationByLife(const MeshParticleState& particle, float lifeProgress) const
{
    if (!_desc.motion.accelerationCurveEnabled)
        return particle.acceleration;

    const uint32 keyCount = max(1u, min(4u, _desc.motion.accelerationCurveKeyCount));
    const Vec3 acceleration{
        Evaluate_CompactCurve(lifeProgress, _desc.motion.accelerationCurveTimes, _desc.motion.accelerationCurveValuesX, keyCount, particle.acceleration.x),
        Evaluate_CompactCurve(lifeProgress, _desc.motion.accelerationCurveTimes, _desc.motion.accelerationCurveValuesY, keyCount, particle.acceleration.y),
        Evaluate_CompactCurve(lifeProgress, _desc.motion.accelerationCurveTimes, _desc.motion.accelerationCurveValuesZ, keyCount, particle.acceleration.z)
    };
    return Resolve_LocalVector(acceleration);
}

Vec3 MeshEmitter::Evaluate_ScaleByLife(float lifeProgress) const
{
    return Evaluate_MeshVector3Curve(_desc.meshTransform.scaleByLife, lifeProgress, Vec3{ 1.f, 1.f, 1.f });
}

Vec3 MeshEmitter::Evaluate_RotationByLife(float lifeProgress) const
{
    return Evaluate_MeshVector3Curve(_desc.meshTransform.rotationByLife, lifeProgress, Vec3{});
}

Vec3 MeshEmitter::Evaluate_AngularVelocityScaleByLife(float lifeProgress) const
{
    return Evaluate_MeshVector3Curve(_desc.meshTransform.angularVelocityScaleByLife, lifeProgress, Vec3{ 1.f, 1.f, 1.f });
}

Vec4 MeshEmitter::Evaluate_ColorOverLife(const MeshParticleState& particle, float lifeProgress) const
{
    Vec4 color = {
        lerp(particle.startColor.x, particle.endColor.x, lifeProgress),
        lerp(particle.startColor.y, particle.endColor.y, lifeProgress),
        lerp(particle.startColor.z, particle.endColor.z, lifeProgress),
        lerp(particle.startColor.w, particle.endColor.w, lifeProgress)
    };

    const PointParticleColorOverLifeCurveDesc& curve = _desc.colorOverLife.curve;
    if (curve.colorCurveEnabled)
    {
        const uint32 keyCount = max(1u, min(4u, curve.colorCurveKeyCount));
        auto evaluate_curve = [lifeProgress, keyCount](const Vec4& times, const Vec4& values, float fallbackValue)
        {
            if (keyCount == 1u || lifeProgress <= Internal::Read_Vec4Component(times, 0u))
                return Internal::Read_Vec4Component(values, 0u);

            for (uint32 index = 1u; index < keyCount; ++index)
            {
                const float rightTime = Internal::Read_Vec4Component(times, index);
                if (lifeProgress > rightTime)
                    continue;

                const float leftTime = Internal::Read_Vec4Component(times, index - 1u);
                const float ratio = clamp((lifeProgress - leftTime) / max(0.0001f, rightTime - leftTime), 0.f, 1.f);
                return lerp(Internal::Read_Vec4Component(values, index - 1u), Internal::Read_Vec4Component(values, index), ratio);
            }

            return lifeProgress > Internal::Read_Vec4Component(times, keyCount - 1u)
                   ? Internal::Read_Vec4Component(values, keyCount - 1u)
                   : fallbackValue;
        };

        color.x = evaluate_curve(curve.colorCurveTimes, curve.colorCurveValuesR, color.x);
        color.y = evaluate_curve(curve.colorCurveTimes, curve.colorCurveValuesG, color.y);
        color.z = evaluate_curve(curve.colorCurveTimes, curve.colorCurveValuesB, color.z);
    }

    if (curve.alphaCurveEnabled)
    {
        const uint32 keyCount = max(1u, min(4u, curve.alphaCurveKeyCount));
        if (keyCount == 1u || lifeProgress <= Internal::Read_Vec4Component(curve.alphaCurveTimes, 0u))
            color.w = Internal::Read_Vec4Component(curve.alphaCurveValues, 0u);
        else
        {
            for (uint32 index = 1u; index < keyCount; ++index)
            {
                const float rightTime = Internal::Read_Vec4Component(curve.alphaCurveTimes, index);
                if (lifeProgress > rightTime)
                    continue;

                const float leftTime = Internal::Read_Vec4Component(curve.alphaCurveTimes, index - 1u);
                const float ratio = clamp((lifeProgress - leftTime) / max(0.0001f, rightTime - leftTime), 0.f, 1.f);
                color.w = lerp(
                    Internal::Read_Vec4Component(curve.alphaCurveValues, index - 1u),
                    Internal::Read_Vec4Component(curve.alphaCurveValues, index),
                    ratio
                );
                break;
            }

            if (lifeProgress > Internal::Read_Vec4Component(curve.alphaCurveTimes, keyCount - 1u))
                color.w = Internal::Read_Vec4Component(curve.alphaCurveValues, keyCount - 1u);
        }
    }

    color.w = clamp(color.w, 0.f, 1.f);
    return color;
}

float MeshEmitter::Evaluate_CompactCurve(float lifeProgress, const Vec4& times, const Vec4& values, uint32 keyCount, float fallbackValue) const
{
    const uint32 clampedKeyCount = max(1u, min(4u, keyCount));
    if (clampedKeyCount == 1u || lifeProgress <= Internal::Read_Vec4Component(times, 0u))
        return Internal::Read_Vec4Component(values, 0u);

    for (uint32 index = 1u; index < clampedKeyCount; ++index)
    {
        const float rightTime = Internal::Read_Vec4Component(times, index);
        if (lifeProgress > rightTime)
            continue;

        const float leftTime = Internal::Read_Vec4Component(times, index - 1u);
        const float ratio = clamp((lifeProgress - leftTime) / max(0.0001f, rightTime - leftTime), 0.f, 1.f);
        return lerp(Internal::Read_Vec4Component(values, index - 1u), Internal::Read_Vec4Component(values, index), ratio);
    }

    return lifeProgress > Internal::Read_Vec4Component(times, clampedKeyCount - 1u)
           ? Internal::Read_Vec4Component(values, clampedKeyCount - 1u)
           : fallbackValue;
}

Vec3 MeshEmitter::Evaluate_MeshVector3Curve(const MeshVector3CurveRuntimeDesc& desc, float lifeProgress, const Vec3& fallbackValue) const
{
    if (!desc.enabled)
        return fallbackValue;

    return Vec3{
        Evaluate_CompactCurve(lifeProgress, desc.curveKeyTimes, desc.curveKeyValuesX, desc.curveKeyCount, lerp(desc.start.x, desc.end.x, lifeProgress)),
        Evaluate_CompactCurve(lifeProgress, desc.curveKeyTimes, desc.curveKeyValuesY, desc.curveKeyCount, lerp(desc.start.y, desc.end.y, lifeProgress)),
        Evaluate_CompactCurve(lifeProgress, desc.curveKeyTimes, desc.curveKeyValuesZ, desc.curveKeyCount, lerp(desc.start.z, desc.end.z, lifeProgress))
    };
}

RenderGroup MeshEmitter::Resolve_RenderGroup() const
{
    if (_desc.renderLayerOverride == EffectEmitterRenderLayerOverride::UIEffect)
        return RenderGroup::UIEffect;

    if (Is_DistortionFamily())
        return RenderGroup::Distortion;

    return _desc.material.blendMode == EffectMaterialBlendMode::Modulate
           ? RenderGroup::NonBlend
           : RenderGroup::Blend;
}

uint32 MeshEmitter::Resolve_ShaderPassIndex() const
{
    if (Is_DistortionFamily())
        return 0u;

    const bool additive = _desc.material.blendMode == EffectMaterialBlendMode::Additive;
    const bool twoSided = _desc.material.twoSided;
    if (additive)
        return twoSided ? 3u : 1u;
    return twoSided ? 2u : 0u;
}

int MeshEmitter::Resolve_OpacitySourceIndex() const
{
    if (_desc.material.opacitySource == "Alpha" || _desc.material.opacitySource == "alpha")
        return kOpacitySourceAlpha;

    if (_desc.material.opacitySource == "Red" || _desc.material.opacitySource == "red")
        return kOpacitySourceRed;

    if (_desc.material.opacitySource == "Luminance" || _desc.material.opacitySource == "luminance")
        return kOpacitySourceLuminance;

    return kOpacitySourceAlpha;
}

Vec3 MeshEmitter::Resolve_LocalVector(const Vec3& localVector) const
{
    if (_transformCom == nullptr)
        return localVector;

    return _transformCom->Get_WorldRight() * localVector.x +
           _transformCom->Get_WorldUp() * localVector.y +
           _transformCom->Get_WorldForward() * localVector.z;
}

float MeshEmitter::Random01(uint32 seed) const
{
    seed ^= 2747636419u;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    return static_cast<float>(seed & 0x00FFFFFFu) / 16777215.f;
}

float MeshEmitter::RandomRange(float minValue, float maxValue, uint32 seed) const
{
    if (minValue > maxValue)
        std::swap(minValue, maxValue);

    return lerp(minValue, maxValue, Random01(seed));
}

Shared<MeshEmitter> MeshEmitter::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MeshEmitter>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : MeshEmitter");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MeshEmitter::Clone(void* arg)
{
    auto instance = make_shared<MeshEmitter>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : MeshEmitter");
        MSG_BOX("Failed to Clone : MeshEmitter");
        return nullptr;
    }

    return instance;
}

void MeshEmitter::Free()
{
    _desc = {};
    _model.reset();
    _shader.reset();
    _texture.reset();
    _noiseTexture.reset();
    _maskTexture.reset();
    _instanceBuffer.Reset();
    _particles.clear();
    _instancePayload.clear();
    _burstExecuted.clear();
    _emitterElapsedTime = 0.f;
    _spawnAccumulator = 0.f;
    _playbackState = PlaybackState::Delayed;
    _loopElapsedTime = 0.f;
    _loopIndex = 0u;
    _completedKillDispatched = false;
    _activeInstanceCount = 0u;
    _nextSpawnCursor = 0u;
    _instanceCapacity = 0u;
    _opacitySourceIndex = kOpacitySourceAlpha;
    _isReady = false;

    __super::Free();
}
