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

    return S_OK;
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

float VIBufferCom_PointParticle::Pick_RandomFloat(float minValue, float maxValue)
{
    if (minValue > maxValue)
        swap(minValue, maxValue);

    static thread_local mt19937 randomEngine{ random_device{}() };
    uniform_real_distribution<float> distribution{ minValue, maxValue };
    return distribution(randomEngine);
}

bool VIBufferCom_PointParticle::Is_Expired(const ParticleInstanceVertex& instance)
{
    return instance.lifeTime.y >= instance.lifeTime.x;
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
    __super::Free();
}
