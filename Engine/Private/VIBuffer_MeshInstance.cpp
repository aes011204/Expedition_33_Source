#include "VIBuffer_MeshInstance.h"

VIBuffer_MeshInstance::VIBuffer_MeshInstance(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : VIBufferCom_Instance{ device, context }
{
}

VIBuffer_MeshInstance::VIBuffer_MeshInstance(const VIBuffer_MeshInstance& prototype)
    : VIBufferCom_Instance{ prototype }, _instanceCapacity{ prototype._instanceCapacity }
{
}

HRESULT VIBuffer_MeshInstance::Initialize_Prototype()
{
    return S_OK;
}

HRESULT VIBuffer_MeshInstance::Initialize(void* arg)
{
    auto* desc = static_cast<MeshInstanceInitDesc*>(arg);
    if (!desc)
    {
        return E_FAIL;
    }

    _vertexStride = sizeof(VTXNORTEX);
    _numVertices = desc->vertexCount;
    _numVertexBuffers = 2;

    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = _vertexStride * _numVertices;
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData{};
    vbData.pSysMem = desc->vertices;

    CHECK_FAILED(_device->CreateBuffer(&vbDesc, &vbData, _vb.GetAddressOf()), E_FAIL);

    _indexStride = sizeof(uint16);
    _numIndices = desc->indexCount;
    _primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    D3D11_BUFFER_DESC ibDesc{};
    ibDesc.ByteWidth = _indexStride * _numIndices;
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData{};
    ibData.pSysMem = desc->indices;

    CHECK_FAILED(_device->CreateBuffer(&ibDesc, &ibData, _ib.GetAddressOf()), E_FAIL);

    _instanceStride = sizeof(MESHINSTANCEDATA);
    _instanceCount = desc->instanceCount;
    _instanceCapacity = desc->instanceCount;

    D3D11_BUFFER_DESC instDesc{};
    instDesc.ByteWidth = _instanceStride * _instanceCapacity;
    instDesc.Usage = D3D11_USAGE_DYNAMIC;
    instDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    instDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA instData{};
    instData.pSysMem = desc->instances;

    CHECK_FAILED(_device->CreateBuffer(&instDesc, &instData, _instanceVB.GetAddressOf()), E_FAIL);

    return S_OK;
}

HRESULT VIBuffer_MeshInstance::UpdateInstances(const MESHINSTANCEDATA* instances, uint32 count)
{
    if (count == 0)
    {
        _instanceCount = 0;
        return S_OK;
    }

    if (count > _instanceCapacity)
    {
        _instanceCapacity = count;

        D3D11_BUFFER_DESC instDesc{};
        instDesc.ByteWidth = _instanceStride * _instanceCapacity;
        instDesc.Usage = D3D11_USAGE_DYNAMIC;
        instDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        instDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        _instanceVB.Reset();
        CHECK_FAILED(_device->CreateBuffer(&instDesc, nullptr, _instanceVB.GetAddressOf()), E_FAIL);
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    CHECK_FAILED(_context->Map(_instanceVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped), E_FAIL);

    memcpy(mapped.pData, instances, static_cast<size_t>(_instanceStride) * count);
    _context->Unmap(_instanceVB.Get(), 0);

    _instanceCount = count;
    return S_OK;
}

Shared<VIBuffer_MeshInstance> VIBuffer_MeshInstance::Create(const ComPtr<Device>& device,
    const ComPtr<Context>& context)
{
    auto instance = make_shared<VIBuffer_MeshInstance>(device, context);
    if (FAILED(instance->Initialize_Prototype()))
    {
        return nullptr;
    }

    return instance;
}

Shared<Component> VIBuffer_MeshInstance::Clone(void* arg)
{
    auto clone = make_shared<VIBuffer_MeshInstance>(*this);
    if (FAILED(clone->Initialize(arg)))
    {
        return nullptr;
    }

    return clone;
}

void VIBuffer_MeshInstance::Free()
{
    __super::Free();
}
