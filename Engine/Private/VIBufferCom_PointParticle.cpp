#pragma warning(push)
#pragma warning(disable: 4996)

#include "VIBufferCom_PointParticle.h"

VIBufferCom_PointParticle::VIBufferCom_PointParticle(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : VIBufferCom_Instance{ device, context }
{
}

VIBufferCom_PointParticle::VIBufferCom_PointParticle(const VIBufferCom_PointParticle& prototype)
    : VIBufferCom_Instance{ prototype }
{
}

VIBufferCom_PointParticle::~VIBufferCom_PointParticle()
{
    Free();
}

HRESULT VIBufferCom_PointParticle::Initialize_Prototype()
{
    _numVertexBuffers = 2;
    _numVertices = 1;
    _vertexStride = sizeof(Vec3);
    _numIndices = 0;
    _indexStride = 0;
    _primitiveType = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;

    _instanceStride = sizeof(ParticleInstanceVertex);
    _verticesPerInstance = 1;

    return Ready_VertexBuffer();
}

HRESULT VIBufferCom_PointParticle::Initialize(void* arg)
{
    if (nullptr == arg)
    {
        LOG_ERROR("VIBufferCom_PointParticle requires PointParticleDesc at clone time.");
        return E_FAIL;
    }

    _desc = *static_cast<PointParticleDesc*>(arg);
    if (0 == _desc.instanceCount)
    {
        LOG_ERROR("VIBufferCom_PointParticle requires at least one instance.");
        return E_FAIL;
    }

    _instanceCount = _desc.instanceCount;

    _instanceBufferDesc = {};
    _instanceBufferDesc.ByteWidth = _instanceStride * _instanceCount;
    _instanceBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    _instanceBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    _instanceBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    _instanceBufferDesc.StructureByteStride = _instanceStride;

    CHECK_FAILED(Ready_InstanceSeedData(), E_FAIL);
    CHECK_FAILED(Ready_InstanceBuffer(), E_FAIL);
    CHECK_FAILED(Ready_IndexedDrawBuffer(), E_FAIL);
    CHECK_FAILED(Ready_IndirectArgsBuffer(), E_FAIL);

    return S_OK;
}

HRESULT VIBufferCom_PointParticle::Render()
{
    if (_instanceCount == 0)
        return E_FAIL;

    switch (_desc.drawMode)
    {
    case PointParticleDrawMode::Direct:
        return __super::Render();

    case PointParticleDrawMode::DrawInstancedIndirect:
        if (nullptr == _indirectArgsBuffer)
            return E_FAIL;

        _context->DrawInstancedIndirect(_indirectArgsBuffer.Get(), 0);
        return S_OK;

    case PointParticleDrawMode::DrawIndexedInstancedIndirect:
        if (nullptr == _indirectArgsBuffer || nullptr == _ib)
            return E_FAIL;

        _context->DrawIndexedInstancedIndirect(_indirectArgsBuffer.Get(), 0);
        return S_OK;

    default:
        return E_FAIL;
    }
}

void VIBufferCom_PointParticle::Drop(float timeDelta)
{
    if (_runtimePayload.empty())
        return;

    for (uint32 i = 0; i < _instanceCount; ++i)
    {
        ParticleInstanceVertex& instance = _runtimePayload[i];

        if (!_desc.loop && Is_Expired(instance))
            continue;

        // 위에서 아래로 단순히 내리는 효과
        instance.translation.y -= _speeds[i] * timeDelta;
        instance.lifeTime.y += timeDelta;
        Update_InstanceOverLifePayload(i);

        if (_desc.loop && Is_Expired(instance))
            instance = _baselinePayload[i];
        else if (!_desc.loop && Is_Expired(instance))
            instance.lifeTime.y = instance.lifeTime.x;
    }

    CHECK_FAILED(Upload_InstancePayload());
}

void VIBufferCom_PointParticle::Spread(float timeDelta)
{
    if (_runtimePayload.empty())
        return;

    for (uint32 i = 0; i < _instanceCount; ++i)
    {
        ParticleInstanceVertex& instance = _runtimePayload[i];

        if (!_desc.loop && Is_Expired(instance))
            continue;

        // 퍼지는 효과
        Vec3 translation{ instance.translation.x, instance.translation.y, instance.translation.z };
        Vec3 direction = translation - _desc.pivot;
        if (direction.LengthSquared() <= 1e-6f)
            direction = Vec3{ 0.f, 1.f, 0.f };

        direction.Normalize();
        translation += direction * (_speeds[i] * timeDelta);

        instance.translation = Vec4{ translation.x, translation.y, translation.z, 1.f };
        instance.lifeTime.y += timeDelta;
        Update_InstanceOverLifePayload(i);

        if (_desc.loop && Is_Expired(instance))
            instance = _baselinePayload[i];
        else if (!_desc.loop && Is_Expired(instance))
            instance.lifeTime.y = instance.lifeTime.x;
    }

    CHECK_FAILED(Upload_InstancePayload());
}

HRESULT VIBufferCom_PointParticle::Ready_VertexBuffer()
{
    D3D11_BUFFER_DESC vertexBufferDesc{};
    vertexBufferDesc.ByteWidth = _vertexStride * _numVertices;
    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.StructureByteStride = _vertexStride;

    const Vec3 vertex{ 0.f, 0.f, 0.f };

    D3D11_SUBRESOURCE_DATA initialData{};
    initialData.pSysMem = &vertex;

    CHECK_FAILED(_device->CreateBuffer(&vertexBufferDesc, &initialData, _vb.GetAddressOf()), E_FAIL);
    return S_OK;
}

HRESULT VIBufferCom_PointParticle::Ready_InstanceSeedData()
{
    _baselinePayload.clear();
    _runtimePayload.clear();
    _speeds.clear();

    _baselinePayload.reserve(_instanceCount);
    _runtimePayload.reserve(_instanceCount);
    _speeds.reserve(_instanceCount);

    for (uint32 i = 0; i < _instanceCount; ++i)
    {
        const float scale = Pick_RandomFloat(_desc.scale.x, _desc.scale.y);
        const float speed = Pick_RandomFloat(_desc.speed.x, _desc.speed.y);
        const float lifeTime = Pick_RandomFloat(_desc.lifeTime.x, _desc.lifeTime.y);
        const Vec4 startColor{
            Pick_RandomFloat(_desc.startColorMin.x, _desc.startColorMax.x),
            Pick_RandomFloat(_desc.startColorMin.y, _desc.startColorMax.y),
            Pick_RandomFloat(_desc.startColorMin.z, _desc.startColorMax.z),
            Pick_RandomFloat(_desc.startColorMin.w, _desc.startColorMax.w)
        };
        const Vec4 endColor{
            Pick_RandomFloat(_desc.endColorMin.x, _desc.endColorMax.x),
            Pick_RandomFloat(_desc.endColorMin.y, _desc.endColorMax.y),
            Pick_RandomFloat(_desc.endColorMin.z, _desc.endColorMax.z),
            Pick_RandomFloat(_desc.endColorMin.w, _desc.endColorMax.w)
        };

        ParticleInstanceVertex instance{};
        instance.right = Vec4{ scale, 0.f, 0.f, 0.f };
        instance.up = Vec4{ 0.f, scale, 0.f, 0.f };
        instance.look = Vec4{ 0.f, 0.f, scale, 0.f };
        instance.translation = Vec4{
            Pick_RandomFloat(_desc.center.x - _desc.range.x * 0.5f, _desc.center.x + _desc.range.x * 0.5f),
            Pick_RandomFloat(_desc.center.y - _desc.range.y * 0.5f, _desc.center.y + _desc.range.y * 0.5f),
            Pick_RandomFloat(_desc.center.z - _desc.range.z * 0.5f, _desc.center.z + _desc.range.z * 0.5f),
            1.f
        };
        instance.lifeTime = Vec2{ lifeTime, 0.f };
        instance.startColor = startColor;
        instance.endColor = endColor;
        instance.subUVRect = Evaluate_SubUVRect(0.f, 0.f, i);

        // baseline은 loop reset 기준이고, runtime은 매 프레임 GPU에 업로드될 clone 전용 payload다.
        _baselinePayload.push_back(instance);
        _runtimePayload.push_back(instance);
        _speeds.push_back(speed);
    }

    return S_OK;
}

HRESULT VIBufferCom_PointParticle::Ready_InstanceBuffer()
{
    D3D11_SUBRESOURCE_DATA initialData{};
    initialData.pSysMem = _runtimePayload.data();

    CHECK_FAILED(_device->CreateBuffer(&_instanceBufferDesc, &initialData, _instanceVB.GetAddressOf()), E_FAIL);
    return S_OK;
}

HRESULT VIBufferCom_PointParticle::Upload_InstancePayload()
{
    if (nullptr == _instanceVB.Get() || _runtimePayload.empty())
        return E_FAIL;

    D3D11_MAPPED_SUBRESOURCE mappedSubResource{};
    CHECK_FAILED(_context->Map(_instanceVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubResource), E_FAIL);

    memcpy(mappedSubResource.pData, _runtimePayload.data(), sizeof(ParticleInstanceVertex) * _runtimePayload.size());
    _context->Unmap(_instanceVB.Get(), 0);

    return S_OK;
}

HRESULT VIBufferCom_PointParticle::Ready_IndexedDrawBuffer()
{
    if (PointParticleDrawMode::DrawIndexedInstancedIndirect != _desc.drawMode)
        return S_OK;

    _numIndices = 1;
    _indexStride = sizeof(uint16);

    D3D11_BUFFER_DESC indexBufferDesc{};
    indexBufferDesc.ByteWidth = _indexStride * _numIndices;
    indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexBufferDesc.StructureByteStride = _indexStride;

    const uint16 index = 0;

    D3D11_SUBRESOURCE_DATA initialData{};
    initialData.pSysMem = &index;

    CHECK_FAILED(_device->CreateBuffer(&indexBufferDesc, &initialData, _ib.GetAddressOf()), E_FAIL);
    return S_OK;
}

HRESULT VIBufferCom_PointParticle::Ready_IndirectArgsBuffer()
{
    if (PointParticleDrawMode::Direct == _desc.drawMode)
        return S_OK;

    D3D11_BUFFER_DESC argsBufferDesc{};
    argsBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    argsBufferDesc.BindFlags = 0;
    argsBufferDesc.CPUAccessFlags = 0;
    argsBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;

    D3D11_SUBRESOURCE_DATA initialData{};

    DrawInstancedIndirectArgs instancedArgs{};
    DrawIndexedInstancedIndirectArgs indexedArgs{};

    if (PointParticleDrawMode::DrawInstancedIndirect == _desc.drawMode)
    {
        instancedArgs.vertexCountPerInstance = _verticesPerInstance;
        instancedArgs.instanceCount = _instanceCount;
        instancedArgs.startVertexLocation = 0;
        instancedArgs.startInstanceLocation = 0;

        argsBufferDesc.ByteWidth = sizeof(DrawInstancedIndirectArgs);
        initialData.pSysMem = &instancedArgs;
    }
    else
    {
        indexedArgs.indexCountPerInstance = _numIndices;
        indexedArgs.instanceCount = _instanceCount;
        indexedArgs.startIndexLocation = 0;
        indexedArgs.baseVertexLocation = 0;
        indexedArgs.startInstanceLocation = 0;

        argsBufferDesc.ByteWidth = sizeof(DrawIndexedInstancedIndirectArgs);
        initialData.pSysMem = &indexedArgs;
    }

    CHECK_FAILED(_device->CreateBuffer(&argsBufferDesc, &initialData, _indirectArgsBuffer.GetAddressOf()), E_FAIL);
    return S_OK;
}

float VIBufferCom_PointParticle::Pick_RandomFloat(float minValue, float maxValue)
{
    if (minValue > maxValue)
        swap(minValue, maxValue);

    // TODO : 랜덤 임시. 나중에 필히 엔진 헬퍼 기능으로 교체할 것
    static thread_local mt19937 randomEngine{ random_device{}() };
    uniform_real_distribution<float> distribution{ minValue, maxValue };
    return distribution(randomEngine);
}

bool VIBufferCom_PointParticle::Is_Expired(const ParticleInstanceVertex& instance)
{
    return instance.lifeTime.y >= instance.lifeTime.x;
}

float VIBufferCom_PointParticle::Hash_Random01(uint32 seed)
{
    seed ^= 2747636419u;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    return static_cast<float>(seed & 0x00FFFFFFu) / 16777215.f;
}

float VIBufferCom_PointParticle::Evaluate_LifeProgress(const ParticleInstanceVertex& instance) const
{
    const float lifeMax = max(instance.lifeTime.x, 0.0001f);
    return clamp(instance.lifeTime.y / lifeMax, 0.f, 1.f);
}

Vec2 VIBufferCom_PointParticle::Evaluate_SizeMultiplier(float lifeProgress) const
{
    if (!_desc.sizeByLife.enabled)
        return Vec2{ 1.f, 1.f };

    return Vec2{
        lerp(_desc.sizeByLife.multiplyXStart, _desc.sizeByLife.multiplyXEnd, lifeProgress),
        lerp(_desc.sizeByLife.multiplyYStart, _desc.sizeByLife.multiplyYEnd, lifeProgress)
    };
}

Vec4 VIBufferCom_PointParticle::Evaluate_SubUVRect(float lifeProgress, float lifeCurrent, uint32 particleIndex) const
{
    const uint32 rows = max(1u, _desc.subUVRows);
    const uint32 cols = max(1u, _desc.subUVCols);
    const uint32 frameCount = max(1u, rows * cols);

    uint32 frameIndex = 0;
    if (_desc.subUvFrameOverLife.enabled)
    {
        const uint32 lastFrame = frameCount - 1;
        const uint32 startFrame = min(_desc.subUvFrameOverLife.startFrame, lastFrame);
        uint32 endFrame = min(_desc.subUvFrameOverLife.endFrame, lastFrame);

        if (endFrame < startFrame)
            endFrame = startFrame;

        const uint32 span = endFrame - startFrame;
        const uint32 rangeCount = span + 1;

        if (_desc.subUvFrameOverLife.playbackMode == SubUVFramePlaybackMode::FixedFrame)
        {
            frameIndex = startFrame;
        }
        else if (_desc.subUvFrameOverLife.playbackMode == SubUVFramePlaybackMode::LifeProgress)
        {
            const uint32 frameOffset = min(
                static_cast<uint32>(floorf(clamp(lifeProgress, 0.f, 1.f) * static_cast<float>(rangeCount))),
                rangeCount - 1
            );
            frameIndex = startFrame + frameOffset;
        }
        else if (_desc.subUvFrameOverLife.playbackMode == SubUVFramePlaybackMode::FramesPerSecond)
        {
            const uint32 frameOffset = static_cast<uint32>(floorf(max(0.f, lifeCurrent) * max(0.f, _desc.subUvFrameOverLife.framesPerSecond)));
            if (_desc.subUvFrameOverLife.loop && rangeCount > 0)
                frameIndex = startFrame + frameOffset % rangeCount;
            else
                frameIndex = startFrame + min(frameOffset, span);
        }
        else if (_desc.subUvFrameOverLife.playbackMode == SubUVFramePlaybackMode::RandomFrame)
        {
            const uint32 seed = particleIndex * 9781u + 1u;
            const uint32 frameOffset = min(
                static_cast<uint32>(floorf(Hash_Random01(seed) * static_cast<float>(rangeCount))),
                rangeCount - 1
            );
            frameIndex = startFrame + frameOffset;
        }
    }

    frameIndex = min(frameIndex, frameCount - 1);

    const uint32 row = frameIndex / cols;
    const uint32 col = frameIndex % cols;
    const float invCols = 1.f / static_cast<float>(cols);
    const float invRows = 1.f / static_cast<float>(rows);

    const float u0 = static_cast<float>(col) * invCols;
    const float v0 = static_cast<float>(row) * invRows;
    const float u1 = u0 + invCols;
    const float v1 = v0 + invRows;
    return Vec4{ u0, v0, u1, v1 };
}

void VIBufferCom_PointParticle::Update_InstanceOverLifePayload(uint32 index)
{
    if (index >= _runtimePayload.size() || index >= _baselinePayload.size())
        return;

    ParticleInstanceVertex& instance = _runtimePayload[index];
    const ParticleInstanceVertex& baseline = _baselinePayload[index];
    const float lifeProgress = Evaluate_LifeProgress(instance);
    const Vec2 sizeMultiplier = Evaluate_SizeMultiplier(lifeProgress);

    // SizeByLife를 현재 값에 누적 곱하지 않도록, spawn 시점 baseline 크기를 다시 꺼낸다.
    const Vec3 baseRight{ baseline.right.x, baseline.right.y, baseline.right.z };
    const Vec3 baseUp{ baseline.up.x, baseline.up.y, baseline.up.z };
    const float baseWidth = baseRight.Length();
    const float baseHeight = baseUp.Length();

    instance.right = Vec4{ baseWidth * sizeMultiplier.x, 0.f, 0.f, 0.f };
    instance.up = Vec4{ 0.f, baseHeight * sizeMultiplier.y, 0.f, 0.f };
    instance.look = baseline.look;
    instance.subUVRect = Evaluate_SubUVRect(lifeProgress, instance.lifeTime.y, index);
}

Shared<VIBufferCom_PointParticle> VIBufferCom_PointParticle::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<VIBufferCom_PointParticle>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : VIBufferCom_PointParticle");
        return nullptr;
    }

    return instance;
}

Shared<Component> VIBufferCom_PointParticle::Clone(void* arg)
{
    auto instance = make_shared<VIBufferCom_PointParticle>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : VIBufferCom_PointParticle");
        MSG_BOX("Failed to Clone : VIBufferCom_PointParticle");
        return nullptr;
    }

    return instance;
}

void VIBufferCom_PointParticle::Free()
{
    _indirectArgsBuffer.Reset();
    __super::Free();
}

#pragma warning(pop)
