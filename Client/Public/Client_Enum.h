#pragma once

namespace Client
{
    enum class PlayerState { Idle, Run, Jump, Attack, Dead, END };

    enum class BattleTurnState
    {
        ChooseSkill,      // 스킬 선택 UI 표시
        AttackTargeting,  // 타겟 선택 (기본 공격용)
        ChooseItem,       // 아이템 선택 UI 표시
        ChooseAction,     // 행동 선택 UI 표시 (공격/스킬/아이템/방어)
        SkillTargeting,   // 타겟 선택 (스킬용)
        ItemTargeting,    // 타겟 선택 (아이템용)
        UltimateTargeting,// 타겟 선택 (궁극기용)
        ChooseUltimate,   // 궁극기 선택 UI 표시
    };
    enum class BattleContext
    {
        Skill,
        Defend,
        Dodge,
        Parry,
        TurnStart,
        BaseAttack,
        ConsumableItem,
        GotHit,
        Died,
        Revived,
        Counter,
        BattleStart,
        Heal,
    };

}

