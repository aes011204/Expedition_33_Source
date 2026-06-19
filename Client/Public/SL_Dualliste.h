#pragma once

#include "SL_Base.h"
NS_BEGIN(Engine)

class AIController;
NS_END
NS_BEGIN(Client)

class Mon_Dualliste_Body;
class Mon_Dualliste_Sword_L;
class Mon_Dualliste_Sword_R;
class MonsterStatCom;

class SL_Dualliste final : public SL_Base
{
    GENERATED_GAMEOBJECT(SL_Dualliste, ObjectType::Monster)

public:
    SL_Dualliste(const ComPtr<Device>& device, const ComPtr<Context>& context);
    SL_Dualliste(const SL_Dualliste& prototype);
    ~SL_Dualliste() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void BeginPlay() override;
    void Priority_Update(float timeDelta) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;

public:
    void OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    Shared<CombatStatCom> Get_Stat() const override;
    Shared<ColliderCom> Get_AttackCollider() const override;
    bool Is_Dead() const override;
    void Apply_Damage(const SL_DamageInfo& damageInfo) override;

protected:
    HRESULT Ready_Components();
    HRESULT Ready_PartObjects();

private:
    Shared<MonsterStatCom> _combatStat = {};
    Shared<ColliderCom> _attackColliderCom = {};
    Shared<AIController>         _aiCtrl;
    Shared<Mon_Dualliste_Body> _body = {};
    Shared<Mon_Dualliste_Sword_L> _swordL = {};
    Shared<Mon_Dualliste_Sword_R> _swordR = {};

public:
    static Shared<SL_Dualliste> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
