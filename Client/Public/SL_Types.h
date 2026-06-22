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

    Dead,
    Parry,
    Jump,
    JumpCounter,

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
    Parry,

    Idle_To_Run,
    Run_To_Idle,
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

struct SL_SkillCooldownView
{
    string skillKey{};
    float remainingSec = 0.f;
    float durationSec = 0.f;

    bool Is_OnCooldown() const { return remainingSec > 0.f && durationSec > 0.f; }

    float Get_Ratio() const
    {
        return durationSec > 0.f ? clamp(remainingSec / durationSec, 0.f, 1.f) : 0.f;
    }
};

struct SL_SkillCooldownRuntime
{
    string skillKey{};
    float remainingSec = 0.f;
    float durationSec = 0.f;
};


NS_END
