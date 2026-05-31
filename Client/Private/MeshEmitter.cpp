#include "MeshEmitter.h"

#include "GameInstance.h"
#include "Material.h"
#include "ModelCom.h"
#include "Renderer_Define.h"
#include "ShaderCom.h"

#include <chrono>

namespace
{
constexpr D3D11_INPUT_ELEMENT_DESC kEffectMeshInputElements[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "INST_WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    { "INST_WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    { "INST_WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    { "INST_WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    { "INST_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    { "INST_LIFE", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 80, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
};

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

    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
}

HRESULT MeshEmitter::Render()
{
    if (!Can_SubmitRender())
        return S_OK;

    if (FAILED(GAME->Bind_CameraCB(_shader)))
    {
        LOG_WARN_ONCE(
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
        LOG_WARN_ONCE(
            "MeshEmitter render failed: pre-transform binding failed. emitter='{}', modelGuid='{}', modelPath='{}'",
            _emitterName,
            _desc.modelGuid,
            _desc.modelPath
        );
        return E_FAIL;
    }

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
    {
        const uint32 materialIndex = _model->Get_MeshMaterialIndex(meshIndex);
        const Shared<Material> material = _model->Get_Material(materialIndex);
        uint32 passIndex = 2u;

        if (nullptr != material)
        {
            if (FAILED(_shader->Bind_MaterialTextures(*material)))
            {
                LOG_WARN_ONCE(
                    "MeshEmitter material binding skip: texture binding failed. emitter='{}', meshIndex={}, materialIndex={}, modelGuid='{}', modelPath='{}'",
                    _emitterName,
                    meshIndex,
                    materialIndex,
                    _desc.modelGuid,
                    _desc.modelPath
                );
                continue;
            }

            if (FAILED(_shader->Bind_CBufferData(material->Get_CB())))
            {
                LOG_WARN_ONCE(
                    "MeshEmitter render failed: material constant binding failed. emitter='{}', meshIndex={}, materialIndex={}, modelGuid='{}', modelPath='{}'",
                    _emitterName,
                    meshIndex,
                    materialIndex,
                    _desc.modelGuid,
                    _desc.modelPath
                );
                return E_FAIL;
            }

            passIndex = static_cast<uint32>(material->Get_PassIndex());
        }
        else
        {
            LOG_WARN_ONCE(
                "MeshEmitter material binding skip: mesh material is missing. emitter='{}', meshIndex={}, materialIndex={}, numMaterials={}, modelGuid='{}', modelPath='{}'",
                _emitterName,
                meshIndex,
                materialIndex,
                static_cast<uint32>(_model->Get_NumMaterials()),
                _desc.modelGuid,
                _desc.modelPath
            );
        }

        if (FAILED(_shader->Begin(passIndex)))
        {
            LOG_WARN_ONCE(
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
            LOG_WARN_ONCE(
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

void MeshEmitter::On_EffectTransformSynced()
{
    if (_activeInstanceCount > 0u)
        Update_InstanceBuffer();
}

void MeshEmitter::Log_DescPayload(const char* context) const
{
    LOG_INFO(
        "MeshEmitter desc payload. context={}, emitter='{}', spawnCount={}, spawnRate={}, bursts={}, life=({}, {}), initialSize={}, sizeByLife={}, motion={}, acceleration={}, drag=({}, {}), velocityScale=({}, {}), rotation={}, colorCurve={}, alphaCurve={}",
        context,
        _emitterName,
        _desc.spawn.instanceCount,
        _desc.spawn.particleSpawn.spawnRate,
        static_cast<uint32>(_desc.spawn.particleSpawn.bursts.size()),
        _desc.lifetime.lifeTime.x,
        _desc.lifetime.lifeTime.y,
        _desc.initialSize.enabled,
        _desc.sizeByLife.enabled,
        _desc.motion.enabled,
        Has_NonZeroVector(_desc.motion.accelerationMin) || Has_NonZeroVector(_desc.motion.accelerationMax),
        _desc.motion.drag.x,
        _desc.motion.drag.y,
        _desc.motion.velocityScaleByLife.x,
        _desc.motion.velocityScaleByLife.y,
        _desc.rotation.enabled,
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

    return true;
}

bool MeshEmitter::Has_MaterialOverride(const MeshEmitterDesc& desc) const
{
    return !desc.material.mainTextureGuid.empty() || !desc.material.mainTexturePath.empty();
}

HRESULT MeshEmitter::Apply_DescForReload(const MeshEmitterDesc& desc)
{
    const auto totalBegin = std::chrono::steady_clock::now();
    _desc = desc;
    Log_DescPayload("reload");
    Reset_PlaybackRuntime();

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
        0.0,
        0.0,
        0.0,
        Elapsed_Milliseconds(materialBegin, materialEnd),
        Elapsed_Milliseconds(bufferBegin, bufferEnd),
        Elapsed_Milliseconds(totalBegin, totalEnd)
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
        Elapsed_Milliseconds(shaderBegin, shaderEnd),
        Elapsed_Milliseconds(resolveModelBegin, resolveModelEnd),
        Elapsed_Milliseconds(modelBegin, modelEnd),
        Elapsed_Milliseconds(materialBegin, materialEnd),
        Elapsed_Milliseconds(bufferBegin, bufferEnd),
        Elapsed_Milliseconds(totalBegin, totalEnd)
    );
    return S_OK;
}

HRESULT MeshEmitter::Ready_Shader()
{
    const wstring shaderPath = Resolve_ShaderPath();
    if (shaderPath.empty())
    {
        LOG_WARN(
            "MeshEmitter shader missing: effect mesh shader path could not be resolved. emitter='{}', shader='{}'",
            _emitterName,
            kMeshShaderPath
        );
        return E_FAIL;
    }

    _shader = ShaderCom::Create(
        _device,
        _context,
        shaderPath.c_str(),
        kEffectMeshInputElements,
        static_cast<uint32>(_countof(kEffectMeshInputElements)),
        ShaderType::VtxInstanceMeshExact
    );
    if (_shader == nullptr)
    {
        LOG_WARN(
            "MeshEmitter shader missing: effect mesh shader create failed. emitter='{}', shaderPath='{}'",
            _emitterName,
            String::ToString(shaderPath)
        );
        return E_FAIL;
    }

    return S_OK;
}

HRESULT MeshEmitter::Apply_EffectMaterialOverride()
{
    if (_model == nullptr)
        return E_FAIL;

    const wstring mainTexturePath = Resolve_MaterialTexturePath(_desc.material.mainTextureGuid, _desc.material.mainTexturePath);
    if (mainTexturePath.empty())
    {
        LOG_WARN_ONCE(
            "MeshEmitter assigned effect material has no resolvable main texture. emitter='{}', materialMainGuid='{}', materialMainPath='{}'",
            _emitterName,
            _desc.material.mainTextureGuid,
            _desc.material.mainTexturePath
        );
        return S_OK;
    }

    const uint32 numMaterials = static_cast<uint32>(_model->Get_NumMaterials());
    if (numMaterials == 0)
    {
        LOG_WARN_ONCE(
            "MeshEmitter assigned effect material override skipped: model has no material slots. emitter='{}', modelGuid='{}', modelPath='{}'",
            _emitterName,
            _desc.modelGuid,
            _desc.modelPath
        );
        return S_OK;
    }

    for (uint32 materialIndex = 0; materialIndex < numMaterials; ++materialIndex)
    {
        const Shared<Material> material = _model->Get_Material(materialIndex);
        if (material == nullptr)
            continue;

        CHECK_FAILED(material->Override_TextureSlot(MaterialTextureSlot::BaseColor, 0u, mainTexturePath), E_FAIL);

        MaterialCB materialCB = material->Get_CB();
        materialCB.baseColorTint = Vec4{
            _desc.material.tint.x * _desc.material.intensity,
            _desc.material.tint.y * _desc.material.intensity,
            _desc.material.tint.z * _desc.material.intensity,
            _desc.material.tint.w
        };
        materialCB.uvScale = _desc.material.mainUVScale;
        materialCB.uvOffset = _desc.material.mainUVOffset;
        material->Set_CB(materialCB);

        const bool useMaskedPass = _desc.material.alphaCutoff > 0.f;
        material->Set_ForcedPassIndex(useMaskedPass ? 3 : 0);
    }

    LOG_INFO_ONCE(
        "MeshEmitter assigned effect material override applied. emitter='{}', materialMainPath='{}', materialCount={}",
        _emitterName,
        String::ToString(mainTexturePath),
        numMaterials
    );
    return S_OK;
}

HRESULT MeshEmitter::Ready_InstanceBuffer()
{
    const uint32 instanceCapacity = max(1u, _desc.spawn.instanceCount);
    if (_instanceBuffer != nullptr && _instanceCapacity == instanceCapacity)
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

    for (const MeshParticleState& particle : _particles)
    {
        if (!particle.active || _activeInstanceCount >= drawLimit)
            continue;

        const float lifeProgress = clamp(particle.age / max(0.0001f, particle.lifeMax), 0.f, 1.f);
        MeshInstanceVertex& instance = _instancePayload[_activeInstanceCount++];
        instance.world = Build_InstanceMatrix(particle);
        instance.color = Evaluate_ColorOverLife(particle, lifeProgress);
        instance.lifeTime = Vec2{ particle.lifeMax, particle.age };
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

            Activate_Particle(particle, index * 9781u + _nextSpawnCursor * 6271u + _loopIndex * 7919u + 1u);
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
    particle.rollRadians = XMConvertToRadians(
        RandomRange(
            min(_desc.rotation.initialRotationDegrees.x, _desc.rotation.initialRotationDegrees.y),
            max(_desc.rotation.initialRotationDegrees.x, _desc.rotation.initialRotationDegrees.y),
            seed + 41u
        )
    );
    particle.rollRateRadians = XMConvertToRadians(
        RandomRange(
            min(_desc.rotation.initialRotationRateDegrees.x, _desc.rotation.initialRotationRateDegrees.y),
            max(_desc.rotation.initialRotationRateDegrees.x, _desc.rotation.initialRotationRateDegrees.y),
            seed + 43u
        )
    );
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
        const Vec3 acceleration = particle.acceleration * velocityScale;
        particle.velocity += acceleration * safeDeltaTime;

        if (particle.drag > 0.f)
            particle.velocity *= expf(-particle.drag * safeDeltaTime);

        particle.position += particle.velocity * velocityScale * safeDeltaTime;

        const float rotationRateScale = Evaluate_RotationRateScaleByLife(lifeProgress);
        particle.rollRadians += particle.rollRateRadians * rotationRateScale * safeDeltaTime;
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

wstring MeshEmitter::Resolve_ShaderPath() const
{
    const fs::path configuredPath = String::ToWString(kMeshShaderPath);
    vector<fs::path> candidates{};
    candidates.push_back(configuredPath);

    if (configuredPath.is_relative())
    {
        candidates.push_back(fs::current_path() / configuredPath);
        candidates.push_back(fs::current_path() / L"Client/Bin/ShaderFiles/Shader_EffectMesh.hlsl");
        candidates.push_back(fs::current_path() / L"../Client/Bin/ShaderFiles/Shader_EffectMesh.hlsl");
        if (GAME != nullptr)
            candidates.push_back(fs::path(GAME->Get_AssetRoot()).parent_path().parent_path() / L"Bin/ShaderFiles/Shader_EffectMesh.hlsl");
    }

    for (const fs::path& candidate : candidates)
    {
        const fs::path normalized = candidate.lexically_normal();
        if (fs::exists(normalized))
            return normalized.wstring();
    }

    return {};
}

Matrix MeshEmitter::Build_InstanceMatrix(const MeshParticleState& particle) const
{
    const float lifeProgress = clamp(particle.age / max(0.0001f, particle.lifeMax), 0.f, 1.f);
    const float sizeByLife = Evaluate_SizeByLife(lifeProgress);
    const Vec3 scale{
        max(0.0001f, particle.baseScale.x * sizeByLife),
        max(0.0001f, particle.baseScale.y * sizeByLife),
        max(0.0001f, particle.baseScale.z * sizeByLife)
    };
    const Vec3 position = particle.position;
    const float rollRadians = particle.rollRadians + XMConvertToRadians(Evaluate_RotationOverLife(lifeProgress));

    return Matrix::CreateScale(scale) * Matrix::CreateRotationZ(rollRadians) * Matrix::CreateTranslation(position);
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

float MeshEmitter::Sample_InitialScale(uint32 seed) const
{
    if (!_desc.initialSize.enabled)
        return 1.f;

    const float minScale = min(_desc.initialSize.sizeMin.x, _desc.initialSize.sizeMax.x);
    const float maxScale = max(_desc.initialSize.sizeMin.x, _desc.initialSize.sizeMax.x);
    return max(0.0001f, RandomRange(minScale, maxScale, seed));
}

float MeshEmitter::Evaluate_VelocityScaleByLife(float lifeProgress) const
{
    return max(0.f, lerp(_desc.motion.velocityScaleByLife.x, _desc.motion.velocityScaleByLife.y, lifeProgress));
}

float MeshEmitter::Evaluate_SizeByLife(float lifeProgress) const
{
    if (!_desc.sizeByLife.enabled)
        return 1.f;

    const uint32 keyCount = max(1u, min(4u, _desc.sizeByLife.curveKeyCount));
    Vec2 multiplier{
        _desc.sizeByLife.multiplyXStart,
        _desc.sizeByLife.multiplyYStart
    };

    if (keyCount == 1u || lifeProgress <= Read_Vec4Component(_desc.sizeByLife.curveKeyTimes, 0u))
    {
        multiplier = Vec2{
            Read_Vec4Component(_desc.sizeByLife.curveKeyValuesX, 0u),
            Read_Vec4Component(_desc.sizeByLife.curveKeyValuesY, 0u)
        };
    }
    else
    {
        bool found = false;
        for (uint32 index = 1u; index < keyCount; ++index)
        {
            const float rightTime = Read_Vec4Component(_desc.sizeByLife.curveKeyTimes, index);
            if (lifeProgress > rightTime)
                continue;

            const float leftTime = Read_Vec4Component(_desc.sizeByLife.curveKeyTimes, index - 1u);
            const float ratio = clamp((lifeProgress - leftTime) / max(0.0001f, rightTime - leftTime), 0.f, 1.f);
            multiplier = Vec2{
                lerp(
                    Read_Vec4Component(_desc.sizeByLife.curveKeyValuesX, index - 1u),
                    Read_Vec4Component(_desc.sizeByLife.curveKeyValuesX, index),
                    ratio
                ),
                lerp(
                    Read_Vec4Component(_desc.sizeByLife.curveKeyValuesY, index - 1u),
                    Read_Vec4Component(_desc.sizeByLife.curveKeyValuesY, index),
                    ratio
                )
            };
            found = true;
            break;
        }

        if (!found)
        {
            multiplier = Vec2{
                Read_Vec4Component(_desc.sizeByLife.curveKeyValuesX, keyCount - 1u),
                Read_Vec4Component(_desc.sizeByLife.curveKeyValuesY, keyCount - 1u)
            };
        }
    }

    if (_desc.sizeByLife.axisLock == PointParticleSizeByLifeAxisLock::X)
        multiplier.y = multiplier.x;
    else if (_desc.sizeByLife.axisLock == PointParticleSizeByLifeAxisLock::Y)
        multiplier.x = multiplier.y;

    if (!_desc.sizeByLife.multiplyX)
        multiplier.x = 1.f;
    if (!_desc.sizeByLife.multiplyY)
        multiplier.y = 1.f;

    return max(0.0001f, (multiplier.x + multiplier.y) * 0.5f);
}

float MeshEmitter::Evaluate_RotationOverLife(float lifeProgress) const
{
    if (!_desc.rotation.enabled)
        return 0.f;

    return Evaluate_CompactCurve(
        lifeProgress,
        _desc.rotation.rotationOverLifeCurveTimes,
        _desc.rotation.rotationOverLifeCurveValues,
        _desc.rotation.rotationOverLifeCurveKeyCount,
        lerp(_desc.rotation.rotationOverLifeDegrees.x, _desc.rotation.rotationOverLifeDegrees.y, lifeProgress)
    );
}

float MeshEmitter::Evaluate_RotationRateScaleByLife(float lifeProgress) const
{
    if (!_desc.rotation.enabled)
        return 1.f;

    return Evaluate_CompactCurve(
        lifeProgress,
        _desc.rotation.rotationRateScaleByLifeCurveTimes,
        _desc.rotation.rotationRateScaleByLifeCurveValues,
        _desc.rotation.rotationRateScaleByLifeCurveKeyCount,
        lerp(_desc.rotation.rotationRateScaleByLife.x, _desc.rotation.rotationRateScaleByLife.y, lifeProgress)
    );
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
            if (keyCount == 1u || lifeProgress <= Read_Vec4Component(times, 0u))
                return Read_Vec4Component(values, 0u);

            for (uint32 index = 1u; index < keyCount; ++index)
            {
                const float rightTime = Read_Vec4Component(times, index);
                if (lifeProgress > rightTime)
                    continue;

                const float leftTime = Read_Vec4Component(times, index - 1u);
                const float ratio = clamp((lifeProgress - leftTime) / max(0.0001f, rightTime - leftTime), 0.f, 1.f);
                return lerp(Read_Vec4Component(values, index - 1u), Read_Vec4Component(values, index), ratio);
            }

            return lifeProgress > Read_Vec4Component(times, keyCount - 1u)
                   ? Read_Vec4Component(values, keyCount - 1u)
                   : fallbackValue;
        };

        color.x = evaluate_curve(curve.colorCurveTimes, curve.colorCurveValuesR, color.x);
        color.y = evaluate_curve(curve.colorCurveTimes, curve.colorCurveValuesG, color.y);
        color.z = evaluate_curve(curve.colorCurveTimes, curve.colorCurveValuesB, color.z);
    }

    if (curve.alphaCurveEnabled)
    {
        const uint32 keyCount = max(1u, min(4u, curve.alphaCurveKeyCount));
        if (keyCount == 1u || lifeProgress <= Read_Vec4Component(curve.alphaCurveTimes, 0u))
            color.w = Read_Vec4Component(curve.alphaCurveValues, 0u);
        else
        {
            for (uint32 index = 1u; index < keyCount; ++index)
            {
                const float rightTime = Read_Vec4Component(curve.alphaCurveTimes, index);
                if (lifeProgress > rightTime)
                    continue;

                const float leftTime = Read_Vec4Component(curve.alphaCurveTimes, index - 1u);
                const float ratio = clamp((lifeProgress - leftTime) / max(0.0001f, rightTime - leftTime), 0.f, 1.f);
                color.w = lerp(
                    Read_Vec4Component(curve.alphaCurveValues, index - 1u),
                    Read_Vec4Component(curve.alphaCurveValues, index),
                    ratio
                );
                break;
            }

            if (lifeProgress > Read_Vec4Component(curve.alphaCurveTimes, keyCount - 1u))
                color.w = Read_Vec4Component(curve.alphaCurveValues, keyCount - 1u);
        }
    }

    color.w = clamp(color.w, 0.f, 1.f);
    return color;
}

float MeshEmitter::Evaluate_CompactCurve(float lifeProgress, const Vec4& times, const Vec4& values, uint32 keyCount, float fallbackValue) const
{
    const uint32 clampedKeyCount = max(1u, min(4u, keyCount));
    if (clampedKeyCount == 1u || lifeProgress <= Read_Vec4Component(times, 0u))
        return Read_Vec4Component(values, 0u);

    for (uint32 index = 1u; index < clampedKeyCount; ++index)
    {
        const float rightTime = Read_Vec4Component(times, index);
        if (lifeProgress > rightTime)
            continue;

        const float leftTime = Read_Vec4Component(times, index - 1u);
        const float ratio = clamp((lifeProgress - leftTime) / max(0.0001f, rightTime - leftTime), 0.f, 1.f);
        return lerp(Read_Vec4Component(values, index - 1u), Read_Vec4Component(values, index), ratio);
    }

    return lifeProgress > Read_Vec4Component(times, clampedKeyCount - 1u)
           ? Read_Vec4Component(values, clampedKeyCount - 1u)
           : fallbackValue;
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
    _isReady = false;

    __super::Free();
}
