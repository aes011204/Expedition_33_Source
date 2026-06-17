#pragma once

#include "Character_WorldBase.h"

NS_BEGIN(Engine)
class MovementCom;
NS_END

NS_BEGIN(Client)

class Body_Player;
class Head_Player;
class Hair_Player;
class EffectCom;

class SL_Maelle final : public Character_WorldBase
{
    GENERATED_GAMEOBJECT(SL_Maelle, ObjectType::Player)

public:
    enum class SoulLikeAnim
    {
        Idle,
        Move,
        Attack,
        Dodge,
        Hit,
        Dead,
    };

public:
    SL_Maelle(const ComPtr<Device>& device, const ComPtr<Context>& context);
    SL_Maelle(const SL_Maelle& prototype);
    ~SL_Maelle() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;
    void    Priority_Update(float timeDelta) override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;

public:
    void    Apply_MoveInput(const Vec3& worldDir, bool sprint, float timeDelta);
    void    Stop_Move();
    bool    Play_SoulLikeAnim(SoulLikeAnim anim, float blendDuration, bool loop);
    bool    Is_AnimFinished() const;

    void    Set_CombatLocked(bool locked) { _combatLocked = locked; }
    bool    Is_CombatLocked() const { return _combatLocked; }

    void    Set_InvincibleTime(float value) { _invincibleRemain = max(0.f, value); }
    bool    Is_Invincible() const { return _invincibleRemain > 0.f; }

    void    Apply_Damage(float damage);
    float   Get_Hp() const { return _hp; }
    bool    Is_Dead() const { return _hp <= 0.f; }

    Body_Player* Get_Body() const { return _body.get(); }

    void    Reset_CombatHp(float maxHp);
    void    Clear_CombatLockIfAnimFinished();



private:
    HRESULT Ready_Components();
    HRESULT Ready_PartObjects();

private:
    Shared<MovementCom> _movementCom{};
    Shared<EffectCom> _effectCom{};

    Shared<Body_Player> _body{};
    Shared<Head_Player> _head{};
    Shared<Hair_Player> _hair{};

    float _hp = 100.f;
    float _invincibleRemain = 0.f;
    bool _combatLocked = false; // 공격/피격처럼 이동 입력을 막아야 하는 전투 상태에서만 true로 둔다.

public:
    static Shared<SL_Maelle> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
