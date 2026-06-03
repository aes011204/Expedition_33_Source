#pragma once
#include "GameObject.h"
#include "VIBuffer_Landscape.h"
#include "MapEditorDefines.h"

NS_BEGIN(Engine)
class ShaderCom;
NS_END

NS_BEGIN(MapEditor)

class MELandscapeActor final : public GameObject 
{
private:
    static constexpr const char* layerNames[MAX_LAYERS] = { "g_Layer0", "g_Layer1", "g_Layer2", "g_Layer3", "g_Layer4", "g_Layer5", "g_Layer6" };
    static constexpr const char* weightNames[MAX_LAYERS] = { "g_Weight0", "g_Weight1", "g_Weight2", "g_Weight3", "g_Weight4", "g_Weight5", "g_Weight6" };
    static constexpr const char* normalNames[MAX_LAYERS] = { "g_Normal0", "g_Normal1", "g_Normal2", "g_Normal3", "g_Normal4", "g_Normal5", "g_Normal6" };
    static constexpr const char* roughnessNames[MAX_LAYERS] = { "g_Roughness0", "g_Roughness1", "g_Roughness2", "g_Roughness3", "g_Roughness4", "g_Roughness5", "g_Roughness6" };

public:
    MELandscapeActor(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual ~MELandscapeActor() = default;

public:
    Shared<VIBuffer_Landscape> Get_VIBuffer() { return _vibuffer; }
    uint32 Get_LayerCount() const { return _layerCount; }
    const wstring& Get_LayerPath(uint32 i) const { return _layerPaths[i]; };
    const wstring& Get_NormalPath(uint32 i) const { return _normalPaths[i]; }
    const wstring& Get_RoughnessPath(uint32 i) const { return _roughnessPaths[i]; }
    float Get_LayerTiling(uint32 i) const { return i < MAX_LAYERS ? _tiling[i] : 1.f; }
    const Vec4& Get_LayerGroundTint(uint32 i) const { return _groundTint[i]; }
    const Vec4& Get_LayerControl(uint32 i) const { return _layerControl[i]; }
    const Vec4& Get_AlbedoControl(uint32 i) const { return _albedoControl[i]; }

    void Set_CameraFar(float camFar) { _cameraFar = camFar; }
    void Set_CameraPos(const Vec3& pos) { _cameraPos = pos; }
    void Set_LodDistances(float lodnear, float lodfar) { _lodDist[0] = lodnear; _lodDist[1] = lodfar; }
    void Set_LayerTiling(uint32 i, float v) { if (i < MAX_LAYERS) _tiling[i] = v; }
    void Set_LayerGroundTint(uint32 i, const Vec4& v) { if (i < MAX_LAYERS) _groundTint[i] = v; }
    void Set_LayerControl(uint32 i, const Vec4& v) { if (i < MAX_LAYERS) _layerControl[i] = v; }
    void Set_AlbedoControl(uint32 i, const Vec4& v) { if (i < MAX_LAYERS) _albedoControl[i] = v; }

public:
    HRESULT Initialize(const VIBuffer_Landscape::LAND_DESC& desc);
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;

private:
    Shared<ShaderCom> _shader;
    Shared<VIBuffer_Landscape> _vibuffer;
    float _cameraFar = 1000.f;
    Vec3 _cameraPos = {};
    float _lodDist[2] = { 50.f, 150.f };

    ComPtr<ID3D11ShaderResourceView> _layerSRVs[MAX_LAYERS];
    ComPtr<ID3D11Texture2D> _weightTextures[MAX_LAYERS];
    ComPtr<ID3D11ShaderResourceView> _weightSRVs[MAX_LAYERS];
    uint32 _layerCount = 0;

    wstring _layerPaths[MAX_LAYERS];

    ComPtr<ID3D11Texture2D> _dummyWeightTex;
    ComPtr<ID3D11ShaderResourceView> _dummyWeightSRV;

    ComPtr<ID3D11ShaderResourceView> _normalSRVs[MAX_LAYERS];
    ComPtr<ID3D11ShaderResourceView> _roughnessSRVs[MAX_LAYERS];

    ComPtr<ID3D11Texture2D> _dummyNormalTex;
    ComPtr<ID3D11ShaderResourceView> _dummyNormalSRV;
    ComPtr<ID3D11Texture2D> _dummyRoughnessTex;
    ComPtr<ID3D11ShaderResourceView> _dummyRoughnessSRV;

    wstring _normalPaths[MAX_LAYERS];
    wstring _roughnessPaths[MAX_LAYERS];

    float _tiling[MAX_LAYERS] = { 3.f, 3.f, 3.14f, 3.f, 10.f, 7.f, 3.68f };
    Vec4 _groundTint[MAX_LAYERS] = { {1,1,1,1}, {1,1,1,1}, {1,1,1,1}, {1,1,1,1}, {1,1,1,1}, {1,1,1,1}, {1,1,1,1} };
    Vec4 _layerControl[MAX_LAYERS] = { {2.f,1.69f,0.4f,0}, {3.f,0.94f,0,0}, {1.92f,1.24f,0,0}, {4.f,2.82f,0,0}, {3.f,0.8f,1.f,1.f}, {1.2f,2.f,0,0}, {3.f,0.4f,0,0} };
    Vec4 _albedoControl[MAX_LAYERS] = {
    {1.f, 13.73f, 1.43f, 0},   // Layer0: Moss Ground
    {0.1f, 1.f,   1.f,   0},   // Layer1: Dry Mud
    {0.2f, 4.f,   0.91f, 0},   // Layer2: Gravel
    {2.43f,1.f,   1.f,   0},   // Layer3: Coal
    {0.f,  4.865f,0.53f, 0},   // Layer4: boudin
    {1.f,  7.459f,0.05f, 0},   // Layer5: Wet Dirt
    {0.2f, 1.973f,1.616f,0},   // Layer6: Moss reuse
    };

private:
    HRESULT Bind_ShaderResources();

public:
    HRESULT Add_LayerTexture(const wstring& albedoPath, const wstring& normalPath = L"", const wstring& roughnessPath = L"");
    void Flush_WeightMap(uint32 layerIdx);
    void Clear_Layers();
    void Remove_LayerTexture(uint32 idx);
    void Set_BaseLayer(uint32 idx);

public:
    static Shared<MELandscapeActor> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const VIBuffer_Landscape::LAND_DESC& desc);
    virtual Shared<GameObject> Clone(void* arg) override { return nullptr; }
    virtual void Free() override;
};

NS_END
