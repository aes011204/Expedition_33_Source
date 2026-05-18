#include "ComputeSpriteEmitter.h"

#include "GameInstance.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "VIBufferCom_ComputePointParticle.h"

IMPLEMENT_REFLECTION(ComputeSpriteEmitter)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "ComputeSpriteEmitter";
    info.category = "EffectEmitter";

    return true;
}

ComputeSpriteEmitter::ComputeSpriteEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : EffectEmitter{ device, context }
{
}

ComputeSpriteEmitter::ComputeSpriteEmitter(const ComputeSpriteEmitter& prototype)
    : EffectEmitter{ prototype }
{
}

ComputeSpriteEmitter::~ComputeSpriteEmitter()
{
    Free();
}

HRESULT ComputeSpriteEmitter::Initialize_Prototype()
{
    return S_OK;
}

HRESULT ComputeSpriteEmitter::Initialize(void* arg)
{
    if (nullptr != arg)
        _desc = *static_cast<ComputeSpriteEmitterDesc*>(arg);

    CHECK_FAILED(__super::Initialize(&_desc), E_FAIL);
    Reset_PlaybackRuntime();

    CHECK_FAILED(Ready_Components(), E_FAIL);

    return S_OK;
}

void ComputeSpriteEmitter::Update(float timeDelta)
{
    Sync_FromEffectOwner();

    if (nullptr != _viBuffer)
    {
        const PlaybackTick playbackTick = Advance_Playback(timeDelta);
        _viBuffer->Dispatch_Compute(
            playbackTick.computeDeltaTime,
            playbackTick.spawnRequest,
            playbackTick.killActiveParticles
        );
    }
}

void ComputeSpriteEmitter::Late_Update(float timeDelta)
{
    if (Can_SubmitRender())
        GAME->Add_RenderGroup(RenderGroup::Blend, GetSharedPtr<GameObject>());
}

HRESULT ComputeSpriteEmitter::Render()
{
    CHECK_FAILED_ONCE(Bind_ShaderResources(), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Begin(_effectSpritePassIndex), E_FAIL);
    CHECK_FAILED_ONCE(_viBuffer->Bind_Resources(), E_FAIL);
    CHECK_FAILED_ONCE(_viBuffer->Render(), E_FAIL);

    return S_OK;
}

HRESULT ComputeSpriteEmitter::Ready_Components()
{
    string resolvedTextureId{};
    CHECK_FAILED(Ready_TextureComponent(resolvedTextureId), E_FAIL);
    CHECK_FAILED(Ready_ShaderComponent(), E_FAIL);
    CHECK_FAILED(Ready_NoiseTextureComponent(resolvedTextureId), E_FAIL);

    ComputePointParticleDesc desc = Make_ComputePointParticleDesc();
    CHECK_FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("Com_Effect_ComputePointParticleBuffer"), _viBuffer, &desc), E_FAIL);

    LOG_INFO(
        "ComputeSpriteEmitter ready: shaderId='{}', textureId='{}', blendMode='{}', passIndex={}, noiseTextureId='{}', opacitySource='{}', noiseStrength={}",
        kEffectSpriteShaderId,
        _desc.textureId,
        _desc.blendMode,
        _effectSpritePassIndex,
        _desc.noiseTextureId,
        _desc.opacitySource,
        _desc.noiseStrength
    );

    return S_OK;
}

HRESULT ComputeSpriteEmitter::Ready_TextureComponent(string& outResolvedTextureId)
{
    const string requestedTextureId =
        !_desc.textureId.empty() ? _desc.textureId : string{ kFallbackTextureId };

    if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), String::ToWString(requestedTextureId), _texture)))
    {
        if (requestedTextureId == kFallbackTextureId)
        {
            LOG_WARN("ComputeSpriteEmitter failed to resolve fallback textureId='{}'.", kFallbackTextureId);
            return E_FAIL;
        }

        CHECK_FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), String::ToWString(kFallbackTextureId), _texture), E_FAIL);

        outResolvedTextureId = kFallbackTextureId;
        return S_OK;
    }

    outResolvedTextureId = requestedTextureId;
    return S_OK;
}

HRESULT ComputeSpriteEmitter::Ready_ShaderComponent()
{
    CHECK_FAILED(__super::Add_Component(ETOI(LevelType::Static), String::ToWString(kEffectSpriteShaderId), _shader), E_FAIL);
    _opacitySourceIndex = Resolve_OpacitySourceIndex();
    _effectSpritePassIndex = Resolve_EffectSpritePassIndex();
    return S_OK;
}

HRESULT ComputeSpriteEmitter::Ready_NoiseTextureComponent(const string& outResolvedTextureId)
{
    if (_desc.noiseTextureId.empty())
        return S_OK;

    if (_desc.noiseTextureId == outResolvedTextureId && nullptr != _texture)
    {
        _noiseTexture = _texture;
        return S_OK;
    }

    if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), String::ToWString(_desc.noiseTextureId), _noiseTexture)))
        _noiseTexture = nullptr;

    return S_OK;
}

HRESULT ComputeSpriteEmitter::Bind_ShaderResources()
{
    CHECK_FAILED_ONCE(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix"), E_FAIL);
    CHECK_FAILED_ONCE(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix"), E_FAIL);
    CHECK_FAILED_ONCE(GAME->Bind_CamPosition(_shader.get(), "g_BillboardTarget"), E_FAIL);
    const int screenAlignmentMode = static_cast<int>(_desc.screenAlignment);
    const int directionalAlignmentMode = static_cast<int>(_desc.directionalAlignmentMode);
    const int spriteTextureAxis = static_cast<int>(_desc.spriteTextureAxis);
    const float spriteRollOffsetRadians = XMConvertToRadians(_desc.spriteRollOffsetDegrees);
    const Vec4 emitterCenter = nullptr != _transformCom
                               ? Vec4{ _transformCom->Get_WorldPosition().x, _transformCom->Get_WorldPosition().y, _transformCom->Get_WorldPosition().z, 1.f }
                               : Vec4{ _desc.center.x, _desc.center.y, _desc.center.z, 1.f };
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_ScreenAlignmentMode", &screenAlignmentMode, sizeof(screenAlignmentMode)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_DirectionalAlignmentMode", &directionalAlignmentMode, sizeof(directionalAlignmentMode)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_SpriteTextureAxis", &spriteTextureAxis, sizeof(spriteTextureAxis)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_SpriteRollOffsetRadians", &spriteRollOffsetRadians, sizeof(spriteRollOffsetRadians)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteEmitterCenter", &emitterCenter, sizeof(emitterCenter)), E_FAIL);
    // 현재 sprite effect 경로에서는 주 텍스처를 lit base color가 아니라 emissive-like color + opacity source로 해석한다.
    CHECK_FAILED_ONCE(_texture->Bind_ShaderResourceView(_shader.get(), "g_Texture", 0), E_FAIL);
    CHECK_FAILED_ONCE(Bind_MaterialResources(), E_FAIL);

    return S_OK;
}

HRESULT ComputeSpriteEmitter::Bind_MaterialResources()
{
    if (nullptr != _noiseTexture)
        CHECK_FAILED_ONCE(_noiseTexture->Bind_ShaderResourceView(_shader.get(), "g_NoiseTexture", 0), E_FAIL);

    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_Tint", &_desc.tint, sizeof(_desc.tint)), E_FAIL);

    const Vec4 effectSpriteParams = Vec4(
        _desc.intensity,
        _desc.opacityPower,
        _desc.noiseStrength,
        nullptr != _noiseTexture ? 1.f : 0.f
    );
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteParams", &effectSpriteParams, sizeof(effectSpriteParams)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_OpacitySource", &_opacitySourceIndex, sizeof(_opacitySourceIndex)), E_FAIL);

    return S_OK;
}

int ComputeSpriteEmitter::Resolve_OpacitySourceIndex() const
{
    if (_desc.opacitySource == "Alpha" || _desc.opacitySource == "alpha")
        return kOpacitySourceAlpha;

    if (_desc.opacitySource == "Red" || _desc.opacitySource == "red")
        return kOpacitySourceRed;

    if (_desc.opacitySource == "Luminance" || _desc.opacitySource == "luminance")
        return kOpacitySourceLuminance;

    return kOpacitySourceAlpha;
}

uint32 ComputeSpriteEmitter::Resolve_EffectSpritePassIndex() const
{
    if (_desc.blendMode == "AlphaBlend" || _desc.blendMode == "alphaBlend" || _desc.blendMode == "alphablend")
        return kEffectSpriteAlphaBlendPass;

    if (_desc.blendMode == "Additive" || _desc.blendMode == "additive")
        return kEffectSpriteAdditivePass;

    LOG_WARN(
        "ComputeSpriteEmitter unknown blendMode='{}'. Falling back to AlphaBlend.",
        _desc.blendMode
    );

    return kEffectSpriteAlphaBlendPass;
}

ComputePointParticleDesc ComputeSpriteEmitter::Make_ComputePointParticleDesc() const
{
    ComputePointParticleDesc desc{};
    desc.instanceCount = max(1u, _desc.instanceCount);
    desc.center = _desc.center;
    desc.range = _desc.spawnRange;
    desc.sizeMin = _desc.sizeMin;
    desc.sizeMax = _desc.sizeMax;
    desc.lifeTime = _desc.lifeTime;
    desc.startColorMin = _desc.startColorMin;
    desc.startColorMax = _desc.startColorMax;
    desc.endColorMin = _desc.endColorMin;
    desc.endColorMax = _desc.endColorMax;
    desc.subUVRows = max(1u, _desc.subUVRows);
    desc.subUVCols = max(1u, _desc.subUVCols);
    desc.subUvFrameOverLife = _desc.subUVFrameOverLife;
    desc.sizeByLife = _desc.sizeByLife;
    desc.motion = _desc.motion;
    desc.rotation = _desc.rotation;
    desc.spawn = _desc.spawn;
    desc.initialLocation = _desc.initialLocation;
    desc.sphereLocation = _desc.sphereLocation;
    if (nullptr != _transformCom)
    {
        desc.emitterRight = _transformCom->Get_WorldRight();
        desc.emitterUp = _transformCom->Get_WorldUp();
        desc.emitterLook = _transformCom->Get_WorldForward();
    }
    desc.spawn.maxActiveCount = min(max(1u, desc.spawn.maxActiveCount), desc.instanceCount);
    desc.useMaxDrawCount = _desc.useMaxDrawCount;
    desc.maxDrawCount = _desc.useMaxDrawCount ? min(desc.instanceCount, max(1u, _desc.maxDrawCount)) : desc.instanceCount;
    desc.drawMode = PointParticleDrawMode::DrawIndexedInstancedIndirect;
    desc.computeShaderLevelIndex = ETOI(LevelType::Static);
    desc.computeShaderPrototypeTag = L"ComputeShader_PointParticle";

    return desc;
}

uint32 ComputeSpriteEmitter::Consume_SpawnRequest(float timeDelta)
{
    const float safeDeltaTime = max(0.f, timeDelta);
    const float previousTime = _emitterElapsedTime;
    _emitterElapsedTime += safeDeltaTime;

    uint32 spawnRequest = 0u;

    if (_desc.spawn.processSpawnRate)
    {
        const float spawnRate = max(0.f, _desc.spawn.spawnRate * _desc.spawn.spawnRateScale);
        _spawnAccumulator += spawnRate * safeDeltaTime;

        const float wholeSpawn = floorf(_spawnAccumulator);
        if (wholeSpawn > 0.f)
        {
            spawnRequest += static_cast<uint32>(wholeSpawn);
            _spawnAccumulator -= wholeSpawn;
        }
    }

    if (_desc.spawn.processBurstList)
    {
        if (_burstExecuted.size() != _desc.spawn.bursts.size())
            _burstExecuted.assign(_desc.spawn.bursts.size(), false);

        for (size_t index = 0; index < _desc.spawn.bursts.size(); ++index)
        {
            if (_burstExecuted[index])
                continue;

            const PointParticleBurstDesc& burst = _desc.spawn.bursts[index];
            if (previousTime > burst.time || _emitterElapsedTime < burst.time)
                continue;

            const float scaledCount = static_cast<float>(burst.count) * max(0.f, _desc.spawn.burstScale);
            spawnRequest += static_cast<uint32>(floorf(scaledCount));
            _burstExecuted[index] = true;
        }
    }

    return min(spawnRequest, max(1u, _desc.spawn.maxActiveCount));
}

void ComputeSpriteEmitter::Reset_PlaybackRuntime()
{
    _burstExecuted.assign(_desc.spawn.bursts.size(), false);
    _emitterElapsedTime = 0.f;
    _spawnAccumulator = 0.f;
    _loopElapsedTime = 0.f;
    _loopIndex = 0u;
    _completedKillDispatched = false;
    _playbackState = Resolve_CurrentLoopDelay() > 0.f ? PlaybackState::Delayed : PlaybackState::Playing;
}

ComputeSpriteEmitter::PlaybackTick ComputeSpriteEmitter::Advance_Playback(float timeDelta)
{
    PlaybackTick tick{};
    const float safeDeltaTime = max(0.f, timeDelta);

    if (PlaybackState::Completed == _playbackState)
    {
        tick.computeDeltaTime = _desc.killOnCompleted ? 0.f : safeDeltaTime;
        if (_desc.killOnCompleted && !_completedKillDispatched)
        {
            tick.killActiveParticles = true;
            _completedKillDispatched = true;
        }
        return tick;
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
            tick.computeDeltaTime += playbackStep;
            tick.spawnRequest += Consume_SpawnRequest(playbackStep);
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
        if (_desc.killOnCompleted && !_completedKillDispatched)
        {
            tick.killActiveParticles = true;
            _completedKillDispatched = true;
        }
        break;
    }

    if (PlaybackState::Completed == _playbackState && !_desc.killOnCompleted)
        tick.computeDeltaTime += remainingTime;

    return tick;
}

void ComputeSpriteEmitter::Start_NextLoop()
{
    ++_loopIndex;
    _loopElapsedTime = 0.f;
    _emitterElapsedTime = 0.f;
    _spawnAccumulator = 0.f;
    _burstExecuted.assign(_desc.spawn.bursts.size(), false);
    _playbackState = Resolve_CurrentLoopDelay() > 0.f ? PlaybackState::Delayed : PlaybackState::Playing;
}

float ComputeSpriteEmitter::Resolve_CurrentLoopDelay() const
{
    if (_desc.delayFirstLoopOnly && _loopIndex > 0u)
        return 0.f;

    return max(0.f, _desc.delay);
}

float ComputeSpriteEmitter::Resolve_Duration() const
{
    return max(0.0001f, _desc.duration);
}

bool ComputeSpriteEmitter::Has_NextLoop() const
{
    return _desc.loopCount == 0u || _loopIndex + 1u < _desc.loopCount;
}

bool ComputeSpriteEmitter::Can_SubmitRender() const
{
    if (PlaybackState::Delayed == _playbackState)
        return false;

    if (PlaybackState::Completed == _playbackState && _desc.killOnCompleted)
        return false;

    return nullptr != _viBuffer;
}

void ComputeSpriteEmitter::Sync_WorldCenterToComputeBuffer()
{
    CHECK_NULL_THROTTLED(_transformCom, 120);
    CHECK_NULL_THROTTLED(_viBuffer, 120);

    _viBuffer->Set_Center(_transformCom->Get_WorldPosition());
    _viBuffer->Set_EmitterBasis(
        _transformCom->Get_WorldRight(),
        _transformCom->Get_WorldUp(),
        _transformCom->Get_WorldForward()
    );
}

void ComputeSpriteEmitter::On_EffectTransformSynced()
{
    Sync_WorldCenterToComputeBuffer();
}

Shared<ComputeSpriteEmitter> ComputeSpriteEmitter::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<ComputeSpriteEmitter>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : ComputeSpriteEmitter");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> ComputeSpriteEmitter::Clone(void* arg)
{
    auto instance = make_shared<ComputeSpriteEmitter>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : ComputeSpriteEmitter");
        MSG_BOX("Failed to Clone : ComputeSpriteEmitter");
        return nullptr;
    }

    return instance;
}

void ComputeSpriteEmitter::Free()
{
}
