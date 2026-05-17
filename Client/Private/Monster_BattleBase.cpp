#include "Monster_BattleBase.h"

#include "AIController.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "GameInstance.h"
#include "MonsterStatCom.h"

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

void Monster_BattleBase::Update(float timeDelta)
{
    if (_combatStat)
        _combatStat->Update(timeDelta);

    Character_BattleBase::Update(timeDelta);
}

void Monster_BattleBase::Late_Update(float timeDelta)
{
    Character_BattleBase::Late_Update(timeDelta);
}

void Monster_BattleBase::Decide_Action(Action& curTurnAction, const vector<BattleChar>& heroes)
{
    if (heroes.empty())
        return;

    Skill_Data skill = _combatStat->Get_Basic_Skill();

    curTurnAction.target = heroes[0];
    curTurnAction._selected_SkillKey = skill;
    curTurnAction.type = EBattleContext::BaseAttack;

    LOG_DEBUG("Monster_BattleBase::Decide_Action: target={}, script={}, type={}",
        curTurnAction.target.actor ? String::ToString(curTurnAction.target.actor->Get_Name()).c_str() : "null",
        curTurnAction._selected_SkillKey.scriptClassName.c_str(),
        static_cast<int>(curTurnAction.type));
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

    return S_OK;
}

void Monster_BattleBase::Free()
{
    Character_BattleBase::Free();
}
