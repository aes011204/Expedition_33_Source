#pragma once
#include "Client_Defines.h"
#include "GameObject.h"
#include "VIBuffer_TerrainUE.h"

NS_BEGIN(Engine)
class ShaderCom;
NS_END

class Terrain_UE final : public GameObject
{
    GENERATED_GAMEOBJECT(Terrain_UE, ObjectType::Terrain)

public:
    struct TERRAIN_UE_DESC : public GameObject::GAMEOBJECT_DESC
    {
        VIBuffer_TerrainUE::TerrainDesc terrainDesc;
    };

public:
    Terrain_UE(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Terrain_UE(const Terrain_UE& prototype);
    virtual ~Terrain_UE() = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    virtual void Priority_Update(float timeDelta) override;
    virtual void Update(float timeDelta) override;
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;

private:
    Shared<ShaderCom> _shader;
    Shared<VIBuffer_TerrainUE> _vibuffer;

private:
    HRESULT Bind_ShaderResources();

public:
    static Shared<Terrain_UE> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* pArg) override;
    virtual void Free() override;
};

