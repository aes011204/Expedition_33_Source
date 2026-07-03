#pragma once

#include "Character_BattleBase.h"
#include "Battle_Enum.h"

NS_BEGIN(Engine)

class BlackBoard;
class ColliderCom;
class AIController;
NS_END

NS_BEGIN(Client)

class Mon_WeakPoint;
class MonsterStatCom;
class EffectCom;

class Monster_BattleBase abstract : public Character_BattleBase
{
public:
    struct BattleDecision
    {
        int skillIndex = -1;
        int targetIndex = -1;
        EBattleContext actionType = EBattleContext::Invalid;
        float score = -FLT_MAX;
    };
    //struct BattleAIPolicy
    //{
    //    bool allowBaseAttack = true;
    //    bool allowSkill = true;
    //    bool allowDefend = true;

    //    float baseAttackBias = 0.f;
    //    float skillBias = 0.f;
    //    float defendBias = 0.f;

    //    vector<string> forbiddenSkillIds = {};
    //    vector<string> preferredSkillIds = {};
    //};
    enum class EAITargetStyle
    {
        Single,
        SingleAoE,
        ComboAoE,
        Self,
        Ally,
        RandomEnemy
    };

    struct SkillAIData
    {
        EAITargetStyle targetStyle = EAITargetStyle::Single;

        float openingBias = 0.f;
        float midBias = 0.f;
        float criticalBias = 0.f;

        bool forceOnPhaseEnter = false;

        int cooldownTurn = 0;
        int minTurn = 0;
        int maxUseCount = -1;
    };

    enum class BattlePhase
    {
        None,
        Opening,
        Mid,      // 1페
        Critical  // 2페
    };

    struct SkillChoiceContext
    {
        float hpRatio = 1.f;
        BattlePhase phase = BattlePhase::None;
        int32 aliveHeroCount = 0;

        string lastSkillIndex = "";
        int32 lastTargetIndex = -1;

        string _prevSkillKey[3];
        int32 _prevTarget[3];

        bool phaseJustChanged = false;
        BattlePhase previousPhase = BattlePhase::None;
    };
public:
    Monster_BattleBase(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Monster_BattleBase(const Monster_BattleBase& prototype);
    virtual ~Monster_BattleBase() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;

    json    To_Json() const override;
    void    From_Json(const json& data) override;

    float   Get_BattleGroundOffsetY() const { return _battleGroundOffsetY; }
    void    Set_BattleGroundOffsetY(float value) { _battleGroundOffsetY = value; }
    void    Set_RevivePosition(const Vec3& value) { _revivePosition = value; _hasRevivePosition = true; }
    bool    Has_RevivePosition() const { return _hasRevivePosition; }
    const Vec3& Get_RevivePosition() const { return _revivePosition; }

    void    Decide_Action(ActionRequest& request, const vector<BattleChar>& heroes);
    void    Decide_Action_BTOnly(ActionRequest& request, const vector<BattleChar>& heroes);

public:
    // 약점 세팅
    virtual Shared<Mon_WeakPoint> Get_BattleWeakPoint() const { return nullptr; }
    // 약점 소비
    virtual bool Try_Consume_FreeAimWeakPoint();
    // 약점 숨기기
    void Hide_FreeAimWeakPoint();
    // 부활하면 리셋?
    void Reset_FreeAimWeakPoint();

public:
    float Get_HpRatio()const;
    virtual BattlePhase Get_CurPhase() const;
    virtual bool Should_AppendPreTurnEndPhase(const ActionRequest& request) const { return false; };
    virtual string Get_PreTurnEndSkillKey() const { return ""; };
    float Score_Skill(const Skill_Data* skill, const SkillChoiceContext& context, const vector<BattleChar>& heroes);
    float Score_Target(const Skill_Data* skill, const BattleChar& target, int32 targetIndex);
    float Estimate_SkillDamage(const Skill_Data* skill, const BattleChar& target);
    void Push_SkillHistory(const string& skillKey);
    void Push_TargetHistory(const int32 prevTarget);
    void Advance_SkillRuntimeTurn();
    void Notify_SkillExecutionSucceeded(const string& skillKey);
    SkillChoiceContext Build_SkillChoiceContext(
        const Shared<BlackBoard>& bb,
        const vector<BattleChar>& heroes);

    SkillChoiceContext _cachedContext = {};
protected:
    bool Try_GetDamageFeedbackSocketWorldPosition(Vec3& outWorldPos) const override;
    virtual HRESULT Ready_Components();
    void ActionFromDecision(ActionRequest& request, const BattleDecision decision, const vector<BattleChar>& heroes);

protected:
    Shared<AIController>            _aiCtrl;
    Shared<MonsterStatCom>          _combatStat;
    Shared<EffectCom>               _effectCom;
    float                           _battleGroundOffsetY = 0.f;
    Vec3                            _revivePosition = Vec3::Zero;
    bool                            _hasRevivePosition = false;

public:
    void Free() override;

};

NS_END
