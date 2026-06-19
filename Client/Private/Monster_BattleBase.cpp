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
#include "DamageSystem.h"

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

    if (_combatStat)
        _combatStat->Rebuild_SkillRuntimeState();
    
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

    Advance_SkillRuntimeTurn();

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

    if (decision.skillIndex >= 0 && decision.targetIndex >= 0 && _combatStat)
    {
        SkillChoiceContext nextContext = _cachedContext;
        const vector<string>& skillKeys = _combatStat->Get_SkillKey();

        if (decision.skillIndex < static_cast<int32>(skillKeys.size()))
        {
            const string& skillKey = skillKeys[decision.skillIndex];
            Push_SkillHistory(skillKey);
            nextContext.lastSkillIndex = skillKey;
            nextContext.lastTargetIndex = decision.targetIndex;
            Notify_SkillExecutionSucceeded(skillKey);
        }

        nextContext.previousPhase = Get_CurPhase();
        _cachedContext = nextContext;
    }

    ActionFromDecision(request, decision, heroes);

    if (!request.skillKey.empty())
    {
        LOG_INFO(
            "[MonsterAI] selected skill. monster={}, skillKey={}, skillIndex={}, targetIndex={}, actionType={}",
            Get_SourcePrefabName().c_str(),
            request.skillKey.c_str(),
            decision.skillIndex,
            decision.targetIndex,
            magic_enum::enum_name(request.type));
    }
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

float Monster_BattleBase::Get_HpRatio() const
{

    if (_combatStat == nullptr) return 1.f;

    const float maxHp = max(_combatStat->Get_StatInfo().maxHp, 1.f);
    return clamp(_combatStat->Get_CurHp() / maxHp, 0.f, 1.f);

}

Monster_BattleBase::BattlePhase Monster_BattleBase::Get_CurPhase() const
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
           else if (i == 1) repeatPenalty -= 4.f;
           else if (i == 2) repeatPenalty -= 2.f;
   }

    score += repeatPenalty;


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
            score += 1.f;
            break;

        //case AITargetStyle::Single:
        //    score -= 1.f;
            break;
        }

    }
    else if (context.aliveHeroCount == 1)
    {
        switch (skill->targetStyle)
        {
        //case AITargetStyle::ComboAoE:
        //case AITargetStyle::SingleAoE:
        //    score -= 1.f;
            break;

        case AITargetStyle::Single:
            score += 1.f;
            break;
        }
    }

    //
    return score;
}

void Monster_BattleBase::Push_SkillHistory(const string& skillKey)
{
    _cachedContext._prevSkillKey[2] = _cachedContext._prevSkillKey[1];
    _cachedContext._prevSkillKey[1] = _cachedContext._prevSkillKey[0];
    _cachedContext._prevSkillKey[0] = skillKey;

  
}
void Monster_BattleBase::Push_TargetHistory(const int32 prevTarget)
{
    _cachedContext._prevTarget[2] = _cachedContext._prevTarget[1];
    _cachedContext._prevTarget[1] = _cachedContext._prevTarget[0];
    _cachedContext._prevTarget[0] = prevTarget;
}

void Monster_BattleBase::Advance_SkillRuntimeTurn()
{
    if (_combatStat == nullptr)
        return;

    _combatStat->Advance_SkillCooldownTurn();
}

void Monster_BattleBase::Notify_SkillExecutionSucceeded(const string& skillKey)
{
    if (_combatStat == nullptr || skillKey.empty())
        return;

    const Skill_Data* skill = CLIENT->Get_Skill(skillKey);
    if (skill == nullptr)
        return;

    _combatStat->Notify_SkillUsed(*skill);
}

Monster_BattleBase::SkillChoiceContext Monster_BattleBase::Build_SkillChoiceContext(
    const Shared<BlackBoard>& bb,
    const vector<BattleChar>& heroes)
{
    SkillChoiceContext context{};
    context.hpRatio = Get_HpRatio();
    context.phase = Get_CurPhase();
    context.previousPhase = _cachedContext.previousPhase;
    context.lastSkillIndex = _cachedContext._prevSkillKey[0];
    context.lastTargetIndex = _cachedContext._prevTarget[0];

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

    BattlePhase curPhase = Get_CurPhase();

    context.phaseJustChanged = false;
    if(context.previousPhase != curPhase)
    {
        context.phaseJustChanged = true;
    }

    return context;
}


float Monster_BattleBase::Score_Target(
    const Skill_Data* skill,
    const BattleChar& target,
    int32 targetIndex
    )
{
    UNREFERENCED_PARAMETER(skill);

    if (skill == nullptr || target.statCom == nullptr)
        return -100000.f;

    if (target.statCom->Get_AliveStatus() != EAliveStatus::Alive)
        return -100000.f;

    float score = 0.f;

    const float maxHp = max(target.statCom->Get_StatInfo().maxHp, 1.f);
    const float hpRatio = clamp(target.statCom->Get_CurHp() / maxHp, 0.f, 1.f);

    // setSkill에서 context 다 만들어놔서 캐싱된 것 이용
    for (int i = 0; i < 3; ++i)
    {
        if (targetIndex != _cachedContext._prevTarget[i])
            continue;

        if (i == 0) score -= 4.f;
        else if (i == 1) score -= 2.f;
        else if (i == 2) score -= 1.f;
    }

    //switch (skill->targetStyle)
    //{
    //case AITargetStyle::Single:
    //    score += (1.f - hpRatio) * 8.f;
    //    break;

    //case AITargetStyle::SingleAoE:
    //case AITargetStyle::ComboAoE:
    //    score += (1.f - hpRatio) * 4.f;
    //    break;

    //case AITargetStyle::Mix:
    //    score += (1.f - hpRatio) * 5.f;
    //    score += static_cast<float>(target.statCom->Get_CurAP()) * 0.5f;
    //    break;

    //case AITargetStyle::RandomEnemy:
    //    score += Math::RandNum(0.f, 2.f);
    //    break;

    //default:
    //    score += (1.f - hpRatio) * 3.f;
    //    break;
    //}

    const float estimatedDamage = Estimate_SkillDamage(skill, target);
    const float effectiveHp =
        target.statCom->Get_CurHp() +
        static_cast<float>(target.statCom->Get_CurrentShield());

    if (estimatedDamage >= effectiveHp)
        score += 5.f;

    return score;
}
float Monster_BattleBase::Estimate_SkillDamage(const Skill_Data* skill, const BattleChar& target)
{
    if (_combatStat == nullptr || skill == nullptr || target.statCom == nullptr)
        return 0.f;

    
    BattleChar attacker{};
    attacker.statCom = _combatStat;

    const int32 hitCount = max(1, static_cast<int32>(skill->targetingType.size()));
    const float oneHitDamage =
        DamageSystem::CalculatePlayerAttackDamage(attacker, skill->powerMultiplier);

    const float totalExpectedDamage = oneHitDamage * static_cast<float>(hitCount);
    return max(totalExpectedDamage, 1.f);
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
