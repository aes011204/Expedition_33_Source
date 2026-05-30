#pragma once
#include "VIBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL VIBuffer_TerrainUE final : public VIBuffer
{
private:
    const uint32 _vertsX = 32;
    const uint32 _vertsZ = 32;

public:
    struct TileDesc
    {
        wstring heightmapPath = {};
        int32 gridX = 0;
        int32 gridY = 0;
    };

    struct TerrainDesc
    {
        vector<TileDesc> tiles;
        float scaleXZ = 1.f;
        float scaleY = 1.f;

        float worldOffsetX = {};
        float worldOffsetY = {};
        float worldOffsetZ = {};
    };

public:
    VIBuffer_TerrainUE(const ComPtr<Device>& device, const ComPtr<Context>& context);
    VIBuffer_TerrainUE(const VIBuffer_TerrainUE& prototype);
    virtual ~VIBuffer_TerrainUE() = default;

public:
    virtual HRESULT Initialize_Prototype(const TerrainDesc& desc);
    virtual HRESULT Initialize(void* arg) override;

private:
    uint32 _totalTilesX = {};
    uint32 _totalTilesZ = {};

private:
    HRESULT Build_Tile(const TileDesc& tile, const TerrainDesc& desc, vector<VTXNORTEX>& outVertices, vector<uint32>& outIndices, uint32 vertexOffset);
    static HRESULT ReadHeightmapPNG(const TCHAR* path, vector<uint16>& outHeights, uint32& outWidth, uint32& outHeight);

public:
    static Shared<VIBuffer_TerrainUE> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const TerrainDesc& desc);
    virtual Shared<Component> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
