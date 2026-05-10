#include "VIBufferCom_ComputePointParticle.h"

#include "ComputeShaderCom.h"
#include "ComputeStructuredBuffer.h"
#include "GameInstance.h"

// TODO : 주석 추가 바람
struct ComputePointParticleParams
{
    float elapsedTime{};
    float instanceCount{};
    Vec2 padding{};
};

VIBufferCom_ComputePointParticle::VIBufferCom_ComputePointParticle(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : VIBufferCom_Instance{ device, context }
{
}

VIBufferCom_ComputePointParticle::VIBufferCom_ComputePointParticle(const VIBufferCom_ComputePointParticle& prototype)
    : VIBufferCom_Instance{ prototype }
{
}

VIBufferCom_ComputePointParticle::~VIBufferCom_ComputePointParticle()
{
    Free();
}

HRESULT VIBufferCom_ComputePointParticle::Initialize_Prototype()
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

HRESULT VIBufferCom_ComputePointParticle::Initialize(void* arg)
{
    if (nullptr == arg)
    {
        LOG_ERROR("VIBufferCom_ComputePointParticle requires ComputePointParticleDesc at clone time.");
        return E_FAIL;
    }

    _desc = *static_cast<ComputePointParticleDesc*>(arg);

    if (_desc.instanceCount == 0)
    {
        LOG_ERROR("VIBufferCom_ComputePointParticle requires at least one instance.");
        return E_FAIL;
    }

    _instanceCount = _desc.instanceCount;

    _instanceBufferDesc = {
        .ByteWidth = _instanceStride * _instanceCount,
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        .CPUAccessFlags = 0,
        .StructureByteStride = _instanceStride
    };

    CHECK_FAILED(Ready_ComputeShader(), E_FAIL);
    CHECK_FAILED(Ready_ComputeOutput(), E_FAIL);
    CHECK_FAILED(Ready_InstanceBuffer(), E_FAIL);
    CHECK_FAILED(Ready_ConstantBuffer(), E_FAIL);
    CHECK_FAILED(Update_ConstantBuffer(), E_FAIL);

    CHECK_FAILED(_computeShader->Bind_UAV(0, _computeOutput->Get_UAV()), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_ConstantBuffer(0, _computeConstantBuffer.Get()), E_FAIL);
    CHECK_FAILED(_computeShader->Dispatch((_instanceCount + kThreadCountX - 1) / kThreadCountX, 1, 1), E_FAIL);
    CHECK_FAILED(Copy_OutputToInstanceVB(), E_FAIL);

    return S_OK;
}

void VIBufferCom_ComputePointParticle::Dispatch_Compute(float timeDelta)
{
    if (nullptr == _computeShader || nullptr == _computeOutput || nullptr == _instanceVB)
        return;

    _elapsedTime += timeDelta;

    CHECK_FAILED(Update_ConstantBuffer());
    CHECK_FAILED(_computeShader->Bind_UAV(0, _computeOutput->Get_UAV()));
    CHECK_FAILED(_computeShader->Bind_ConstantBuffer(0, _computeConstantBuffer.Get()));
    CHECK_FAILED(_computeShader->Dispatch((_instanceCount + kThreadCountX - 1) / kThreadCountX, 1, 1));
    CHECK_FAILED(Copy_OutputToInstanceVB());
}

HRESULT VIBufferCom_ComputePointParticle::Ready_VertexBuffer()
{
    const D3D11_BUFFER_DESC vertexBufferDesc = {
        .ByteWidth = _vertexStride * _numVertices,
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        .StructureByteStride = _vertexStride
    };

    const Vec3 vertex{ 0.f, 0.f, 0.f };

    const D3D11_SUBRESOURCE_DATA initialData = {
        .pSysMem = &vertex
    };

    CHECK_FAILED(_device->CreateBuffer(&vertexBufferDesc, &initialData, _vb.GetAddressOf()), E_FAIL);
    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_ComputeShader()
{
    if (_desc.computeShaderPrototypeTag.empty())
        return E_FAIL;

    _computeShader = dynamic_pointer_cast<ComputeShaderCom>(
        GAME->Clone_Prototype(
            Prototype::Comopnent,
            _desc.computeShaderLevelIndex,
            _desc.computeShaderPrototypeTag
        )
    );
    CHECK_NULL(_computeShader, E_FAIL);

    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_ComputeOutput()
{
    _computeOutput = ComputeStructuredBuffer::Create(
        _device,
        _context,
        sizeof(ParticleInstanceVertex),
        _instanceCount
    );
    CHECK_NULL(_computeOutput, E_FAIL);

    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_InstanceBuffer()
{
    CHECK_FAILED(_device->CreateBuffer(&_instanceBufferDesc, nullptr, _instanceVB.GetAddressOf()), E_FAIL);
    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_ConstantBuffer()
{
    const D3D11_BUFFER_DESC bufferDesc = {
        .ByteWidth = sizeof(ComputePointParticleParams),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER
    };

    CHECK_FAILED(_device->CreateBuffer(&bufferDesc, nullptr, _computeConstantBuffer.GetAddressOf()), E_FAIL);
    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Update_ConstantBuffer()
{
    if (nullptr == _computeConstantBuffer)
        return E_FAIL;

    const ComputePointParticleParams params = {
        .elapsedTime = _elapsedTime,
        .instanceCount = static_cast<float>(_instanceCount)
    };

    _context->UpdateSubresource(_computeConstantBuffer.Get(), 0, nullptr, &params, 0, 0);
    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Copy_OutputToInstanceVB()
{
    if (nullptr == _computeOutput || nullptr == _instanceVB)
        return E_FAIL;

    _context->CopyResource(_instanceVB.Get(), _computeOutput->Get_Buffer());
    return S_OK;
}

Shared<VIBufferCom_ComputePointParticle> VIBufferCom_ComputePointParticle::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<VIBufferCom_ComputePointParticle>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : VIBufferCom_ComputePointParticle");
        return nullptr;
    }

    return instance;
}

Shared<Component> VIBufferCom_ComputePointParticle::Clone(void* arg)
{
    auto instance = make_shared<VIBufferCom_ComputePointParticle>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : VIBufferCom_ComputePointParticle");
        MSG_BOX("Failed to Clone : VIBufferCom_ComputePointParticle");
        return nullptr;
    }

    return instance;
}

void VIBufferCom_ComputePointParticle::Free()
{
    _computeConstantBuffer.Reset();
    _computeOutput.reset();
    _computeShader.reset();

    __super::Free();
}
