#pragma once
#include "Client_Defines.h"
#include "GameObject.h"
#include "PhysX_Handle.h"

NS_BEGIN(Engine)
class ShaderCom;
class VIBuffer_Landscape;
struct StaticCollisionData;
NS_END

NS_BEGIN(Client)

class MapLandscape final : public GameObject
{
    GENERATED_GAMEOBJECT(MapLandscape, ObjectType::Landscape)

private:
    static constexpr const char* layerNames[MAX_LAYERS] = { "g_Layer0", "g_Layer1", "g_Layer2", "g_Layer3", "g_Layer4", "g_Layer5", "g_Layer6" };
    static constexpr const char* weightNames[MAX_LAYERS] = { "g_Weight0", "g_Weight1", "g_Weight2", "g_Weight3", "g_Weight4", "g_Weight5", "g_Weight6" };
    static constexpr const char* normalNames[MAX_LAYERS] = { "g_Normal0", "g_Normal1", "g_Normal2", "g_Normal3", "g_Normal4", "g_Normal5", "g_Normal6" };
    static constexpr const char* roughnessNames[MAX_LAYERS] = { "g_Roughness0", "g_Roughness1", "g_Roughness2", "g_Roughness3", "g_Roughness4", "g_Roughness5", "g_Roughness6" };

public:
    typedef struct tagLandscapeDesc final : public GameObject::GAMEOBJECT_DESC
    {
        wstring hmapPath;
        wstring texRoot;
    } LANDSCAPE_DESC;

public:
    MapLandscape(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MapLandscape(const MapLandscape& prototype);
    virtual ~MapLandscape() = default;

public:
    void Set_UseLOD(bool useLOD) { _useLOD = useLOD; }

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;

private:
    Shared<ShaderCom> _shader;
    Shared<VIBuffer_Landscape> _vibuffer;
    Shared<StaticCollisionData> _collisionData;
    PhysXStaticActorHandle _physxStaticHandle{};
    wstring _collisionCacheKey{};

    ComPtr<ID3D11ShaderResourceView> _layerSRVs[MAX_LAYERS];
    ComPtr<ID3D11Texture2D> _weightTextures[MAX_LAYERS];
    ComPtr<ID3D11ShaderResourceView> _weightSRVs[MAX_LAYERS];
    uint32 _layerCount = 0;

    ComPtr<ID3D11Texture2D> _dummyWeightTex;
    ComPtr<ID3D11ShaderResourceView> _dummyWeightSRV;

    ComPtr<ID3D11ShaderResourceView> _normalSRVs[MAX_LAYERS];
    ComPtr<ID3D11ShaderResourceView> _roughnessSRVs[MAX_LAYERS];

    ComPtr<ID3D11Texture2D> _dummyNormalTex;
    ComPtr<ID3D11ShaderResourceView> _dummyNormalSRV;
    ComPtr<ID3D11Texture2D> _dummyRoughnessTex;
    ComPtr<ID3D11ShaderResourceView> _dummyRoughnessSRV;

    Vec2  _tiling[MAX_LAYERS] = { {3.f, 3.f}, {3.f, 3.f}, {3.14f, 3.14f}, {3.f, 3.f}, {10.f, 10.f}, {7.f, 7.f}, {3.68f, 3.68f} };
    Vec4  _groundTint[MAX_LAYERS] = { {1,1,1,1},{1,1,1,1},{1,1,1,1},{1,1,1,1},{1,1,1,1},{1,1,1,1},{1,1,1,1} };
    Vec4  _layerControl[MAX_LAYERS] = { {2.f,1.69f,0.4f,0},{3.f,0.94f,0,0},{1.92f,1.24f,0,0},{4.f,2.82f,0,0},{3.f,0.8f,1.f,0},{1.2f,2.f,0,0},{3.f,0.4f,0,0} };

    Vec4  _albedoControl[MAX_LAYERS] = {
    {1.f, 1.f, 1.f, 0},
    {1.f, 1.f, 1.f, 0},
    {1.f, 1.f, 1.f, 0},
    {1.f, 1.f, 1.f, 0},
    {1.f, 1.f, 1.f, 0},
    {1.f, 1.f, 1.f, 0},
    {1.f, 1.f, 1.f, 0},
    };

    float _lodDist[2] = { 100.f, 200.f };

    bool _useLOD = true;

private:
    HRESULT Load_HMap(const wstring& hmapPath, const wstring& texRoot);
    HRESULT Add_LayerTexture(const wstring& fullTexPath, uint32 idx, const wstring& normalPath = L"", const wstring& roughnessPath = L"");
    void Flush_WeightMap(uint32 idx);
    HRESULT Bind_ShaderResources();
    HRESULT Build_CollisionData();

public:
    static Shared<MapLandscape> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* pArg) override;
    virtual void Free() override;
};

NS_END
