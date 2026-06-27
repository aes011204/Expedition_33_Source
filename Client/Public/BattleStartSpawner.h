#pragma once

#include "GameObject.h"

NS_BEGIN(Client)

class BattleStartSpawner : public GameObject
{
    GENERATED_GAMEOBJECT(BattleStartSpawner, ObjectType::BattleStartSpawner)

public:
    BattleStartSpawner(const ComPtr<Device>& device, const ComPtr<Context>& context);
    BattleStartSpawner(const BattleStartSpawner& prototype);
    virtual ~BattleStartSpawner() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Late_Update(float timeDelta) override;

public:
    int32   Get_SpawnerId() const { return _spawnerId; }
    void    Set_SpawnerId(int32 id) { _spawnerId = id; }

private:
    HRESULT Ready_Components();

private:
    int32               _spawnerId = 1;          // MW_ 프리팹이 이 값으로 월드 스포너를 찾는다.
    Shared<ColliderCom> _colliderCom{};          // 배치 위치를 구형으로 시각화하는 용도

public:
    static Shared<BattleStartSpawner> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;


};

NS_END
