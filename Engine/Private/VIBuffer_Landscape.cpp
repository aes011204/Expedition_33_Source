#include "VIBuffer_Landscape.h"

VIBuffer_Landscape::VIBuffer_Landscape(const ComPtr<Device>& device, const ComPtr<Context>& context, const LAND_DESC& desc)
    : VIBuffer{ device, context }, _desc{ desc }
{
}

VIBuffer_Landscape::VIBuffer_Landscape(const VIBuffer_Landscape& prototype)
    : VIBuffer{ prototype }, _desc{ prototype._desc }, _heights{ prototype._heights }
{
}

HRESULT VIBuffer_Landscape::Initialize_Prototype()
{
    const uint32 numVerticesX = _desc.countX + 1;
    const uint32 numVerticesZ = _desc.countZ + 1;

    _numVertexBuffers = 1;
    _numVertices = numVerticesX * numVerticesZ;
    _vertexStride = sizeof(VTXNORTEX);
    _numIndices = _desc.countX * _desc.countZ * 6;
    _indexStride = 4;
    _primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    _heights.assign(_numVertices, 0.f);

    VTXNORTEX* vertices = new VTXNORTEX[_numVertices];

    for (uint32 z = 0; z < numVerticesZ; ++z)
    {
        for (uint32 x = 0; x < numVerticesX; ++x)
        {
            uint32 idx = z * numVerticesX + x;
            vertices[idx].position = Vec3(x * _desc.sizeX, 0.f, z * _desc.sizeZ);
            vertices[idx].normal = Vec3(0.f, 1.f, 0.f);
            vertices[idx].texcoord = Vec2(static_cast<float>(x) / _desc.countX, static_cast<float>(z) / _desc.countZ);
        }
    }

    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = _vertexStride * _numVertices;
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    vbDesc.StructureByteStride = _vertexStride;

    D3D11_SUBRESOURCE_DATA vbData{};
    vbData.pSysMem = vertices;

    if (FAILED(_device->CreateBuffer(&vbDesc, &vbData, _vb.GetAddressOf())))
    {
        Safe_Delete(vertices);
        return E_FAIL;
    }

    uint32* indices = new uint32[_numIndices];
    uint32 idx = {};

    for (uint32 z = 0; z < _desc.countZ; ++z)
    {
        for (uint32 x = 0; x < _desc.countX; ++x)
        {
            uint32 tl = z * numVerticesX + x;
            uint32 tr = tl + 1;
            uint32 bl = tl + numVerticesX;
            uint32 br = bl + 1;

            indices[idx++] = tl;
            indices[idx++] = tr;
            indices[idx++] = bl;
            indices[idx++] = tr;
            indices[idx++] = br;
            indices[idx++] = bl;
        }
    }

    D3D11_BUFFER_DESC ibDesc{};
    ibDesc.ByteWidth = _indexStride * _numIndices;
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.StructureByteStride = _indexStride;

    D3D11_SUBRESOURCE_DATA ibData{};
    ibData.pSysMem = indices;

    if (FAILED(_device->CreateBuffer(&ibDesc, &ibData, _ib.GetAddressOf())))
    {
        Safe_Delete_Array(vertices);
        Safe_Delete_Array(indices);
        return E_FAIL;
    }

    Safe_Delete_Array(vertices);
    Safe_Delete_Array(indices);
    return S_OK;
}

HRESULT VIBuffer_Landscape::Initialize(void* arg)
{
    return S_OK;
}

void VIBuffer_Landscape::Rebuild_Normals(VTXNORTEX* vertices)
{
    const uint32 numVerticesX = _desc.countX + 1;
    const uint32 numVerticesZ = _desc.countZ + 1;

    for (uint32 z = 0; z < numVerticesZ; ++z)
    {
        for (uint32 x = 0; x < numVerticesX; ++x)
        {
            uint32 idx = z * numVerticesX + x;

            float hL = _heights[idx - (x > 0 ? 1 : 0)];
            float hR = _heights[idx + (x < _desc.countX ? 1 : 0)];
            float hD = _heights[idx - (z > 0 ? numVerticesX : 0)];
            float hU = _heights[idx + (z < _desc.countZ ? numVerticesX : 0)];

            Vec3 n(hL - hR, 2.f * _desc.sizeX, hD - hU);
            n.Normalize();
            vertices[idx].normal = n;
        }
    }
}

void VIBuffer_Landscape::Flush()
{
    const uint32 numVerticesX = _desc.countX + 1;
    const uint32 numVerticesZ = _desc.countZ + 1;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(_context->Map(_vb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return;
    }

    VTXNORTEX* vertices = static_cast<VTXNORTEX*>(mapped.pData);

    for (uint32 z = 0; z < numVerticesZ; ++z)
    {
        for (uint32 x = 0; x < numVerticesX; ++x)
        {
            uint32 idx = z * numVerticesX + x;
            vertices[idx].position.y = _heights[idx];
            vertices[idx].texcoord = Vec2(static_cast<float>(x) / _desc.countX, static_cast<float>(z) / _desc.countZ);
        }
    }

    Rebuild_Normals(vertices);

    _context->Unmap(_vb.Get(), 0);
}

float VIBuffer_Landscape::Sample_Height(float worldX, float worldZ) const
{
    const uint32 numVerticesX = _desc.countX + 1;

    float fx = worldX / _desc.sizeX;
    float fz = worldZ / _desc.sizeZ;

    int x0 = static_cast<int>(fx);
    int z0 = static_cast<int>(fz);
    int x1 = x0 + 1;
    int z1 = z0 + 1;

    x0 = max(0, min(static_cast<int>(_desc.countX), x0));
    z0 = max(0, min(static_cast<int>(_desc.countZ), z0));
    x1 = max(0, min(static_cast<int>(_desc.countX), x1));
    z1 = max(0, min(static_cast<int>(_desc.countZ), z1));

    float tx = fx - static_cast<float>(x0);
    float tz = fz - static_cast<float>(z0);

    float h00 = _heights[z0 * numVerticesX + x0];
    float h10 = _heights[z0 * numVerticesX + x1];
    float h01 = _heights[z1 * numVerticesX + x0];
    float h11 = _heights[z1 * numVerticesX + x1];

    return h00 * (1 - tx) * (1 - tz) + h10 * tx * (1 - tz) + h01 * (1 - tx) * tz + h11 * tx * tz;
}

Shared<VIBuffer_Landscape> VIBuffer_Landscape::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const LAND_DESC& desc)
{
    auto instance = make_shared<VIBuffer_Landscape>(device, context, desc);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_WARN_ONCE("Failed to create VIBuffer_Landscape");
        return nullptr;
    }
    return instance;
}

Shared<Component> VIBuffer_Landscape::Clone(void* arg)
{
    auto instance = make_shared<VIBuffer_Landscape>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : VIBuffer_Landscape");
        return nullptr;
    }
    return instance;
}

void VIBuffer_Landscape::Free()
{
    __super::Free();
}
