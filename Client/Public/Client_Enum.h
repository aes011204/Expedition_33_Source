#pragma once

namespace Client
{
    enum class PlayerState
    {
        Idle,
        Move,
        Jump,
        WorldAction,
        GameMenu,
        FreeAim,

        Climb,
        RopeAction,
        End,
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


    enum class ERuntimeBattleStatType
    {

        CurrentHP,
        CurrentAP,
        CurrentHPPercentage,
        CurrentAPPercentage,
        Invalid
    };

    enum class PlayerCharacterType
    {
        Gustave,
        Lune,
        Maelle,
        None,
    };

    // 캐릭터별 Physics 정보
    enum class HairPhysicsProfile
    {
        Lune,
        Maelle,
        Gustave,
    };

    enum class HeroPartySlot : uint8
    {
        Slot_0,
        Slot_1,
        Slot_2,

        END
    };

    enum class HeroId : uint8
    {
        Gustave,
        Lune,
        Maelle,

        END
    };

    // 프리뷰 애니메이션 타입
    enum class PreviewAnimState
    {
        Overview_Intro,
        Overview_Idle,

        Skill_Intro,
        Skill_Idle,

        SkillTree_Intro,
        SkillTree_Idle,

        Weapon_Intro,
        Weapon_Idle,

        Wardrobe_Intro,
        Wardrobe_Idle,

        Pictos_Intro,
        Pictos_Idle,
    };

    enum class SoulLikePlayerState
    {
        Idle,
        Move,
        BasicAttack,
        Dodge,
        Skill,
        Hit,
        Dead,
        End
    };

    enum class SoulLikeAnim
    {
        Idle,
        Move,
        Attack_1,
        Attack_2,
        Attack_3,
        Dodge,
        Hit,
        Dead,
    };

}

