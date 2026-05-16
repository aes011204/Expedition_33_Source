#pragma once
namespace Engine
{
    class ModelCom;
    class Character;
}

namespace Client
{
    enum class Buffs;
    struct Skill_Data;
    enum class EAttackType;
    class CombatStatCom;
    // =============================
    // 상태 / 컨텍스트
    // =============================

    //현재 턴에서 플레이어가 무엇을 선택 중인지
    enum class BattleTurnState
    {
        None,
        ChooseSkill,
        AttackTargeting,
        ChooseItem,
        ChooseAction,
        SkillTargeting,
        ItemTargeting,
        UltimateTargeting,
        ChooseUltimate
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
        FreeAimShot,
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
        FreeAimShot,
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
        SkillCast,
        BuffEffect,
        FreeAim,
        Attack,
        Counter,
        Lumina,
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
    enum class EDefenseMoveType
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
        FreeAimShot,
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

    struct Item_Data
    {
        
    };

    //-------------------------------------
    enum class EEnemyClass
    {
        Invalid,
        Normal,
        Elite,
        Boss
    };

    enum class EAttackType
    {
        None,
        Fire,
        Ice,
        Lightning,
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
        EAttackType        attackType = EAttackType::None;
        EElementalAffinity affinity = EElementalAffinity::Normal;
    };

    struct BattleChar
    {
        Shared<Character> actor;
        Shared<CombatStatCom> statCom;
        Shared<ModelCom> ModelCom;
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


    struct Skill_Data
    {
        string skillName={""};            // "FireSlash"
        string scriptClassName={""};      // "FireSlashScript"
        vector<string> animationKey;
        string IdleAnimationKey;

        int32 apCost = 0;
        ECharacterTeamSelector targetingType={};

        uint32 _curAnimIndex = {};

        //
        EAttackType element={};

        // 
        Buffs ailmentType = Buffs::None;
        float ailmentChance = 0.f;
    };
    struct Action
    {
        BattleChar _curChar = {};
        BattleChar  target = {};

        EBattleContext type = {};

        Item_Data   _selected_ItemKey = {};
        Skill_Data   _selected_SkillKey = {};

    };
    struct TurnEntry
    {
        BattleChar hero;
        float nextTime;
    };

    struct DamageContext
    {
        BattleChar attacker;
        BattleChar target;

        float baseDamage;

        EDamageReason damageType;
        EAttackType element;

        float finalMultiplier = 1.0f;
        float flatBonus = 0.0f;

        bool canCrit = true;
        bool ignoreDefense = false;
    };


   
    //------------------------------------
   
}
