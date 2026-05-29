#pragma once
#include "VIBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL VIBuffer_Landscape : public VIBuffer
{
private:
    static constexpr float SKIRT_DEPTH = 2.f;

    struct ChunkData
    {
        Vec3 center = {};
        int32 neighbor[4] = { -1, -1, -1, -1 }; // [N, S, W, E]
        ComPtr<ID3D11Buffer> skirtIB;
        uint32 skirtIndexCount = 0;
    };

public:
    struct LAND_DESC
    {
        uint32 countX = 64;
        uint32 countZ = 64;
        float sizeX = 1.f;
        float sizeZ = 1.f;
        uint32 chunkSize = 16;
    };

public:
    VIBuffer_Landscape(const ComPtr<Device>& device, const ComPtr<Context>& context, const LAND_DESC& desc);
    VIBuffer_Landscape(const VIBuffer_Landscape& prototype);
    virtual ~VIBuffer_Landscape() = default;

public:
    vector<float>& Get_Heights() { return _heights; }
    uint32 Get_CountX() const { return _desc.countX; }
    uint32 Get_CountZ() const { return _desc.countZ; }
    float Get_SizeX() const { return _desc.sizeX; }
    float Get_SizeZ() const { return _desc.sizeZ; }
    uint32 Get_ChunkSize() const { return _desc.chunkSize; }
    uint32 Get_NumChunkX() const { return _numChunksX; }
    uint32 Get_NumChunkZ() const { return _numChunksZ; }
    vector<float>& Get_WeightMap(uint32 layerIdx) { return _weightMaps[layerIdx]; }
    uint32 Get_LayerCount() const { return _layerCount; }

    void Set_LayerCount(uint32 count);

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;

private:
    LAND_DESC _desc{};
    vector<float> _heights;

    vector<ChunkData> _chunks;
    uint32 _numChunksX = {};
    uint32 _numChunksZ = {};
    uint32 _numTerrainVerts = {};

    ComPtr<ID3D11Buffer> _dynamicIB;
    uint32 _maxDynamicIndices = {};

    vector<uint32> _lodLevel;
    vector<uint32> _indexScratch;

    vector<vector<float>> _weightMaps; // [layeridx][vertexidx]
    uint32 _layerCount = 0;

private:
    void Rebuild_Normals(VTXNORTEX* vertices);
    void Build_ChunkSkirt(ChunkData& chunk, uint32 cx, uint32 cz, uint32 numVX, uint32 skirtVertBase);
    void Fill_ChunkIndices(uint32 cx, uint32 cz, uint32 lod, const uint32 neighborLod[4], uint32* indices, uint32& count);

public:
    void Flush();
    HRESULT Render_LOD(const Vec3& cameraPos, const float lodDist[2]);
    float Sample_Height(float worldX, float worldZ) const;
    void Paint_Layer(uint32 layerIdx, float worldX, float worldZ, float radius, float strength);
    void Clear_Layers();

public:
    static Shared<VIBuffer_Landscape> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const LAND_DESC& desc);
    virtual Shared<Component> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
