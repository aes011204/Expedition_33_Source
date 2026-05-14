#include "VIBufferCom_ComputePointParticle.h"

#include "ComputeShaderCom.h"
#include "ComputeStructuredBuffer.h"
#include "GameInstance.h"

struct ComputePointParticleParams
{
    float elapsedTime{};
    float instanceCount{};
    uint32 drawMode{};
    float padding{};
    Vec4 center{};
    Vec4 scaleLifeTime{}; // x/y: scale range, z/w: lifetime range.
    Vec4 startColorMin{};
    Vec4 startColorMax{};
    Vec4 endColorMin{};
    Vec4 endColorMax{};
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
    CHECK_FAILED(Ready_IndexedDrawBuffer(), E_FAIL);
    CHECK_FAILED(Ready_IndirectArgsBuffers(), E_FAIL);
    CHECK_FAILED(Ready_ConstantBuffer(), E_FAIL);
    CHECK_FAILED(Update_ConstantBuffer(), E_FAIL);

    CHECK_FAILED(_computeShader->Bind_UAV(0, _computeOutput->Get_UAV()), E_FAIL);
    if (nullptr != _computeArgsOutput)
        CHECK_FAILED(_computeShader->Bind_UAV(1, _computeArgsOutput->Get_UAV()), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_ConstantBuffer(0, _computeConstantBuffer.Get()), E_FAIL);
    CHECK_FAILED(_computeShader->Dispatch((_instanceCount + kThreadCountX - 1) / kThreadCountX, 1, 1), E_FAIL);
    CHECK_FAILED(Copy_OutputToInstanceVB(), E_FAIL);
    CHECK_FAILED(Copy_OutputToIndirectArgs(), E_FAIL);

    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Render()
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

void VIBufferCom_ComputePointParticle::Dispatch_Compute(float timeDelta)
{
    if (nullptr == _computeShader || nullptr == _computeOutput || nullptr == _instanceVB)
        return;

    _elapsedTime += timeDelta;

    CHECK_FAILED(Update_ConstantBuffer());
    CHECK_FAILED(_computeShader->Bind_UAV(0, _computeOutput->Get_UAV()));
    if (nullptr != _computeArgsOutput)
        CHECK_FAILED(_computeShader->Bind_UAV(1, _computeArgsOutput->Get_UAV()));
    CHECK_FAILED(_computeShader->Bind_ConstantBuffer(0, _computeConstantBuffer.Get()));
    CHECK_FAILED(_computeShader->Dispatch((_instanceCount + kThreadCountX - 1) / kThreadCountX, 1, 1));
    CHECK_FAILED(Copy_OutputToInstanceVB());
    CHECK_FAILED(Copy_OutputToIndirectArgs());
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

HRESULT VIBufferCom_ComputePointParticle::Ready_IndexedDrawBuffer()
{
    if (PointParticleDrawMode::DrawIndexedInstancedIndirect != _desc.drawMode)
        return S_OK;

    _numIndices = 1;
    _indexStride = sizeof(uint16);

    const D3D11_BUFFER_DESC indexBufferDesc = {
        .ByteWidth = _indexStride * _numIndices,
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_INDEX_BUFFER,
        .StructureByteStride = _indexStride
    };

    const uint16 index = 0;
    const D3D11_SUBRESOURCE_DATA initialData = {
        .pSysMem = &index
    };

    CHECK_FAILED(_device->CreateBuffer(&indexBufferDesc, &initialData, _ib.GetAddressOf()), E_FAIL);
    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_IndirectArgsBuffers()
{
    if (PointParticleDrawMode::Direct == _desc.drawMode)
        return S_OK;

    _computeArgsOutput = ComputeStructuredBuffer::Create(
        _device,
        _context,
        sizeof(ComputeIndirectArgs),
        1
    );
    CHECK_NULL(_computeArgsOutput, E_FAIL);

    const D3D11_BUFFER_DESC argsBufferDesc = {
        .ByteWidth = sizeof(ComputeIndirectArgs),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = 0,
        .CPUAccessFlags = 0,
        .MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS
    };

    CHECK_FAILED(_device->CreateBuffer(&argsBufferDesc, nullptr, _indirectArgsBuffer.GetAddressOf()), E_FAIL);

#ifdef _DEBUG
    D3D11_BUFFER_DESC readbackDesc = argsBufferDesc;
    readbackDesc.Usage = D3D11_USAGE_STAGING;
    readbackDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    readbackDesc.MiscFlags = 0;

    CHECK_FAILED(_device->CreateBuffer(&readbackDesc, nullptr, _debugArgsReadbackBuffer.GetAddressOf()), E_FAIL);
#endif

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
        .instanceCount = static_cast<float>(_instanceCount),
        .drawMode = ETOI(_desc.drawMode),
        .center = Vec4{ _desc.center.x, _desc.center.y, _desc.center.z, 1.f },
        .scaleLifeTime = Vec4{ _desc.scale.x, _desc.scale.y, _desc.lifeTime.x, _desc.lifeTime.y },
        .startColorMin = _desc.startColorMin,
        .startColorMax = _desc.startColorMax,
        .endColorMin = _desc.endColorMin,
        .endColorMax = _desc.endColorMax
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

HRESULT VIBufferCom_ComputePointParticle::Copy_OutputToIndirectArgs()
{
    if (PointParticleDrawMode::Direct == _desc.drawMode)
        return S_OK;

    if (nullptr == _computeArgsOutput || nullptr == _indirectArgsBuffer)
        return E_FAIL;

    _context->CopyResource(_indirectArgsBuffer.Get(), _computeArgsOutput->Get_Buffer());

#ifdef _DEBUG
    Debug_LogIndirectArgs();
#endif

    return S_OK;
}

#ifdef _DEBUG
void VIBufferCom_ComputePointParticle::Debug_LogIndirectArgs()
{
    if (_debugArgsLogCount >= kDebugArgsLogLimit || nullptr == _debugArgsReadbackBuffer || nullptr == _indirectArgsBuffer)
        return;

    _context->CopyResource(_debugArgsReadbackBuffer.Get(), _indirectArgsBuffer.Get());

    D3D11_MAPPED_SUBRESOURCE mappedSubResource{};
    if (FAILED(_context->Map(_debugArgsReadbackBuffer.Get(), 0, D3D11_MAP_READ, 0, &mappedSubResource)))
        return;

    const auto* args = static_cast<const ComputeIndirectArgs*>(mappedSubResource.pData);
    if (nullptr != args && (_debugArgsLogCount == 0 || _debugLastArgsInstanceCount != args->instanceCount))
    {
        LOG_INFO(
            "Compute indirect args: mode={}, value0={}, instanceCount={}, value2={}, value3={}, startInstance={}",
            magic_enum::enum_name(_desc.drawMode),
            args->value0,
            args->instanceCount,
            args->value2,
            args->value3,
            args->startInstanceLocation
        );
        _debugLastArgsInstanceCount = args->instanceCount;
        ++_debugArgsLogCount;
    }

    _context->Unmap(_debugArgsReadbackBuffer.Get(), 0);
}
#endif

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
#ifdef _DEBUG
    _debugArgsReadbackBuffer.Reset();
#endif
    _indirectArgsBuffer.Reset();
    _computeConstantBuffer.Reset();
    _computeArgsOutput.reset();
    _computeOutput.reset();
    _computeShader.reset();

    __super::Free();
}
