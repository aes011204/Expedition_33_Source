#pragma once
#include "Client_Defines.h"
#include "GameObject.h"
#include "Shader_CBuffer_Define.h"
#include "VIBuffer_Plane.h"

NS_BEGIN(Engine)
class ShaderCom;
NS_END

NS_BEGIN(Client)

class MapWater final : public GameObject
{
    GENERATED_GAMEOBJECT(MapWater, ObjectType::Water)

public:
    typedef struct tagMapWaterDesc final : public GameObject::GAMEOBJECT_DESC
    {
        Vec3  position = Vec3::Zero;
        Quat  rotation = Quat::Identity;
        Vec3  scale = Vec3::One;

        VIBuffer_Plane::DESC planeDesc = {};

        wstring largeWaveNormalPath = {};
        wstring smallWaveNormalPath = {};
        wstring seafoamPath = {};
        wstring cubemapPath = {};

        WaterCB waterParams = {};
    } MAPWATER_DESC;

public:
    MapWater(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MapWater(const MapWater& prototype);
    virtual ~MapWater() = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    virtual void Update(float timeDelta) override;
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;

private:
    Shared<ShaderCom> _shader;
    Shared<VIBuffer_Plane> _vibuffer;

    WaterCB _waterCB = {};

    ComPtr<ID3D11ShaderResourceView> _largeWaveNormalSRV;
    ComPtr<ID3D11ShaderResourceView> _smallWaveNormalSRV;
    ComPtr<ID3D11ShaderResourceView> _seafoamSRV;
    ComPtr<ID3D11ShaderResourceView> _cubemapSRV;

private:
    HRESULT Bind_ShaderResources();
    ComPtr<ID3D11ShaderResourceView> Load_SRV(const wstring& path);

public:
    static Shared<MapWater> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
