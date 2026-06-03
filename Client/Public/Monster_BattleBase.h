#pragma once

#include "Character_BattleBase.h"
#include "Battle_Enum.h"

NS_BEGIN(Engine)
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
    struct BattleAIPolicy
    {
        bool allowBaseAttack = true;
        bool allowSkill = true;
        bool allowDefend = true;

        float baseAttackBias = 0.f;
        float skillBias = 0.f;
        float defendBias = 0.f;

        vector<string> forbiddenSkillIds = {};
        vector<string> preferredSkillIds = {};
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

    void    Decide_Action(ActionRequest& request, const vector<BattleChar>& heroes);
    void    Decide_Action_BTOnly(ActionRequest& request, const vector<BattleChar>& heroes);

public:
    // 약점 세팅
    virtual Shared<Mon_WeakPoint> Get_BattleWeakPoint() const { return nullptr; }

    // 약점 소비
    bool Try_Consume_FreeAimWeakPoint();
    // 약점 숨기기
    void Hide_FreeAimWeakPoint();
    // 부활하면 리셋?
    void Reset_FreeAimWeakPoint();

protected:
    virtual HRESULT Ready_Components();
    void ActionFromDecision(ActionRequest& request, const BattleDecision decision, const vector<BattleChar>& heroes);

protected:
    Shared<AIController>            _aiCtrl;
    Shared<MonsterStatCom>          _combatStat;
    Shared<EffectCom>               _effectCom;
    float                           _battleGroundOffsetY = 0.f;

public:
    void Free() override;

};

NS_END
