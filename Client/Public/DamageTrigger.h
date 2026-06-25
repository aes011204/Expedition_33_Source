#pragma once

#include "GameObject.h"
#include "Battle_Enum.h"
#include "SL_Types.h"

NS_BEGIN(Engine)
class ColliderCom;
NS_END

NS_BEGIN(Client)

class DamageTrigger : public GameObject
{
    GENERATED_GAMEOBJECT(DamageTrigger, ObjectType::Trigger)

public:
    DamageTrigger(const ComPtr<Device>& device, const ComPtr<Context>& context);
    DamageTrigger(const DamageTrigger& prototype);
    ~DamageTrigger() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    void OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void Set_LifeTimeSec(float lifeTimeSec);
    void Refresh_CollisionPresetFromOwner();

private:
    HRESULT Ready_Components();

private:
    Shared<ColliderCom> _colliderCom{};

    float _elapsedSec = 0.f;
    float _lifeTimeSec = 0.2f;
    float _damageMultiplier = 1.f;
    AttackType _attackType = AttackType::BaseAttack;
    SL_HitAnimOverride _hitAnimOverride = SL_HitAnimOverride::Hit_Light;
    bool _pendingDestroy = false;
    uint32 _spawnedLevelIndex = 0;

public:
    static Shared<DamageTrigger> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
