#pragma once

#include "GameObject.h"

NS_BEGIN(Client)

class Trigger_WorldAction : public GameObject
{
    GENERATED_GAMEOBJECT(Trigger_WorldAction, ObjectType::WorldActionTrigger)

public:
    Trigger_WorldAction(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Trigger_WorldAction(const Trigger_WorldAction& Prototype);
    virtual ~Trigger_WorldAction() = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;
    void    OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void    Set_LifeTimeSec(float lifeTimeSec);

private:
    HRESULT Ready_Components();

private:
    Shared<ColliderCom> _colliderCom{};

    float _elapsedSec = 0.f;
    float _lifeTimeSec = 0.2f;   // 월드액션 타격 판정만 하고 바로 사라지는 1회용 충돌체
    bool  _pendingDestroy = false;
    uint32 _spawnedLevelIndex = 0;

public:
    static Shared<Trigger_WorldAction> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
