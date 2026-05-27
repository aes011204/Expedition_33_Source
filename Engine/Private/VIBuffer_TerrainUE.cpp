#include "VIBuffer_TerrainUE.h"

VIBuffer_TerrainUE::VIBuffer_TerrainUE(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : VIBuffer{ device, context }
{
}

VIBuffer_TerrainUE::VIBuffer_TerrainUE(const VIBuffer_TerrainUE& prototype)
    : VIBuffer{ prototype }, _totalTilesX{ prototype._totalTilesX }, _totalTilesZ{ prototype._totalTilesZ }
{
}

HRESULT VIBuffer_TerrainUE::Initialize_Prototype(const TerrainDesc& desc)
{
    vector<VTXNORTEX> allVertices;
    vector<uint32> allIndices;

    struct TileRange { uint32 vertexOffset; int32 gridX; int32 gridY; };
    vector<TileRange> tileRanges;

    for (const auto& tile : desc.tiles)
    {
        uint32 vertexOffset = static_cast<uint32>(allVertices.size());
        tileRanges.push_back({ vertexOffset, tile.gridX, tile.gridY });

        if (FAILED(Build_Tile(tile, desc, allVertices, allIndices, vertexOffset)))
        {
            LOG_WARN("VIBuffer_TerrainUE: Failed to build tile ({}, {})", tile.gridX, tile.gridY);
            continue;
        }
    }

    for (auto& a : tileRanges)
    {
        for (auto& b : tileRanges)
        {
            if (b.gridX == a.gridX + 1 && b.gridY == a.gridY)
            {
                for (uint32 z = 0; z < _vertsZ; ++z)
                {
                    uint32 idxA = a.vertexOffset + z * _vertsX + (_vertsX - 1); // a의 오른쪽 끝
                    uint32 idxB = b.vertexOffset + z * _vertsX + 0;              // b의 왼쪽 끝
                    allVertices[idxB].position.y = allVertices[idxA].position.y;
                }
            }

            if (b.gridY == a.gridY + 1 && b.gridX == a.gridX)
            {
                for (uint32 x = 0; x < _vertsX; ++x)
                {
                    uint32 idxA = a.vertexOffset + (_vertsZ - 1) * _vertsX + x; // a의 아랫줄
                    uint32 idxB = b.vertexOffset + 0 * _vertsX + x;              // b의 윗줄
                    allVertices[idxB].position.y = allVertices[idxA].position.y;
                }
            }
        }
    }

    for (auto& tr : tileRanges)
    {
        for (uint32 z = 0; z < _vertsZ; ++z)
        {
            for (uint32 x = 0; x < _vertsX; ++x)
            {
                uint32 li = tr.vertexOffset + z * _vertsX + (x > 0 ? x - 1 : 0);
                uint32 ri = tr.vertexOffset + z * _vertsX + (x < _vertsX - 1 ? x + 1 : _vertsX - 1);
                uint32 ui = tr.vertexOffset + (z > 0 ? z - 1 : 0) * _vertsX + x;
                uint32 di = tr.vertexOffset + (z < _vertsZ - 1 ? z + 1 : _vertsZ - 1) * _vertsX + x;

                Vec3 dx = allVertices[ri].position - allVertices[li].position;
                Vec3 dz = allVertices[di].position - allVertices[ui].position;

                Vec3 n = dz.Cross(dx);
                n.Normalize();

                allVertices[tr.vertexOffset + z * _vertsX + x].normal = n;
            }
        }
    }

    if (allVertices.empty() || allIndices.empty())
    {
        return E_FAIL;
    }

    _numVertexBuffers = 1;
    _vertexStride = sizeof(VTXNORTEX);
    _numVertices = static_cast<uint32>(allVertices.size());
    _indexStride = 4;
    _numIndices = static_cast<uint32>(allIndices.size());
    _primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = _vertexStride * _numVertices;
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.StructureByteStride = _vertexStride;

    D3D11_SUBRESOURCE_DATA vbData{};
    vbData.pSysMem = allVertices.data();

    if (FAILED(_device->CreateBuffer(&vbDesc, &vbData, _vb.GetAddressOf())))
    {
        return E_FAIL;
    }

    D3D11_BUFFER_DESC ibDesc{};
    ibDesc.ByteWidth = _indexStride * _numIndices;
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.StructureByteStride = _indexStride;

    D3D11_SUBRESOURCE_DATA ibData{};
    ibData.pSysMem = allIndices.data();

    if (FAILED(_device->CreateBuffer(&ibDesc, &ibData, _ib.GetAddressOf())))
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT VIBuffer_TerrainUE::Initialize(void* arg)
{
    return S_OK;
}

HRESULT VIBuffer_TerrainUE::Build_Tile(const TileDesc& tile, const TerrainDesc& desc, vector<VTXNORTEX>& outVertices, vector<uint32>& outIndices, uint32 vertexOffset)
{
    vector<uint16> heights;
    uint32 texW = 0, texH = 0;

    if (FAILED(ReadHeightmapPNG(tile.heightmapPath.c_str(), heights, texW, texH)))
    {
        return E_FAIL;
    }

    const float tileSpan = 126.f * desc.scaleXZ;
    const float tileOriginX = desc.worldOffsetX + tile.gridX * tileSpan;
    const float tileOriginZ = desc.worldOffsetZ + tile.gridY * tileSpan;

    for (uint32 z = 0; z < _vertsZ; ++z)
    {
        for (uint32 x = 0; x < _vertsX; ++x)
        {
            uint32 px = (x * (texW - 1)) / (_vertsX - 1);
            uint32 pz = (z * (texH - 1)) / (_vertsZ - 1);
            uint16 h = heights[pz * texW + px];

            float worldY = desc.worldOffsetY + (static_cast<float>(h) - 32768.f) / 128.0f * desc.scaleY;

            VTXNORTEX v{};
            const float stepX = tileSpan / (_vertsX - 1);
            const float stepZ = tileSpan / (_vertsZ - 1);
            v.position = Vec3(tileOriginX + x * stepX, worldY, tileOriginZ + z * stepZ);
            v.normal = Vec3(0.f, 1.f, 0.f);
            v.texcoord = Vec2(x / static_cast<float>(_vertsX - 1), z / static_cast<float>(_vertsZ - 1));

            outVertices.push_back(v);
        }
    }

    for (uint32 z = 0; z < _vertsZ - 1; ++z)
    {
        for (uint32 x = 0; x < _vertsX - 1; ++x)
        {
            uint32 base = vertexOffset + z * _vertsX + x;

            outIndices.push_back(base);
            outIndices.push_back(base + _vertsX);
            outIndices.push_back(base + _vertsX + 1);

            outIndices.push_back(base);
            outIndices.push_back(base + _vertsX + 1);
            outIndices.push_back(base + 1);
        }
    }

    return S_OK;
}

HRESULT VIBuffer_TerrainUE::ReadHeightmapPNG(const TCHAR* path, vector<uint16>& outHeights, uint32& outWidth, uint32& outHeight)
{
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
    {
        return E_FAIL;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder)))
    {
        return E_FAIL;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame)))
    {
        return E_FAIL;
    }

    frame->GetSize(&outWidth, &outHeight);

    // RGBA32
    ComPtr<IWICFormatConverter> converter;
    factory->CreateFormatConverter(&converter);
    converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

    const uint32 stride = outWidth * 4;
    vector<uint8> pixels(stride * outHeight);
    if (FAILED(converter->CopyPixels(nullptr, stride, static_cast<uint32>(pixels.size()), pixels.data())))
    {
        return E_FAIL;
    }

    outHeights.resize(outWidth * outHeight);
    for (uint32 i = 0; i < outWidth * outHeight; ++i)
    {
        uint8 r = pixels[i * 4 + 0];
        uint8 g = pixels[i * 4 + 1];
        outHeights[i] = (static_cast<uint16>(r) << 8) | static_cast<uint16>(g);
    }

    return S_OK;
}

Shared<VIBuffer_TerrainUE> VIBuffer_TerrainUE::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const TerrainDesc& desc)
{
    auto instance = make_shared<VIBuffer_TerrainUE>(device, context);

    if (FAILED(instance->Initialize_Prototype(desc)))
    {
        LOG_ERROR_ONCE("Failed to create VIBuffer_TerrainUE");
        return nullptr;
    }
    return instance;
}

Shared<Component> VIBuffer_TerrainUE::Clone(void* arg)
{
    auto instance = make_shared<VIBuffer_TerrainUE>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        return nullptr;
    }

    return instance;
}

void VIBuffer_TerrainUE::Free()
{
    __super::Free();
}
