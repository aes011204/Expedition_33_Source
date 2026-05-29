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
    static constexpr uint32 MAX_LAYERS = 4;
    static constexpr const char* layerNames[MAX_LAYERS] = { "g_Layer0",  "g_Layer1",  "g_Layer2",  "g_Layer3" };
    static constexpr const char* weightNames[MAX_LAYERS] = { "g_Weight0", "g_Weight1", "g_Weight2", "g_Weight3" };

public:
    MELandscapeActor(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual ~MELandscapeActor() = default;

public:
    Shared<VIBuffer_Landscape> Get_VIBuffer() { return _vibuffer; }
    uint32 Get_LayerCount() const { return _layerCount; }

    void Set_CameraFar(float camFar) { _cameraFar = camFar; }
    void Set_CameraPos(const Vec3& pos) { _cameraPos = pos; }
    void Set_LodDistances(float lodnear, float lodfar) { _lodDist[0] = lodnear; _lodDist[1] = lodfar; }

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

private:
    HRESULT Bind_ShaderResources();

public:
    HRESULT Add_LayerTexture(const wstring& texturePath);
    void Flush_WeightMap(uint32 layerIdx);

public:
    static Shared<MELandscapeActor> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const VIBuffer_Landscape::LAND_DESC& desc);
    virtual Shared<GameObject> Clone(void* arg) override { return nullptr; }
    virtual void Free() override;
};

NS_END
