#include "Monster_BattleBase.h"
#include "ClientInstance.h"

#include "AIController.h"
#include "BlackBoard.h"
#include "ClientInstance.h"
#include "MonsterStatCom.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "GameInstance.h"
#include "MonsterStatCom.h"
#include "Monster_Part.h"
#include "EffectCom.h"
#include "Mon_WeakPoint.h"
#include "BlackBoard.h"

Monster_BattleBase::Monster_BattleBase(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Character_BattleBase(device, context)
{
}

Monster_BattleBase::Monster_BattleBase(const Monster_BattleBase& prototype)
    : Character_BattleBase(prototype)
{
}

HRESULT Monster_BattleBase::Initialize_Prototype()
{
    return Character_BattleBase::Initialize_Prototype();
}

HRESULT Monster_BattleBase::Initialize(void* arg)
{
    CHECK_FAILED(Character_BattleBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    return S_OK;
}

void Monster_BattleBase::BeginPlay()
{
    Character_BattleBase::BeginPlay();
    
    if (auto* body = dynamic_cast<Monster_Part*>(Get_PartObject(L"Part_Body")))
    {
        if (Shared<ModelCom> model = body->Get_Model())
        {
            model->Refresh_AnimStateMappingFromOwnerContext();
        }
    }
}

void Monster_BattleBase::Update(float timeDelta)
{
    if (_combatStat)
        _combatStat->Update(timeDelta);

    Character_BattleBase::Update(timeDelta);

    if (_effectCom)
        _effectCom->Update(timeDelta); // 몬스터 사망 시 디졸브용
}

void Monster_BattleBase::Late_Update(float timeDelta)
{
    Character_BattleBase::Late_Update(timeDelta);
}

json Monster_BattleBase::To_Json() const
{
    json root = Character_BattleBase::To_Json();
    root["custom_properties"]["battle_ground_offset_y"] = _battleGroundOffsetY;
    return root;
}

void Monster_BattleBase::From_Json(const json& data)
{
    Character_BattleBase::From_Json(data);

    _battleGroundOffsetY = 0.f;
    _revivePosition = Vec3::Zero;
    _hasRevivePosition = false;

    if (data.contains("custom_properties") && data["custom_properties"].is_object())
    {
        const json& customProperties = data["custom_properties"];
        _battleGroundOffsetY = customProperties.value("battle_ground_offset_y", 0.f);
    }
}

void Monster_BattleBase::ActionFromDecision(ActionRequest& request, const BattleDecision decision, const vector<BattleChar>& heroes)
{
    auto stat = Get_Component<MonsterStatCom>();
    if (stat == nullptr)
        return;

    const auto& skillIds = stat->Get_SkillKey();

    if (decision.targetIndex < 0 || decision.targetIndex >= static_cast<int>(heroes.size()))
        return;

    if (decision.skillIndex < 0 || decision.skillIndex >= static_cast<int>(skillIds.size()))
        return;

    const string& skillId = skillIds[decision.skillIndex];
    const Skill_Data* skill = CLIENT->Get_Skill(skillId);
    if (skill == nullptr)
        return;

    request.InitTarget = heroes[decision.targetIndex];
    request.skillKey = skill->skillKey;
    request.type = decision.actionType;
}

void Monster_BattleBase::Decide_Action_BTOnly(ActionRequest& request, const vector<BattleChar>& heroes)
{
    if (heroes.empty() || _aiCtrl == nullptr)
        return;

    const Shared<BlackBoard> bb = _aiCtrl->Get_BlackBoard();
    CHECK_NULL(bb);


    bb->Set_Value("Heroes", heroes);
    bb->Set_Value("DecisionComplete", false);
    bb->Set_Value("TargetIndex", -1);
    bb->Set_Value("SkillIndex", -1);
    bb->Set_Value("ActionType", static_cast<int>(EBattleContext::Invalid));

    _aiCtrl->Run_BT_Once();

    BattleDecision decision = {};
    if (bb->Has_Value("DecisionComplete") && bb->Get_Value<bool>("DecisionComplete"))
    {
        if (bb->Has_Value("TargetIndex"))
            decision.targetIndex = bb->Get_Value<int>("TargetIndex");

        if (bb->Has_Value("SkillIndex"))
            decision.skillIndex = bb->Get_Value<int>("SkillIndex");

        if (bb->Has_Value("ActionType"))
            decision.actionType = static_cast<EBattleContext>(bb->Get_Value<int>("ActionType"));
    }

    ActionFromDecision(request, decision, heroes);
}

bool Monster_BattleBase::Try_Consume_FreeAimWeakPoint()
{
    const Shared<Mon_WeakPoint> weakPoint = Get_BattleWeakPoint();
    return weakPoint != nullptr && weakPoint->Try_Consume_BattleWeakPointHit();
}

void Monster_BattleBase::Hide_FreeAimWeakPoint()
{
    if (const Shared<Mon_WeakPoint> weakPoint = Get_BattleWeakPoint())
        weakPoint->Hide_BattleWeakPoint();
}

void Monster_BattleBase::Reset_FreeAimWeakPoint()
{
    if (const Shared<Mon_WeakPoint> weakPoint = Get_BattleWeakPoint())
        weakPoint->Reset_BattleWeakPoint();
}

float Monster_BattleBase::Get_HpRatio()
{

    if (_combatStat == nullptr) return 1.f;

    const float maxHp = max(_combatStat->Get_StatInfo().maxHp, 1.f);
    return clamp(_combatStat->Get_CurHp() / maxHp, 0.f, 1.f);

}

Monster_BattleBase::BattlePhase Monster_BattleBase::Get_CurPhase()
{
    float hpRatio = Get_HpRatio();

    if (hpRatio <= _combatStat->Get_CriticalRatio())
        return BattlePhase::Critical;
    if (hpRatio <= _combatStat->Get_MidRatio())
        return BattlePhase::Mid;

    return BattlePhase::Opening;
}

float Monster_BattleBase::Score_Skill(const Skill_Data* skill, const SkillChoiceContext& context, const vector<BattleChar>& heroes)
{
    float score = 10.f;
    //
    float repeatPenalty = 0.f;

   for(int i = 0 ;  i < 3; i++)
   {
       if (skill->skillKey != context._prevSkillKey[i])
           continue;

           if (i == 0) repeatPenalty -= 6.f;
           else if (i == 1) repeatPenalty -= 3.f;
           else if (i == 2) repeatPenalty -= 1.f;
   }

    score += repeatPenalty;

    //

    if (skill->skillKey == context.lastSkillIndex)
        score -= 4.f;


    if (skill->skillKey == context.lastSkillIndex)
        score -= 4.f;

    switch(context.phase)
    {
    case BattlePhase::Opening:
        score += skill->openingBias;
        break;
    case BattlePhase::Mid:
        score += skill->midBias;
        break;
    case BattlePhase::Critical:
        score += skill->criticalBias;
        break;
    }

    //
    
    if (context.aliveHeroCount >= 3)
    {
        switch(skill->targetStyle)
        {
        case AITargetStyle::ComboAoE:
        case AITargetStyle::SingleAoE:
            score += 2.f;
            break;

        case AITargetStyle::Single:
            score -= 2.f;
            break;
        }

    }
    else if (context.aliveHeroCount == 1)
    {
        switch (skill->targetStyle)
        {
        case AITargetStyle::ComboAoE:
        case AITargetStyle::SingleAoE:
            score -= 2.f;
            break;

        case AITargetStyle::Single:
            score += 2.f;
            break;
        }
    }

    //






    return score;
}

void Monster_BattleBase::Push_SkillHistory(const int32 skillKey, int32 prevTarget)
{
    context._prevSkillKey[2] = skillKey;
    context._prevSkillKey[1] = context._prevSkillKey[2];
    context._prevSkillKey[0] = context._prevSkillKey[1];

    context._prevTarget = prevTarget;
}

Monster_BattleBase::SkillChoiceContext Monster_BattleBase::Build_SkillChoiceContext(
    const Shared<BlackBoard>& bb,
    const vector<BattleChar>& heroes)
{
    SkillChoiceContext context{};
    context.hpRatio = Get_HpRatio();
    context.phase = Get_CurPhase();

    for (const BattleChar& hero : heroes)
    {
        if (hero.statCom && hero.statCom->Get_AliveStatus() == EAliveStatus::Alive)
            ++context.aliveHeroCount;
    }

    int32 skillIndex = -1;

    if (const any* value = bb->Find_Value("SkillIndex"))
    {
        if (const int32* typedValue = any_cast<int32>(value))
            skillIndex = *typedValue;
    }

    int32 targetIndex = -1;

    if (const any* value = bb->Find_Value("TargetIndex"))
    {
        if (const int32* typedValue = any_cast<int32>(value))
            targetIndex = *typedValue;
    }

    Push_SkillHistory(skillIndex, targetIndex);




    //context.phaseJustChanged = _phaseJustChanged;
    //context.previousPhase = _previousPhase;

    return context;
}

void Monster_BattleBase::Decide_Action(ActionRequest& request, const vector<BattleChar>& heroes)
{
    if (heroes.empty())
        return;

    const BattleDecision decision = {};
    ActionFromDecision(request, decision, heroes);

    LOG_DEBUG(
        "Monster::Decide_Action: target={}, script={}, type={}",
        request.InitTarget.actor ? String::ToString(request.InitTarget.actor->Get_Name()).c_str() : "null",
        request.skillKey.c_str(),
        static_cast<int>(request.type)
    );
}

HRESULT Monster_BattleBase::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_AIController"), _aiCtrl), E_FAIL);
    _aiCtrl->Set_Owner(static_pointer_cast<Monster_BattleBase>(shared_from_this()));

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_MonsterStat"), _combatStat), E_FAIL);

    Bounding_OBB::BoundingOBBDesc obbDesc{};
    obbDesc.center = Vec3(0.f, 0.5f, 0.f);
    obbDesc.extents = Vec3(0.5f, 0.6f, 0.5f);

    CHECK_FAILED(Ready_Collider(L"Com_Collider_OBB", &obbDesc, Collision_Preset::Monster_Body), E_FAIL);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Effect"), _effectCom), E_FAIL);

    return S_OK;
}

void Monster_BattleBase::Free()
{
    Character_BattleBase::Free();
}
