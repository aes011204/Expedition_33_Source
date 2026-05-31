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
    static constexpr const char* layerNames[MAX_LAYERS] = { "g_Layer0",  "g_Layer1",  "g_Layer2",  "g_Layer3" };
    static constexpr const char* weightNames[MAX_LAYERS] = { "g_Weight0", "g_Weight1", "g_Weight2", "g_Weight3" };

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

    float _lodDist[2] = { 50.f, 150.f };

private:
    HRESULT Load_HMap(const wstring& hmapPath, const wstring& texRoot);
    HRESULT Add_LayerTexture(const wstring& fullTexPath, uint32 idx);
    void Flush_WeightMap(uint32 idx);
    HRESULT Bind_ShaderResources();
    HRESULT Build_CollisionData();

public:
    static Shared<MapLandscape> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* pArg) override;
    virtual void Free() override;
};

NS_END
