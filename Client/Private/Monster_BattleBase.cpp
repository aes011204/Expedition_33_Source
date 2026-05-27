#include "Monster_BattleBase.h"
#include "ClientInstance.h"

#include "AIController.h"
#include "BlackBoard.h"
#include "Body_Player.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "GameInstance.h"
#include "MonsterStatCom.h"
#include "Monster_Part.h"
#include "EffectCom.h"

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

     if (_effectCom)
        _effectCom->Update(timeDelta); // 몬스터 사망 시 디졸브용

    Character_BattleBase::Update(timeDelta);
}

void Monster_BattleBase::Late_Update(float timeDelta)
{
    Character_BattleBase::Late_Update(timeDelta);
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
