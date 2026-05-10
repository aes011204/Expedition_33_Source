#pragma once

namespace Client
{
    enum class PlayerState
    {
        Idle, 
        Move,
        Jump,       // Drop도 간단하게 같이 처리 해야하는가?
        Attack, 

        Climb,
        RopeAction,
        End
    };

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

    enum class StatType
    {
        Vitality,   // 활력
        Might,      // 무력
        Agility,    // 민첩
        Defense,    // 방어
        Luck,       // 행운
        END
    };

    enum class SkillType
    {
        Invalid,        // 잘못된 값 / 미설정
        Skill,          // 일반 기술
        GradientAttack, // 특수 해금 공격
        Item,           // 아이템으로 해금/사용되는 타입
        END
    };

    enum class SkillNodeType
    {
        Invalid,
        Learnt,         // 이미 배운 상태
        Learnable,      // 지금 배울 수 있는 상태
        Locked,         // 아직 ㄴㄴ
        Unaffordable,   // 조건은 됐는데 스킬포인트가 부족

        END
    };

}

