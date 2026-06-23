#pragma once

#include "SL_Base.h"

NS_BEGIN(Client)

class Mon_Lancelier_Body;
class Mon_Lancelier_Weapon;
class MonsterStatCom;

// 쌍검 들어오기 전 충돌처리 테스트 임시용 클래스
class SL_Test_Lancelier : public SL_Base
{
    GENERATED_GAMEOBJECT(SL_Test_Lancelier, ObjectType::Monster)

public:
    enum class Anim
    {
        Idle,
        Attack_1, Attack_2,
        Hit,
        Dead
    };

public:
    SL_Test_Lancelier(const ComPtr<Device>& device, const ComPtr<Context>& context);
    SL_Test_Lancelier(const SL_Test_Lancelier& prototype);
    ~SL_Test_Lancelier() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void BeginPlay() override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;

    // 충돌체 중심점 구하기
    Vec3 Resolve_ColliderWorldCenter(const Shared<ColliderCom>& collider) const;

public:
    void OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;

    Shared<CombatStatCom> Get_Stat() const override;
    Shared<ColliderCom> Get_AttackCollider() const ;
    void Set_AttackHitAnimOverride(const string& colliderTag, SL_HitAnimOverride hitAnimOverride) override;
    void Clear_AttackHitAnimOverride(const string& colliderTag) override;
    bool Is_Dead() const override;
    void Apply_Damage(const SL_DamageInfo& damageInfo) override;

private:
    HRESULT Ready_Components();
    HRESULT Ready_PartObjects();

    void Play_Anim(Anim anim, float blendDuration, bool loop);
    void Update_Attack(float timeDelta);
    void LookAt_Player();

private:
    Shared<MonsterStatCom> _combatStat{};
    Shared<ColliderCom> _attackColliderCom{};
    Shared<Mon_Lancelier_Body> _body{};
    Shared<Mon_Lancelier_Weapon> _weapon{};

    Anim _currentAnim = Anim::Idle;
    Anim _nextAttackAnim = Anim::Attack_1;
    SL_HitAnimOverride _attackHitAnimOverride = SL_HitAnimOverride::Hit_Light;
    float _attackCooldown = 0.f;
    float _attackRange = 2.2f;
    float _attackInterval = 2.0f;

public:
    static Shared<SL_Test_Lancelier> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;

};

NS_END
