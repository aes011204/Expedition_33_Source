#include "VIBuffer_Plane.h"

VIBuffer_Plane::VIBuffer_Plane(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : VIBuffer(device, context) {
}

VIBuffer_Plane::VIBuffer_Plane(const VIBuffer_Plane& prototype)
    : VIBuffer(prototype) {
}

HRESULT VIBuffer_Plane::Initialize_Prototype(const DESC& desc)
{
    const uint32 cX = max(desc.countX, 2u);
    const uint32 cZ = max(desc.countZ, 2u);

    _numVertexBuffers = 1;
    _vertexStride = sizeof(VTXNORTEX);
    _numVertices = cX * cZ;
    _numIndices = (cX - 1) * (cZ - 1) * 6;
    _indexStride = 4;
    _primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    VTXNORTEX* vertices = new VTXNORTEX[_numVertices];
    for (uint32 z = 0; z < cZ; ++z)
    {
        for (uint32 x = 0; x < cX; ++x)
        {
            uint32 i = z * cX + x;
            vertices[i].position = Vec3(x * desc.sizeX, 0.f, z * desc.sizeZ);
            vertices[i].normal = Vec3(0.f, 1.f, 0.f);
            vertices[i].texcoord = Vec2(x / float(cX - 1), z / float(cZ - 1));
        }
    }

    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = _vertexStride * _numVertices;
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.StructureByteStride = _vertexStride;

    D3D11_SUBRESOURCE_DATA vbData{};
    vbData.pSysMem = vertices;
    HRESULT hr = _device->CreateBuffer(&vbDesc, &vbData, _vb.GetAddressOf());
    delete[] vertices;
    if (FAILED(hr)) return E_FAIL;

    uint32* indices = new uint32[_numIndices];
    uint32  idx = 0;
    for (uint32 z = 0; z < cZ - 1; ++z)
    {
        for (uint32 x = 0; x < cX - 1; ++x)
        {
            uint32 tl = z * cX + x;
            uint32 tr = tl + 1;
            uint32 bl = tl + cX;
            uint32 br = bl + 1;
            indices[idx++] = tl; indices[idx++] = br; indices[idx++] = bl;
            indices[idx++] = tl; indices[idx++] = tr; indices[idx++] = br;
        }
    }

    D3D11_BUFFER_DESC ibDesc{};
    ibDesc.ByteWidth = _indexStride * _numIndices;
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.StructureByteStride = _indexStride;

    D3D11_SUBRESOURCE_DATA ibData{};
    ibData.pSysMem = indices;
    hr = _device->CreateBuffer(&ibDesc, &ibData, _ib.GetAddressOf());
    delete[] indices;
    if (FAILED(hr)) return E_FAIL;

    return S_OK;
}

HRESULT VIBuffer_Plane::Initialize(void* arg)
{
    return S_OK;
}

Shared<VIBuffer_Plane> VIBuffer_Plane::Create(
    const ComPtr<Device>& device, const ComPtr<Context>& context, const DESC& desc)
{
    auto instance = make_shared<VIBuffer_Plane>(device, context);
    if (FAILED(instance->Initialize_Prototype(desc)))
        return nullptr;
    return instance;
}

Shared<Component> VIBuffer_Plane::Clone(void* arg)
{
    return make_shared<VIBuffer_Plane>(*this);
}

void VIBuffer_Plane::Free()
{
    VIBuffer::Free();
}
