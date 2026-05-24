#include "BT_SetSkill.h"

#include "AIController.h"
#include "BlackBoard.h"
#include "Monster_BattleBase.h"
#include "MonsterStatCom.h"

IMPLEMENT_REFLECTION(BT_SetSkill)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    return true;
}

BT_SetSkill::BT_SetSkill()
{
}

BT_SetSkill::~BT_SetSkill()
{
}

void BT_SetSkill::OnStart()
{
}

BNode::NODE_STATE BT_SetSkill::Logic_Update(float deltaTime)
{

    const Shared<BlackBoard> bb = _AICtrl.lock()->Get_BlackBoard();

    const Shared<MonsterStatCom> stat = _owner.lock()->Get_Component<MonsterStatCom>();
    CHECK_NULL(stat, BNode::NODE_STATE::FAILED);

    if (stat->Get_SkillKey().empty())
        return BNode::NODE_STATE::FAILED;
    int32 SkillNum = stat->Get_SkillKey().size();

    int32 RandNum = Math::RandNum(0, SkillNum-1);

    bb->Set_Value("SkillIndex", RandNum);
    bb->Set_Value("ActionType", static_cast<int>(EBattleContext::Skill));

    return BNode::NODE_STATE::SUCCESS;
}

void BT_SetSkill::OnFinish()
{
}

Shared<BT_SetSkill> BT_SetSkill::Create()
{
    return make_shared<BT_SetSkill>();
}

void BT_SetSkill::Free()
{
    Task::Free();
}
