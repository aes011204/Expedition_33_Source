#pragma once

#include "Client_Enum.h"

namespace Engine
{
    class ModelCom;
    class Character;
}

namespace Client
{
    class Character_BattleBase;
    struct BattleActionPhase;
    struct Skill_Data;
    enum class ElementalType;
    class CombatStatCom;
    class BattleMechanismCom;

    // =============================
    // 상태 / 컨텍스트
    // =============================

    //현재 턴에서 플레이어가 무엇을 선택 중인지
    enum class BattleTurnState
    {
        None,
        ChooseSkill,
        AttackTargeting,
        AttackPlaying,      // 타겟 선택 끝나고, 평타 or 스킬 실행중일 때
        ChooseItem,
        ChooseAction,
        SkillTargeting,
        ItemTargeting,
        UltimateTargeting,
        ChooseUltimate,
        FreeAimTargeting,
    };

    // 현재 전투 상황의 유형이나 행동 컨텍스트
    enum class EBattleContext
    {
        Invalid,
        Skill,
        Defend,
        Dodge,
        Parry,
        TurnStart,
        FreeAimShoot,
        BaseAttack,
        ConsumableItem,
        Debug,
        QTE,
        GotHit,
        Died,
        Revived,
        Counter,
        BattleStart,
        PassiveEffect,
        Heal,
        LuminaOverride
    };

    //액션 시퀀스 내의 이벤트 타입
    enum class EBattleActionEvent
    {
        None,
        ComboWindowStart,
        ComboWindowPre,
        ComboWindowEnd,
        ActionEffect,
        ComboExitPoint
    };

    //전투 장면 전환 효과 타입을 정의
    enum class EBattleTransitionType
    {
        None,
        Fade,
        Blur
    };

    //특정 행동이 금지된 이유
    enum class EBattleForbiddenActionReason
    {
        Invalid,
        OutOfMana,
        EmptyConsumable,
        NoValidTarget,
        RequirementsNotMet
    };

    //전투 종료 결과
    enum class EBattleEndResult
    {
        Invalid,
        Victory, // 승리
        Defeat, // 패배
        Neutral // 도망
    };

    // =============================
    // 자원 / 게이지 변경
    // =============================

    //AP(행동력) 변경 원인
    enum class EAPChangeReason
    {
        Invalid,
        SkillEffect,
        Defend,
        Dodge,
        Parry,
        TurnStart,
        FreeAimShoot,
        BaseAttack,
        ConsumableItem,
        Debug,
        PassiveEffect,
        BattleStart,
        Drain,
        SkillCost
    };

    //그라디언트 게이지 등 특정 게이지 변화 원인
    enum class EGradientChangeReason
    {
        Invalid,
        APSpentForSkill,
        PassiveEffect
    };

    // =============================
    // 데미지 / 공격 관련
    // =============================

    //데미지 발생 원인
    enum class EDamageReason
    {
        Invalid,
        Skill, // 일반 스킬 
        BuffEffect,
        FreeAim,
        Attack,
        Counter,
        Item,
        GradientCounter,
        RangeCounter,
        AOECounter,
        JumpCounter
    };

    //데미지 계산에 쓰이는 보정 타입
    enum class EDamageModifierType
    {
        Invalid,
        RelativeMultiplier,
        FlatValue,
        AbsoluteMultiplier
    };

    //데미지 값의 상태
    enum class EDamageValueState
    {
        Invalid,
        Editable,
        Initial,
        Final
    };
    enum class AttackType
    {
        Invalid,
        SingleSkill, // 일반 스킬 
        BuffEffect,
        FreeAimWeak,
        FreeAimLight,
        BaseAttack,
        Counter,
        Combo,
        ComboLast,
        //GradientCounter,
        SingleAOE,
        //JumpCounter
    };
    //EMissReason
    enum class EMissReason
    {
        Invalid,
        Dodge,
        Parry,
        Nullified,
        Absorbed,
        Jump,
        GradientParry,
        AutoSkillCounter
    };

    //카운터 공격/반격의 종류
    enum class ECounterType
    {
        Invalid,
        CloseRange,
        LongRange,
        AOE
    };

    // =============================
    // 방어 / 반응
    // =============================

    //방어 기술 분류
    enum class DefenseMoveType
    {
        Defend,
        Dodge,
        Parry
    };

    //브레이크 혹은 스턴이 발생한 이유
    enum class EBreakStunReason
    {
        Invalid,
        Skill,
        PassiveEffect
    };

    //차단 처리 방식 구분
    enum class EBlockedActionType
    {
        Invalid,
        BlockAction,
        OnlyFeedback
    };

    // =============================
    // 스탠스 / 전투 스타일
    // =============================

    //E_StanceType
    enum class EStanceType
    {
        Stanceless,
        Offensive,
        Defensive,
        Virtuose
    };

    // =============================
    // 퍼펙션 / 랭크
    // =============================


    //퍼펙션/완벽 상태가 생긴 이유를 나타내는 타입
    enum class EPerfectionReason
    {
        Invalid,
        Skill,
        Defend,
        Dodge,
        Parry,
        TurnStart,
        FreeAimShoot,
        BaseAttack,
        ConsumableItem,
        Debug,
        QTE,
        GotHit,
        Died,
        Revived,
        Counter,
        BattleStart,
        PassiveEffect,
        Heal,
        LuminaOverride
    };


    //퍼펙션 값 변경 종류
    enum class EPerfectionUpdateType
    {
        Gain,
        Lose,
        SimpleUpdate
    };

    //퍼펙션 랭크 등급
    enum class EPerfectionRank
    {
        Invalid,
        D,
        C,
        B,
        A,
        S
    };

    // =============================
    // 타겟팅 / 상태 검색
    // =============================

    //대상의 생존 상태 필터
    enum class EAliveStatus
    {
        Invalid,
        Alive,
        Dead,
        Any
    };

    //팀 대상 선택 기준
    enum class ECharacterTeamSelector
    {
        Invalid,
        Allies,
        Enemies,
        Any
    };

    ////런타임 전투 스탯 조회 타입
    //enum class ERuntimeBattleStatType
    //{
    //    Invalid,
    //    CurrentHP,
    //    CurrentAP,
    //    CurrentHPPercentage,
    //    CurrentAPPercentage
    //};

    //숫자 비교/검색 타입
    enum class ENumericValueQuery
    {
        Invalid,
        Minimum,
        Maximum
    };

    // =============================
    // 상태 / 이펙트 원인
    // =============================

    //스테인(상태 이상) 변경 이유
    enum class EStainChangeReason
    {
        Invalid,
        SkillCast,
        PassiveEffect,
        ResetStains,
        Debug
    };

    //치유가 발생한 이유
    enum class EHealReason
    {
        Invalid,
        SkillCast,
        PassiveEffect,
        Debug,
        Absorb
    };

    // =============================
    // 기타 고유 메커닉
    // =============================

    //전투에서 보고 있는 방향
    enum class EBattleLookDirection
    {
        Invalid,
        Left,
        Right
    };

    //프리즈프레임(느려짐/강조) 타입
    enum class EFreezeFrame
    {
        No,
        VeryLight,
        Light,
        Medium,
        Heavy,
        VeryHeavy,
        AttackSlowMo,
        FreeAimWeakpoint
    };

    //프리즈프레임(느려짐/강조) 타입
    enum class ETransformMaskMechanic
    {
        Invalid,
        Balanced,
        Heavy,
        Caster,
        Agile,
        Almighty
    };

    //------------------------------------------




    struct FEnemyPortraitData
    {
        //Texture2D* TimelinePortrait;
        double PortraitOffset;
    };

    enum class BattleItemType
    {
        Invalid,
        HealingTint,
        EnergyTint,
        ReviveTint,
    };

    struct Item_Data
    {
        string itemKey = "";
        wstring displayName = TEXT("");
        BattleItemType itemType = BattleItemType::Invalid;

        int32 count = 0;      // 전투 중 현재 사용 가능한 수량
        int32 healHp = 0;     // HealingTint 전용 회복량
        int32 restoreAp = 0;  // EnergyTint 전용 AP 회복량
        int32 reviveHp = 0;   // ReviveTint 전용 부활 후 HP

        bool IsValid() const
        {
            return itemType != BattleItemType::Invalid;
        }
    };

    //-------------------------------------
    enum class EEnemyClass
    {
        Invalid,
        Normal,
        Elite,
        Boss
    };

    enum class ElementalType
    {
        None,
        Fire,
        Ice,
        Lightning,
        Earth,
        Dark,
        Light
    };

    enum class EElementalAffinity
    {
        Normal,
        Weak,
        Resist,
        Immune,
        Absorb
    };

    struct  FLootData
    {
        Item_Data Item;   // 예: DT_jRPG_Items_Composite : Lanceram
        int Quantity;               // 1
        double LootPercentageChance;// 100.0
        int LootContextLevelOffset; // 0
    };

    struct FCommonCharacterStats
    {
        double BurnPercentage;//Burn 데미지 계산 배율(최대 HP의)
        double RegularDamageReductionMultiplier;//일반 공격 피해 감소 배율
        double FreeAimDamageReductionMultiplier;//(약점사격) 피해 감소
        int ShieldPointsAtSpawn;//전투 시작 시 기본 보호막
        double ReceivedSkillDamageMultiplier;//스킬 데미지 받는 배율
        double ReceivedFreeAimDamageMultiplier; //자유조준 피해 배율
    };


    struct FElementalAffinity
    {
        ElementalType        attackType = ElementalType::None;
        EElementalAffinity affinity = EElementalAffinity::Normal;
    };

    struct BattleChar
    {
        HeroId heroId = HeroId::END; // 어떤 월드 영웅과 연결된 배틀 캐릭터인지 끝날 때 역반영으로 사용
        Shared<Character_BattleBase> actor;
        Shared<CombatStatCom> statCom;
        Shared<ModelCom> ModelCom;
        Shared<BattleMechanismCom> mechanismCom;
    };


    enum class BuffType { beforeTurn, changeStat, dot, SLogic };

    enum class Buffs
    {
        //버프

        // 상태이상
        // 
        //turn
        None,

        Burn,

        Freeze,
        Stun,
        Charm,
        Rage,

        //changeStat
        Powerful,
        Shell,
        Mark,
        Blight,
        Berserk,
        Powerless,
        Shield,
        Defenceless,


        Slow,
        Rush,

        Exhausted,
        Bound,
        Silence,
        Dizzy,
        Cursed,
        Inverted,

        // 디버프

    };

    enum class TargetType
    {
        Invalid,
        Primary,
        RandomEnemy,
        AllEnemies,
        Self,
        ETC, //스폰 , 쉴드 등 
    };

    struct TurnEntry
    {
        BattleChar hero;
        float nextTime = 0.f;
        uint64 orderSeq = 0; // 같은 nextTime이면 먼저 예약된 턴부터 처리한다.
    };

    struct DamageContext
    {
        BattleChar attacker;
        BattleChar target;

        float baseDamage;

        EDamageReason damageType;
        ElementalType element;

        float finalMultiplier = 1.0f;
        float flatBonus = 0.0f;

        bool canCrit = true;
        bool ignoreDefense = false;
    };

    struct DamageAppliedInfo
    {
        BattleChar attacker{};      // 때린놈
        BattleChar target{};        // 실제 HP가 깎이 대상
        float damage = 0.f;         // 최종 데미지
        AttackType attackType = AttackType::Invalid; // 기존 피격 애니메이션 분기

        bool isCritical = false;    // 크리티컬
        bool isShieldBlocked = false;
        uint32 prevShield = 0;
        uint32 currentShield = 0;
    };

    struct Skill_Data
    {
        string characterId = "";
        string skillKey = "";
        string scriptClassName = "";
        wstring battleAnnounceActionText = L"";

        vector<BattleActionPhase> AnimPhases = {};

        vector<TargetType> targetingType = {};


        int32 apCost = 0;
        int32 AttackCount = 0;
        //ECharacterTeamSelector targetingType = ECharacterTeamSelector::Invalid;
        ElementalType element = ElementalType::None;
        Buffs buff = Buffs::None;
        float ailmentChance = 0.f;

        int32 buffDurationTurn = 0;
    };

    //------------------------------------
    struct ActionRequest
    {
        EBattleContext type = EBattleContext::Invalid;

        BattleChar actor = {};
        BattleChar InitTarget = {};
        string skillKey = "";
        BattleItemType itemType = BattleItemType::Invalid;
    };

    //
    enum class BattleMovementPolicy
    {
        None,               // 제자리
        SnapToTarget,       // 바로 목표 위치로 스냅
        MoveToTarget,        // 대쉬/돌진처럼 전체 이동량을 목표 위치에 맞춤
        ReturnToOrigin,     // 시작 위치로 복귀
        RootMotionToTarget,    // 걷기/달리기처럼 루트모션 속도 기반
        Custom
    };

    enum class BattleTargetAnchor
    {
        None,
        Self,
        Origin,
        PrimaryTarget,
        TargetCenter,
        FormationSlot,
        CustomSocket

    };

    struct BattleActionPhase
    {
        string animationKey = "";
        string cameraPresetId = "";
        string cameraClipId = "";
        //  string sequenceKey = "";
        uint32 phaseIndex = {};
        BattleMovementPolicy movementPolicy = BattleMovementPolicy::None;
        BattleTargetAnchor targetAnchor = BattleTargetAnchor::PrimaryTarget;
        bool useDistance = false;
        float offsetDistance = {};
        bool faceTarget = true;
        //   bool disableSequenceSnapping = false;
    };

    struct BattleSlowDesc
    {
        float elapsed = 0.f;
        float duration = 0.6f;
        float worldTimeScale = 0.12f;
    };

    enum class BattleAnimType
    {
        Base_Idle,

        Battle_Start,
        Turn_Start,

        Hit_Heavy,
        Hit_Medium,
        Hit_Light,

        FreeAim_Enter,
        FreeAim_Idle,
        FreeAim_Shoot,

        Dodge,
        Parray,
        Counter,

        Dead,

        Item_Enter, // Idle은 노티파이로
        Item_Idle,
        Item_Use,

        Revive,

        Skill_Enter,
        Skill_Idle,

        Offensive_Idle, // 마엘 공격 태세 idle
        Defensive_Idle, // 마엘 방어 태세 idle
        Virtuose_Idle,  // 마엘 명인 태세 idle

        Targeted_Start, // 적 턴 시작할 때
        Targeted_Idle,
        Targeted_End,

        Return,
    };

    enum class BattleReactionType
    {
        None, Parry, Dodge, QTE, END
    };

    enum class BattleQTEResult
    {
        None, Fail, SuccessNormal, SuccessPerfect, END
    };

    enum class BattleQTEPreset
    {
        Center,

        // 구
        LumiereAssault_QTE1,
        LumiereAssault_QTE2,

        UnleashedCharge_QTE1,
        UnleashedCharge_QTE2,

        MarkingShot,

        // 루
        Immolation_QTE1,
        Immolation_QTE2,

        IceGust_QTE1,

        Thunderfall_QTE1,
        Thunderfall_QTE2,

        // 마
        OffensiveSwitch_QTE1,

        Percee_QTE1,

        FleureFury_QTE1,

        END
    };

    struct BattleQTERequest
    {
        // 일단은 애니메이션에 맞춰서 초기값만 세팅
        float introDuration = 21.f / 63.f;      // QTE_Intro.uianim 기준
        float progressDuration = 80.f / 120.f;  // QTE_Progress.uianim 기준
        float perfectStartRatio = 61.f / 80.f;  // 노란 구간 시작
        float perfectEndRatio = 1.f;            // 노란 구간 끝

        // QTE 화면 배치. ANS_QTE에서 preset 고르고, request에는 실제 적용값
        BattleQTEPreset placementPreset = BattleQTEPreset::Center;
        Vec2 screenPosition = Vec2(800.f, 450.f);
        float screenScale = 1.f;
    };

    // 배틀 턴 조준 시 파츠
    enum class FreeAimHitPart
    {
        None, Body, WeakPoint, END
    };

    struct BattleFreeAimHitTarget
    {
        BattleChar target{};
        FreeAimHitPart hitPart = FreeAimHitPart::None;

        bool Is_Valid() const
        {
            return target.actor != nullptr && target.statCom != nullptr && hitPart != FreeAimHitPart::None;
        }
    };

    // 튜토리얼 순서
    enum class BattleTutorialStep
    {
        BattleSystem,
        TurnOrder,
        Attack,
        EnemyTurn,
        Dodge,
        Skill,
        QTE,
        Counter,
        FreeAim,
        END
    };

    struct BattleTutorialRequest
    {
        BattleTutorialStep step = BattleTutorialStep::END;
        bool pauseBattle = true; // 튜토리얼 표시 중, Model을 멈출것인지
        bool blockInput = true;  // 입력은?
        float delaySec = 0.f;    // 연출과 겹치지 않게, 튜토리얼 표시 늦출 때
    };

}
