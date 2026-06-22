#pragma once

#include "Battle_Enum.h"

NS_BEGIN(Engine)
class GameObject;
NS_END

NS_BEGIN(Client)

class CombatStatCom;

struct SL_MoveRequest
{
    Vec3  worldDir = Vec3::Zero;
    float moveX = 0.f;
    float moveZ = 0.f;
    bool  sprint = false;
    bool  lockOn = false;
};

enum class SL_PlayerState
{
    Idle,
    Run,
    Sprint,
    BasicAttack,
    Dodge,
    Skill,
    Hit,
    Parry,

    Dead,

    End
};

enum class SL_MonState
{
    Intro,
    PhaseChange,
    Idle,
    Chase,
    Attack,
    Hit,
    Dead,
    Stun,

    End
};

enum class SL_MonAttackType
{
    SingleSlash_1,
    SingleSlash_2,
    SingleSlash_3,
    DashBack_Spine,
    DashIn_Stab,
    DashIn_Stab_Estoc,
    Att_Projectile,
    Combo_1,
    Combo_2,

    End
};

enum class SoulLikeAnim
{
    Idle,
    Run,
    Target_Run_Fwd, Target_Run_Back, Target_Run_Left, Target_Run_Right,
    Sprint,
    Attack_1, Attack_2, Attack_3,

    Dodge_Fwd, Dodge_Back, Dodge_Left, Dodge_Right,

    Skill_1, Skill_2, SKill_3, Skill_4,

    Hit_Light, Hit_Middle, Hit_Heavy,

    Jump, Jump_Counter,

    Dead,
};

enum class SL_DodgeDirection
{
    Fwd, Back, Left, Right, END
};

struct SL_DamageInfo
{
    Shared<GameObject>      attacker{};
    Shared<GameObject>      target{};
    Shared<CombatStatCom>   attackerStat{};
    Shared<CombatStatCom>   targetStat{};

    AttackType attackType = AttackType::Invalid;
    float      finalDamage = 0.f;
    bool       isCritical = false;
    bool       isKilled = false;
};

NS_END
