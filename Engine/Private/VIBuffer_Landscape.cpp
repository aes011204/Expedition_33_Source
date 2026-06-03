#include "VIBuffer_Landscape.h"

VIBuffer_Landscape::VIBuffer_Landscape(const ComPtr<Device>& device, const ComPtr<Context>& context, const LAND_DESC& desc)
    : VIBuffer{ device, context }, _desc{ desc }
{
}

VIBuffer_Landscape::VIBuffer_Landscape(const VIBuffer_Landscape& prototype)
    : VIBuffer{ prototype }, _desc{ prototype._desc }, _heights{ prototype._heights }
{
}

void VIBuffer_Landscape::Set_LayerCount(uint32 count)
{
    count = min(count, MAX_LAYERS);
    uint32 oldCount = static_cast<uint32>(_weightMaps.size());
    if (count <= oldCount)
    {
        return;
    }

    _weightMaps.resize(count);
    for (uint32 i = oldCount; i < count; ++i)
    {
        _weightMaps[i].assign(_numTerrainVerts, 0.f);
    }

    if (oldCount == 0 && count > 0)
    {
        fill(_weightMaps[0].begin(), _weightMaps[0].end(), 1.f);
    }

    _layerCount = count;
}

HRESULT VIBuffer_Landscape::Initialize_Prototype()
{
    const uint32 cs = _desc.chunkSize;
    _desc.countX = (((_desc.countX + cs - 1) / cs) * cs);
    _desc.countZ = (((_desc.countZ + cs - 1) / cs) * cs);

    _numChunksX = _desc.countX / cs;
    _numChunksZ = _desc.countZ / cs;

    const uint32 numVX = _desc.countX + 1;
    const uint32 numVZ = _desc.countZ + 1;
    _numTerrainVerts = numVX * numVZ;

    const uint32 numChunks = _numChunksX * _numChunksZ;
    const uint32 numSkirtVerts = numChunks * 4 * (cs + 1);
    const uint32 totalVerts = _numTerrainVerts + numSkirtVerts;

    _numVertexBuffers = 1;
    _numVertices = totalVerts;
    _vertexStride = sizeof(VTXNORTEX);
    _numIndices = 0;
    _indexStride = 4;
    _primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    _heights.assign(_numTerrainVerts, 0.f);

    VTXNORTEX* vertices = new VTXNORTEX[totalVerts];

    for (uint32 z = 0; z < numVZ; ++z)
    {
        for (uint32 x = 0; x < numVX; ++x)
        {
            uint32 idx = z * numVX + x;
            vertices[idx].position = Vec3(x * _desc.sizeX, 0.f, z * _desc.sizeZ);
            vertices[idx].normal = Vec3(0.f, 1.f, 0.f);
            vertices[idx].texcoord = Vec2(static_cast<float>(x) / _desc.countX, static_cast<float>(z) / _desc.countZ);
        }
    }

    for (uint32 i = 0; i < numSkirtVerts; ++i)
    {
        vertices[_numTerrainVerts + i].position = Vec3(0.f, -SKIRT_DEPTH, 0.f);
        vertices[_numTerrainVerts + i].normal = Vec3(0.f, 1.f, 0.f);
        vertices[_numTerrainVerts + i].texcoord = Vec2(0.f, 0.f);
    }

    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = _vertexStride * totalVerts;
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    vbDesc.StructureByteStride = _vertexStride;

    D3D11_SUBRESOURCE_DATA vbData{ vertices };
    if (FAILED(_device->CreateBuffer(&vbDesc, &vbData, _vb.GetAddressOf())))
    {
        Safe_Delete_Array(vertices);
        return E_FAIL;
    }
    Safe_Delete_Array(vertices);

    _chunks.resize(numChunks);

    for (uint32 cz = 0; cz < _numChunksZ; ++cz)
    {
        for (uint32 cx = 0; cx < _numChunksX; ++cx)
        {
            const uint32 chunkIdx = cz * _numChunksX + cx;
            auto& chunk = _chunks[chunkIdx];
            const uint32 skirtBase = _numTerrainVerts + chunkIdx * 4 * (cs + 1);

            chunk.center = Vec3(
                (cx * cs + cs * 0.5f) * _desc.sizeX,
                0.f,
                (cz * cs + cs * 0.5f) * _desc.sizeZ);

            chunk.neighbor[0] = (cz > 0) ? static_cast<int32>((cz - 1) * _numChunksX + cx) : -1;
            chunk.neighbor[1] = (cz < _numChunksZ - 1) ? static_cast<int32>((cz + 1) * _numChunksX + cx) : -1;
            chunk.neighbor[2] = (cx > 0) ? static_cast<int32>(cz * _numChunksX + (cx - 1)) : -1;
            chunk.neighbor[3] = (cx < _numChunksX - 1) ? static_cast<int32>(cz * _numChunksX + (cx + 1)) : -1;

            Build_ChunkSkirt(chunk, cx, cz, numVX, skirtBase);
        }
    }

    const uint32 lod0IndicesPerChunk = cs * cs * 6;
    _maxDynamicIndices = numChunks * lod0IndicesPerChunk;

    D3D11_BUFFER_DESC dynIBDesc{};
    dynIBDesc.ByteWidth = 4 * _maxDynamicIndices;
    dynIBDesc.Usage = D3D11_USAGE_DYNAMIC;
    dynIBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    dynIBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(_device->CreateBuffer(&dynIBDesc, nullptr, _dynamicIB.GetAddressOf())))
    {
        return E_FAIL;
    }

    _lodLevel.resize(numChunks, 0);
    _indexScratch.resize(_maxDynamicIndices, 0);

    return S_OK;
}

HRESULT VIBuffer_Landscape::Initialize(void* arg)
{
    return S_OK;
}

void VIBuffer_Landscape::Rebuild_Normals(VTXNORTEX* vertices)
{
    const uint32 numVX = _desc.countX + 1;
    const uint32 numVZ = _desc.countZ + 1;

    for (uint32 z = 0; z < numVZ; ++z)
    {
        for (uint32 x = 0; x < numVX; ++x)
        {
            uint32 idx = z * numVX + x;

            float hL = _heights[idx - (x > 0 ? 1 : 0)];
            float hR = _heights[idx + (x < _desc.countX ? 1 : 0)];
            float hD = _heights[idx - (z > 0 ? numVX : 0)];
            float hU = _heights[idx + (z < _desc.countZ ? numVX : 0)];

            Vec3 n(hL - hR, 2.f * _desc.sizeX, hD - hU);
            n.Normalize();
            vertices[idx].normal = n;
        }
    }
}

void VIBuffer_Landscape::Build_ChunkSkirt(ChunkData& chunk, uint32 cx, uint32 cz, uint32 numVX, uint32 skirtVertBase)
{
    const uint32 cs = _desc.chunkSize;
    chunk.skirtIndexCount = 4 * cs * 6;

    uint32* skirtIndices = new uint32[chunk.skirtIndexCount];
    uint32 si = 0;

    auto AddSkirtEdge = [&](uint32 t0Terrain, uint32 terrainStride, uint32 t0Skirt, bool flip = false)
        {
            for (uint32 i = 0; i < cs; ++i)
            {
                uint32 t0 = t0Terrain + i * terrainStride;
                uint32 t1 = t0Terrain + (i + 1) * terrainStride;
                uint32 s0 = t0Skirt + i;
                uint32 s1 = t0Skirt + i + 1;

                if (flip)
                {
                    skirtIndices[si++] = t0; skirtIndices[si++] = s1; skirtIndices[si++] = s0;
                    skirtIndices[si++] = t0; skirtIndices[si++] = t1; skirtIndices[si++] = s1;
                }
                else
                {
                    skirtIndices[si++] = t0; skirtIndices[si++] = s0; skirtIndices[si++] = s1;
                    skirtIndices[si++] = t0; skirtIndices[si++] = s1; skirtIndices[si++] = t1;
                }
            }
        };

    AddSkirtEdge(cz * cs * numVX + cx * cs, 1, skirtVertBase + 0 * (cs + 1), true);
    AddSkirtEdge((cz + 1) * cs * numVX + cx * cs, 1, skirtVertBase + 1 * (cs + 1));
    AddSkirtEdge(cz * cs * numVX + cx * cs, numVX, skirtVertBase + 2 * (cs + 1));
    AddSkirtEdge(cz * cs * numVX + (cx + 1) * cs, numVX, skirtVertBase + 3 * (cs + 1), true);

    D3D11_BUFFER_DESC sibDesc{};
    sibDesc.ByteWidth = 4 * chunk.skirtIndexCount;
    sibDesc.Usage = D3D11_USAGE_DEFAULT;
    sibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA sibData{ skirtIndices };
    _device->CreateBuffer(&sibDesc, &sibData, chunk.skirtIB.GetAddressOf());
    Safe_Delete_Array(skirtIndices);
}

void VIBuffer_Landscape::Fill_ChunkIndices(uint32 cx, uint32 cz, uint32 lod, const uint32 neighborLod[4], uint32* indices, uint32& count)
{
    const uint32 cs = _desc.chunkSize;
    const uint32 numVX = _desc.countX + 1;
    const uint32 steps[3] = { 1, 2, 4 };
    const uint32 step = steps[lod];
    const uint32 quadsPerRow = cs / step;

    for (uint32 iz = 0; iz < quadsPerRow; ++iz)
    {
        for (uint32 ix = 0; ix < quadsPerRow; ++ix)
        {
            uint32 x = cx * cs + ix * step;
            uint32 z = cz * cs + iz * step;

            uint32 tl = z * numVX + x;
            uint32 tr = z * numVX + (x + step);
            uint32 bl = (z + step) * numVX + x;
            uint32 br = (z + step) * numVX + (x + step);

            bool stitchN = (iz == 0) && neighborLod[0] < lod;
            bool stitchS = (iz == quadsPerRow - 1) && neighborLod[1] < lod;
            bool stitchW = (ix == 0) && neighborLod[2] < lod;
            bool stitchE = (ix == quadsPerRow - 1) && neighborLod[3] < lod;

            if (!stitchN && !stitchS && !stitchW && !stitchE)
            {
                indices[count++] = bl; indices[count++] = br; indices[count++] = tr;
                indices[count++] = bl; indices[count++] = tr; indices[count++] = tl;
                continue;
            }

            const uint32 h = step / 2;
            uint32 tc = z * numVX + (x + h);
            uint32 bc = (z + step) * numVX + (x + h);
            uint32 lc = (z + h) * numVX + x;
            uint32 rc = (z + h) * numVX + (x + step);
            uint32 cv = (z + h) * numVX + (x + h);

            if (stitchN)
            {
                indices[count++] = cv; indices[count++] = tc; indices[count++] = tl;
                indices[count++] = cv; indices[count++] = tr; indices[count++] = tc;
            }
            else
            {
                indices[count++] = cv; indices[count++] = tr; indices[count++] = tl;
            }

            // East
            if (stitchE)
            {
                indices[count++] = cv; indices[count++] = rc; indices[count++] = tr;
                indices[count++] = cv; indices[count++] = br; indices[count++] = rc;
            }
            else
            {
                indices[count++] = cv; indices[count++] = br; indices[count++] = tr;
            }

            // South
            if (stitchS)
            {
                indices[count++] = cv; indices[count++] = bl; indices[count++] = bc;
                indices[count++] = cv; indices[count++] = bc; indices[count++] = br;
            }
            else
            {
                indices[count++] = cv; indices[count++] = bl; indices[count++] = br;
            }

            // West
            if (stitchW)
            {
                indices[count++] = cv; indices[count++] = tl; indices[count++] = lc;
                indices[count++] = cv; indices[count++] = lc; indices[count++] = bl;
            }
            else
            {
                indices[count++] = cv; indices[count++] = tl; indices[count++] = bl;
            }
        }
    }
}

void VIBuffer_Landscape::Flush()
{
    const uint32 cs = _desc.chunkSize;
    const uint32 numVX = _desc.countX + 1;
    const uint32 numVZ = _desc.countZ + 1;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(_context->Map(_vb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return;
    }

    VTXNORTEX* vertices = static_cast<VTXNORTEX*>(mapped.pData);

    for (uint32 z = 0; z < numVZ; ++z)
    {
        for (uint32 x = 0; x < numVX; ++x)
        {
            uint32 idx = z * numVX + x;
            vertices[idx].position.x = x * _desc.sizeX;
            vertices[idx].position.y = _heights[idx];
            vertices[idx].position.z = z * _desc.sizeZ;
            vertices[idx].texcoord = Vec2(static_cast<float>(x) / _desc.countX, static_cast<float>(z) / _desc.countZ);
        }
    }

    Rebuild_Normals(vertices);

    for (uint32 cz = 0; cz < _numChunksZ; ++cz)
    {
        for (uint32 cx = 0; cx < _numChunksX; ++cx)
        {
            const uint32 chunkIdx = cz * _numChunksX + cx;
            const uint32 skirtBase = _numTerrainVerts + chunkIdx * 4 * (cs + 1);

            auto SetSkirt = [&](uint32 terrainIdx, uint32 skirtIdx)
                {
                    vertices[skirtIdx].position = vertices[terrainIdx].position;
                    vertices[skirtIdx].position.y -= SKIRT_DEPTH;
                    vertices[skirtIdx].normal = Vec3(0.f, 1.f, 0.f);
                    vertices[skirtIdx].texcoord = vertices[terrainIdx].texcoord;
                };

            for (uint32 i = 0; i <= cs; ++i)
            {
                SetSkirt(cz * cs * numVX + cx * cs + i, skirtBase + i);
                SetSkirt((cz + 1) * cs * numVX + cx * cs + i, skirtBase + (cs + 1) + i);
                SetSkirt((cz * cs + i) * numVX + cx * cs, skirtBase + 2 * (cs + 1) + i);
                SetSkirt((cz * cs + i) * numVX + (cx + 1) * cs, skirtBase + 3 * (cs + 1) + i);
            }
        }
    }

    _context->Unmap(_vb.Get(), 0);
}

HRESULT VIBuffer_Landscape::Render_LOD(const Vec3& cameraPos, const float lodDist[2])
{
    const uint32 numChunks = static_cast<uint32>(_chunks.size());
    if (numChunks == 0 || !_dynamicIB)
    {
        return S_OK;
    }

    const float d0sq = lodDist[0] * lodDist[0];
    const float d1sq = lodDist[1] * lodDist[1];

    fill(_lodLevel.begin(), _lodLevel.end(), 0);
    for (uint32 i = 0; i < numChunks; ++i)
    {
        const float distSq = (_chunks[i].center - cameraPos).LengthSquared();
        _lodLevel[i] = (distSq > d1sq) ? 2 : (distSq > d0sq) ? 1 : 0;
    }

    bool changed = true;
    while (changed)
    {
        changed = false;
        for (uint32 i = 0; i < numChunks; ++i)
        {
            for (uint32 d = 0; d < 4; ++d)
            {
                int32 nb = _chunks[i].neighbor[d];
                if (nb < 0)
                {
                    continue;
                }

                if (_lodLevel[i] > _lodLevel[nb] + 1)
                {
                    _lodLevel[i] = _lodLevel[nb] + 1;
                    changed = true;
                }
            }
        }
    }

    uint32 totalCount = 0;

    for (uint32 i = 0; i < numChunks; ++i)
    {
        const uint32 cz = i / _numChunksX;
        const uint32 cx = i % _numChunksX;

        uint32 neighborLod[4];
        for (uint32 d = 0; d < 4; ++d)
        {
            int32 nb = _chunks[i].neighbor[d];
            neighborLod[d] = (nb >= 0) ? _lodLevel[nb] : _lodLevel[i];
        }

        Fill_ChunkIndices(cx, cz, _lodLevel[i], neighborLod, _indexScratch.data(), totalCount);
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(_context->Map(_dynamicIB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return E_FAIL;
    }

    memcpy(mapped.pData, _indexScratch.data(), totalCount * 4);
    _context->Unmap(_dynamicIB.Get(), 0);

    _context->IASetIndexBuffer(_dynamicIB.Get(), DXGI_FORMAT_R32_UINT, 0);
    _context->DrawIndexed(totalCount, 0, 0);

    for (auto& chunk : _chunks)
    {
        if (chunk.skirtIB)
        {
            _context->IASetIndexBuffer(chunk.skirtIB.Get(), DXGI_FORMAT_R32_UINT, 0);
            _context->DrawIndexed(chunk.skirtIndexCount, 0, 0);
        }
    }

    return S_OK;
}

float VIBuffer_Landscape::Sample_Height(float worldX, float worldZ) const
{
    const uint32 numVX = _desc.countX + 1;

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

    float h00 = _heights[z0 * numVX + x0];
    float h10 = _heights[z0 * numVX + x1];
    float h01 = _heights[z1 * numVX + x0];
    float h11 = _heights[z1 * numVX + x1];

    return h00 * (1 - tx) * (1 - tz) + h10 * tx * (1 - tz) + h01 * (1 - tx) * tz + h11 * tx * tz;
}

void VIBuffer_Landscape::Paint_Layer(uint32 layerIdx, float worldX, float worldZ, float radius, float strength)
{
    if (layerIdx >= _layerCount)
    {
        return;
    }

    const uint32 numVX = _desc.countX + 1;
    const uint32 numVZ = _desc.countZ + 1;
    const float r2 = radius * radius;

    for (uint32 z = 0; z < numVZ; ++z)
    {
        for (uint32 x = 0; x < numVX; ++x)
        {
            float dx = x * _desc.sizeX - worldX;
            float dz = z * _desc.sizeZ - worldZ;
            float d2 = dx * dx + dz * dz;
            if (d2 >= r2)
            {
                continue;
            }

            float t = 1.f - d2 / r2;
            uint32 idx = z * numVX + x;

            float& w = _weightMaps[layerIdx][idx];
            w = min(1.f, w + strength * t);

            float remaining = 1.f - w;
            float otherSum = 0.f;
            for (uint32 i = 0; i < _layerCount; ++i)
            {
                if (i != layerIdx)
                {
                    otherSum += _weightMaps[i][idx];
                }
            }

            if (otherSum > 0.f)
            {
                for (uint32 i = 0; i < _layerCount; ++i)
                {
                    if (i != layerIdx)
                    {
                        _weightMaps[i][idx] = _weightMaps[i][idx] / otherSum * remaining;
                    }
                }
            }
            else if (_layerCount > 1 && layerIdx != 0)
            {
                _weightMaps[0][idx] = remaining;
            }
        }
    }
}

void VIBuffer_Landscape::Clear_Layers()
{
    _weightMaps.clear();
    _layerCount = 0;
}

void VIBuffer_Landscape::Remove_Layer(uint32 idx)
{
    if (idx >= _layerCount || _layerCount <= 1)
    {
        return;
    }

    const uint32 numVerts = (_desc.countX + 1) * (_desc.countZ + 1);

    if (idx != 0)
    {
        for (uint32 v = 0; v < numVerts; ++v)
        {
            _weightMaps[0][v] += _weightMaps[idx][v];
        }
    }
    else
    {
        for (uint32 v = 0; v < numVerts; ++v)
        {
            _weightMaps[1][v] += _weightMaps[0][v];
        }
    }

    _weightMaps.erase(_weightMaps.begin() + idx);
    --_layerCount;
}

void VIBuffer_Landscape::Swap_Layer(uint32 a, uint32 b)
{
    if (a >= _layerCount || b >= _layerCount || a == b)
    {
        return;
    }

    swap(_weightMaps[a], _weightMaps[b]);
}

Shared<VIBuffer_Landscape> VIBuffer_Landscape::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const LAND_DESC& desc)
{
    auto instance = make_shared<VIBuffer_Landscape>(device, context, desc);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_WARN_ONCE("Failed to create VIBuffer_Landscape");
        return nullptr;
    }

    instance->Flush();

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
