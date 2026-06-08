#include "ComputeRibbonEmitter.h"

#include <execution>
#include "ComputeShaderCom.h"
#include "ComputeStructuredBuffer.h"
#include "GameInstance.h"
#include "Renderer_Define.h"
#include "ShaderCom.h"
#include "Texture.h"

namespace Client::Internal
{
static Vec4 Average_Color(const Vec4& minColor, const Vec4& maxColor)
{
    return (minColor + maxColor) * 0.5f;
}

static Vec3 Average_Vector(const Vec3& minValue, const Vec3& maxValue)
{
    return (minValue + maxValue) * 0.5f;
}
}

IMPLEMENT_REFLECTION(ComputeRibbonEmitter)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "ComputeRibbonEmitter";
    info.category = "Effect";

    return true;
}

ComputeRibbonEmitter::ComputeRibbonEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : EffectEmitter{ device, context }
{
}

ComputeRibbonEmitter::ComputeRibbonEmitter(const ComputeRibbonEmitter& prototype)
    : EffectEmitter{ prototype }
{
}

ComputeRibbonEmitter::~ComputeRibbonEmitter()
{
    Free();
}

HRESULT ComputeRibbonEmitter::Initialize_Prototype()
{
    return S_OK;
}

HRESULT ComputeRibbonEmitter::Initialize(void* arg)
{
    if (nullptr != arg)
        _desc = *static_cast<ComputeRibbonEmitterDesc*>(arg);

    _desc.playback.duration = max(0.0001f, _desc.playback.duration);
    _desc.playback.delay = max(0.f, _desc.playback.delay);
    _desc.spawn.instanceCount = max(1u, _desc.spawn.instanceCount);
    _desc.spawn.particleSpawn.maxActiveCount = min(
        max(1u, _desc.spawn.particleSpawn.maxActiveCount),
        _desc.spawn.instanceCount
    );
    _desc.lifetime.lifeTime.x = max(0.0001f, _desc.lifetime.lifeTime.x);
    _desc.lifetime.lifeTime.y = max(_desc.lifetime.lifeTime.x, _desc.lifetime.lifeTime.y);
    _desc.maxSampleCount = max(2u, _desc.maxSampleCount);
    _desc.tilingDistance = max(0.f, _desc.tilingDistance);
    _desc.distanceTessellationStepSize = max(0.001f, _desc.distanceTessellationStepSize);

    CHECK_FAILED(__super::Initialize(&_desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);
    Reset_PlaybackRuntime();

    return S_OK;
}

void ComputeRibbonEmitter::Update(float timeDelta)
{
    Sync_FromEffectOwner();
    Advance_Playback(timeDelta);
    _materialElapsedTime += max(0.f, timeDelta);

    if (PlaybackState::Playing == _playbackState)
        Update_Samples(timeDelta);

    CHECK_FAILED_THROTTLED(Dispatch_Compute(), 60);
}

void ComputeRibbonEmitter::Late_Update(float)
{
    if (Can_SubmitRender())
        GAME->Add_RenderGroup(RenderGroup::Blend, GetSharedPtr<GameObject>());
}

HRESULT ComputeRibbonEmitter::Render()
{
    CHECK_FAILED_THROTTLED(Bind_ShaderResources(), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Begin(_desc.material.blendMode == EffectMaterialBlendMode::Additive ? 1u : 0u), 60, E_FAIL);

    constexpr uint32 stride0 = sizeof(Vec3);
    constexpr uint32 offset0 = 0;
    constexpr uint32 stride1 = sizeof(RibbonInstanceVertex);
    constexpr uint32 offset1 = 0;
    Buffer* vertexBuffers[]{ _pointVB.Get(), _instanceBuffer.Get() };
    const uint32 strides[]{ stride0, stride1 };
    const uint32 offsets[]{ offset0, offset1 };

    _context->IASetVertexBuffers(0, 2, vertexBuffers, strides, offsets);
    _context->IASetIndexBuffer(_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    _context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    _context->DrawIndexedInstancedIndirect(_indirectArgsBuffer.Get(), 0);
    return S_OK;
}

HRESULT ComputeRibbonEmitter::Reset_ForEffectReplay()
{
    Reset_PlaybackRuntime();
    _samples.clear();
    _burstExecuted.assign(_desc.spawn.particleSpawn.bursts.size(), false);
    _spawnAccumulator = 0.f;
    _materialElapsedTime = 0.f;
    _visibleRibbonLength = 1.f;
    _drawSegmentCount = 0u;

    if (nullptr != _computeArgsOutput)
        CHECK_FAILED(Reset_IndirectArgs(), E_FAIL);
    if (nullptr != _computeOutput && nullptr != _computeArgsOutput)
        CHECK_FAILED(Copy_ComputeOutput(), E_FAIL);

    return S_OK;
}

bool ComputeRibbonEmitter::Is_EffectFinished() const
{
    return PlaybackState::Completed == _playbackState;
}

bool ComputeRibbonEmitter::Try_Get_BlendSortWorldPosition(Vec3& outWorldPosition) const
{
    if (nullptr == _transformCom)
        return false;

    outWorldPosition = _transformCom->Get_WorldPosition();
    return true;
}

EffectSortPolicy ComputeRibbonEmitter::Get_BlendSortPolicy() const
{
    return _desc.sort.sortPolicy;
}

int32 ComputeRibbonEmitter::Get_BlendSortLayer() const
{
    return _desc.sort.sortLayer;
}

float ComputeRibbonEmitter::Get_BlendSortBias() const
{
    return _desc.sort.artistSortBias;
}

void ComputeRibbonEmitter::Reset_PlaybackRuntime()
{
    _playbackState = PlaybackState::Delayed;
    _loopElapsedTime = 0.f;
    _loopIndex = 0u;
}

void ComputeRibbonEmitter::Advance_Playback(float timeDelta)
{
    if (PlaybackState::Completed == _playbackState)
        return;

    const float safeDeltaTime = max(0.f, timeDelta);
    _loopElapsedTime += safeDeltaTime;

    const float delay = Resolve_CurrentLoopDelay();
    if (_loopElapsedTime < delay)
    {
        _playbackState = PlaybackState::Delayed;
        return;
    }

    _playbackState = PlaybackState::Playing;
    const float activeElapsedTime = _loopElapsedTime - delay;
    if (activeElapsedTime < Resolve_Duration())
        return;

    if (Has_NextLoop())
    {
        Start_NextLoop();
        return;
    }

    _playbackState = PlaybackState::Completed;
}

HRESULT ComputeRibbonEmitter::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), kRibbonShaderId, _shader), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), kRibbonComputeShaderId, _computeShader), E_FAIL);

    _mainTexture = Texture::Create(_device, _context, kFallbackTexturePath, 1);
    CHECK_NULL(_mainTexture, E_FAIL);

    CHECK_FAILED(Ready_DrawBuffers(), E_FAIL);
    CHECK_FAILED(Ready_ComputeBuffers(), E_FAIL);
    return S_OK;
}

HRESULT ComputeRibbonEmitter::Ready_DrawBuffers()
{
    const Vec3 point{};
    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = sizeof(Vec3);
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData{};
    vbData.pSysMem = &point;
    CHECK_FAILED(_device->CreateBuffer(&vbDesc, &vbData, _pointVB.GetAddressOf()), E_FAIL);

    const uint16 index = 0;
    D3D11_BUFFER_DESC ibDesc{};
    ibDesc.ByteWidth = sizeof(uint16);
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibData{};
    ibData.pSysMem = &index;
    CHECK_FAILED(_device->CreateBuffer(&ibDesc, &ibData, _indexBuffer.GetAddressOf()), E_FAIL);

    D3D11_BUFFER_DESC instanceDesc{};
    instanceDesc.ByteWidth = sizeof(RibbonInstanceVertex) * Compute_MaxSegmentCount();
    instanceDesc.Usage = D3D11_USAGE_DEFAULT;
    instanceDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    CHECK_FAILED(_device->CreateBuffer(&instanceDesc, nullptr, _instanceBuffer.GetAddressOf()), E_FAIL);
    return S_OK;
}

HRESULT ComputeRibbonEmitter::Ready_ComputeBuffers()
{
    const uint32 maxSegmentCount = Compute_MaxSegmentCount();
    _sampleInput = ComputeStructuredBuffer::Create(_device, _context, sizeof(RibbonSamplePayload), maxSegmentCount + 1u);
    CHECK_NULL(_sampleInput, E_FAIL);
    _computeOutput = ComputeStructuredBuffer::Create(_device, _context, sizeof(RibbonInstanceVertex), maxSegmentCount);
    CHECK_NULL(_computeOutput, E_FAIL);
    _computeArgsOutput = ComputeStructuredBuffer::Create(_device, _context, sizeof(DrawIndexedInstancedIndirectArgs), 1);
    CHECK_NULL(_computeArgsOutput, E_FAIL);

    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(RibbonComputeParams);
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    CHECK_FAILED(_device->CreateBuffer(&cbDesc, nullptr, _computeConstantBuffer.GetAddressOf()), E_FAIL);

    D3D11_BUFFER_DESC argsDesc{};
    argsDesc.ByteWidth = sizeof(DrawIndexedInstancedIndirectArgs);
    argsDesc.Usage = D3D11_USAGE_DEFAULT;
    argsDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    argsDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
    CHECK_FAILED(_device->CreateBuffer(&argsDesc, nullptr, _indirectArgsBuffer.GetAddressOf()), E_FAIL);
    return Reset_IndirectArgs();
}

void ComputeRibbonEmitter::Update_Samples(float timeDelta)
{
    const float safeDeltaTime = max(0.f, timeDelta);

    for (RibbonSample& sample : _samples)
    {
        sample.age += safeDeltaTime;
        sample.position += sample.velocity * safeDeltaTime;
    }

    _samples.erase(
        remove_if(
            _samples.begin(),
            _samples.end(),
            [](const RibbonSample& sample)
            {
                return sample.age >= sample.lifetime;
            }
        ),
        _samples.end()
    );

    uint32 spawnRequest = 0u;
    const auto& particleSpawn = _desc.spawn.particleSpawn;
    if (particleSpawn.processSpawnRate)
    {
        const float spawnRate = max(0.f, particleSpawn.spawnRate * particleSpawn.spawnRateScale);
        _spawnAccumulator += spawnRate * safeDeltaTime;
        const float wholeSpawn = floorf(_spawnAccumulator + 0.0001f);
        if (wholeSpawn > 0.f)
        {
            spawnRequest += static_cast<uint32>(wholeSpawn);
            _spawnAccumulator -= wholeSpawn;
        }
    }

    if (particleSpawn.processBurstList)
    {
        if (_burstExecuted.size() != particleSpawn.bursts.size())
            _burstExecuted.assign(particleSpawn.bursts.size(), false);

        const float activeElapsedTime = max(0.f, _loopElapsedTime - Resolve_CurrentLoopDelay());
        for (size_t index = 0; index < particleSpawn.bursts.size(); ++index)
        {
            const PointParticleBurstDesc& burst = particleSpawn.bursts[index];
            if (_burstExecuted[index] || activeElapsedTime < burst.time)
                continue;

            spawnRequest += burst.count;
            _burstExecuted[index] = true;
        }
    }

    if (_desc.spawnInitialParticle && _samples.empty())
        ++spawnRequest;

    const uint32 room = _desc.maxSampleCount > static_cast<uint32>(_samples.size())
                        ? _desc.maxSampleCount - static_cast<uint32>(_samples.size())
                        : 0u;
    spawnRequest = min(spawnRequest, room);

    for (uint32 index = 0; index < spawnRequest; ++index)
        Spawn_RibbonSample();

    if (!_samples.empty())
    {
        _samples.front().distance = 0.f;
        for (size_t index = 1; index < _samples.size(); ++index)
        {
            const Vec3 delta = _samples[index].position - _samples[index - 1].position;
            _samples[index].distance = _samples[index - 1].distance + delta.Length();
        }
        _visibleRibbonLength = max(0.0001f, _samples.back().distance);
    }
    else
        _visibleRibbonLength = 1.f;
}

void ComputeRibbonEmitter::Spawn_RibbonSample()
{
    RibbonSample sample{};
    const Vec3 ownerPosition = _transformCom != nullptr ? _transformCom->Get_WorldPosition() : Vec3{};
    Vec3 localOffset{};
    if (_desc.initialLocation.enabled)
        localOffset += Internal::Average_Vector(_desc.initialLocation.minOffset, _desc.initialLocation.maxOffset);
    if (_desc.sphereLocation.enabled)
        localOffset += _desc.sphereLocation.offset;

    sample.position = ownerPosition + localOffset;
    sample.velocity = _desc.motion.initialVelocityEnabled
                      ? Internal::Average_Vector(_desc.motion.initialVelocityMin, _desc.motion.initialVelocityMax)
                      : Vec3{};
    sample.age = 0.f;
    sample.lifetime = max(0.0001f, _desc.lifetime.lifeTime.y);
    sample.distance = 0.f;
    sample.startColor = Internal::Average_Color(_desc.initialColor.startColorMin, _desc.initialColor.startColorMax);
    sample.endColor = Internal::Average_Color(_desc.colorOverLife.endColorMin, _desc.colorOverLife.endColorMax);

    _samples.insert(_samples.begin(), sample);
    while (static_cast<uint32>(_samples.size()) > _desc.maxSampleCount)
    {
        _samples.pop_back();
    }
}

HRESULT ComputeRibbonEmitter::Dispatch_Compute()
{
    CHECK_FAILED(Reset_IndirectArgs(), E_FAIL);

    if (_samples.size() < 2)
    {
        _drawSegmentCount = 0u;
        CHECK_FAILED(Copy_ComputeOutput(), E_FAIL);
        return S_OK;
    }

    CHECK_FAILED(Update_ComputeInput(), E_FAIL);
    if (_drawSegmentCount == 0u)
    {
        CHECK_FAILED(Copy_ComputeOutput(), E_FAIL);
        return S_OK;
    }

    CHECK_FAILED(Update_ComputeConstants(), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_SRV(0, _sampleInput->Get_SRV()), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_UAV(0, _computeOutput->Get_UAV()), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_UAV(1, _computeArgsOutput->Get_UAV()), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_ConstantBuffer(0, _computeConstantBuffer.Get()), E_FAIL);
    CHECK_FAILED(_computeShader->Dispatch((_drawSegmentCount + kThreadCountX - 1u) / kThreadCountX, 1, 1), E_FAIL);
    CHECK_FAILED(Copy_ComputeOutput(), E_FAIL);
    return S_OK;
}

HRESULT ComputeRibbonEmitter::Update_ComputeInput()
{
    if (_samples.size() < 2)
    {
        _drawSegmentCount = 0u;
        return S_OK;
    }

    _drawSegmentCount = min(static_cast<uint32>(_samples.size() - 1u), Compute_MaxSegmentCount());
    vector<RibbonSamplePayload> payloads{};
    payloads.reserve(_drawSegmentCount + 1u);

    for (uint32 index = 0; index <= _drawSegmentCount; ++index)
    {
        const RibbonSample& sample = _samples[index];
        const float lifeProgress = clamp(sample.age / max(sample.lifetime, 0.0001f), 0.f, 1.f);
        const float widthScale = _desc.sizeByLife.enabled
                                 ? lerp(_desc.sizeByLife.multiplyXStart, _desc.sizeByLife.multiplyXEnd, lifeProgress)
                                 : 1.f;

        RibbonSamplePayload payload{};
        payload.currentWorldPosition = Vec4(sample.position.x, sample.position.y, sample.position.z, 1.f);
        payload.sampleParams = Vec4(sample.age, sample.lifetime, sample.distance, max(0.001f, widthScale));
        payload.startColor = sample.startColor;
        payload.endColor = sample.endColor;
        payloads.push_back(payload);
    }

    CHECK_FAILED(_sampleInput->Update_Data(payloads.data(), static_cast<uint32>(payloads.size())), E_FAIL);
    return S_OK;
}

HRESULT ComputeRibbonEmitter::Update_ComputeConstants()
{
    RibbonComputeParams params{};
    params.segmentCount = _drawSegmentCount;
    params.renderAxis = static_cast<uint32>(_desc.renderAxis);
    params.tilingDistance = _desc.tilingDistance;
    params.visibleLength = _visibleRibbonLength;
    params.axisFallback = Vec4(1.f, 0.f, 0.f, 0.f);
    _context->UpdateSubresource(_computeConstantBuffer.Get(), 0, nullptr, &params, 0, 0);
    return S_OK;
}

HRESULT ComputeRibbonEmitter::Reset_IndirectArgs()
{
    const DrawIndexedInstancedIndirectArgs args{};
    CHECK_FAILED(_computeArgsOutput->Update_Data(&args, 1), E_FAIL);
    return Copy_ComputeOutput();
}

HRESULT ComputeRibbonEmitter::Copy_ComputeOutput()
{
    if (nullptr != _instanceBuffer && nullptr != _computeOutput)
        _context->CopyResource(_instanceBuffer.Get(), _computeOutput->Get_Buffer());
    if (nullptr != _indirectArgsBuffer && nullptr != _computeArgsOutput)
        _context->CopyResource(_indirectArgsBuffer.Get(), _computeArgsOutput->Get_Buffer());
    return S_OK;
}

HRESULT ComputeRibbonEmitter::Bind_ShaderResources()
{
    CHECK_FAILED(GAME->Bind_CameraCB(_shader), E_FAIL);
    CHECK_FAILED(_mainTexture->Bind_ShaderResourceView(_shader.get(), "g_MainTexture", 0), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_Tint", &_desc.material.tint, sizeof(Vec4)), E_FAIL);

    const Vec4 effectRibbonParams{
        _desc.material.intensity,
        max(_desc.material.opacityPower, 0.0001f),
        clamp(_desc.material.alphaCutoff, 0.f, 1.f),
        _materialElapsedTime
    };
    const Vec4 effectRibbonAxisParams{
        static_cast<float>(static_cast<uint32>(_desc.renderAxis)),
        _visibleRibbonLength,
        _desc.tilingDistance,
        0.f
    };
    const Vec4 effectRibbonFallbackAxis{ 1.f, 0.f, 0.f, 0.f };
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonParams", &effectRibbonParams, sizeof(effectRibbonParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonAxisParams", &effectRibbonAxisParams, sizeof(effectRibbonAxisParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonFallbackAxis", &effectRibbonFallbackAxis, sizeof(effectRibbonFallbackAxis)), E_FAIL);
    return S_OK;
}

bool ComputeRibbonEmitter::Can_SubmitRender() const
{
    return _drawSegmentCount > 0u && Is_Visible() && nullptr != _shader &&
           nullptr != _pointVB && nullptr != _indexBuffer && nullptr != _instanceBuffer && nullptr != _indirectArgsBuffer;
}

uint32 ComputeRibbonEmitter::Compute_MaxSegmentCount() const
{
    return max(1u, _desc.maxSampleCount - 1u);
}

void ComputeRibbonEmitter::Start_NextLoop()
{
    ++_loopIndex;
    _loopElapsedTime = 0.f;
    _playbackState = PlaybackState::Delayed;
    _samples.clear();
    _burstExecuted.assign(_desc.spawn.particleSpawn.bursts.size(), false);
    _spawnAccumulator = 0.f;
    _drawSegmentCount = 0u;
}

float ComputeRibbonEmitter::Resolve_CurrentLoopDelay() const
{
    if (_desc.playback.delayFirstLoopOnly && _loopIndex > 0u)
        return 0.f;

    return max(0.f, _desc.playback.delay);
}

float ComputeRibbonEmitter::Resolve_Duration() const
{
    return max(0.0001f, _desc.playback.duration);
}

bool ComputeRibbonEmitter::Has_NextLoop() const
{
    return 0u == _desc.playback.loopCount || _loopIndex + 1u < _desc.playback.loopCount;
}

Shared<ComputeRibbonEmitter> ComputeRibbonEmitter::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<ComputeRibbonEmitter>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : ComputeRibbonEmitter");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> ComputeRibbonEmitter::Clone(void* arg)
{
    auto instance = make_shared<ComputeRibbonEmitter>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : ComputeRibbonEmitter");
        MSG_BOX("Failed to Clone : ComputeRibbonEmitter");
        return nullptr;
    }

    return instance;
}

void ComputeRibbonEmitter::Free()
{
    _computeArgsOutput = nullptr;
    _computeOutput = nullptr;
    _sampleInput = nullptr;
    _indirectArgsBuffer.Reset();
    _computeConstantBuffer.Reset();
    _instanceBuffer.Reset();
    _indexBuffer.Reset();
    _pointVB.Reset();
    _mainTexture = nullptr;
    _computeShader = nullptr;
    _shader = nullptr;
    _samples.clear();
    _burstExecuted.clear();
    _desc = {};
    Reset_PlaybackRuntime();
    __super::Free();
}
