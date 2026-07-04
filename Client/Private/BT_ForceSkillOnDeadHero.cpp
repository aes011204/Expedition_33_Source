#include "BT_ForceSkillOnDeadHero.h"

#include "AIController.h"
#include "BlackBoard.h"
#include "ClientInstance.h"
#include "GameObject.h"
#include "Monster_BattleBase.h"
#include "MonsterStatCom.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(BT_ForceSkillOnDeadHero)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_STRING("Forced Skill Key", _forcedSkillKey);

    return true;
}

BT_ForceSkillOnDeadHero::BT_ForceSkillOnDeadHero()
{
}

void BT_ForceSkillOnDeadHero::OnStart()
{
}

BNode::NODE_STATE BT_ForceSkillOnDeadHero::Logic_Update(float deltaTime)
{
    UNREFERENCED_PARAMETER(deltaTime);

    if (_forcedSkillKey.empty())
        return BNode::NODE_STATE::FAILED;

    const Shared<AIController> aiCtrl = _AICtrl.lock();
    CHECK_NULL(aiCtrl, BNode::NODE_STATE::FAILED);

    const Shared<BlackBoard> bb = aiCtrl->Get_BlackBoard();
    CHECK_NULL(bb, BNode::NODE_STATE::FAILED);

    const Shared<GameObject> owner = _owner.lock();
    CHECK_NULL(owner, BNode::NODE_STATE::FAILED);

    const Shared<Monster_BattleBase> monster = dynamic_pointer_cast<Monster_BattleBase>(owner);
    CHECK_NULL(monster, BNode::NODE_STATE::FAILED);

    const Shared<MonsterStatCom> stat = owner->Get_Component<MonsterStatCom>();
    CHECK_NULL(stat, BNode::NODE_STATE::FAILED);

    const vector<string>& skillKeys = stat->Get_SkillKeys();
    for (int32 i = 0; i < static_cast<int32>(skillKeys.size()); ++i)
    {
        if (skillKeys[i] != _forcedSkillKey)
            continue;

        const Skill_Data* skill = CLIENT->Get_Skill(skillKeys[i]);
        if (skill == nullptr)
            return BNode::NODE_STATE::FAILED;

        if (!stat->Can_UseSkill(*skill))
            return BNode::NODE_STATE::FAILED;

        bb->Set_Value("SkillIndex", i);
        bb->Set_Value("ActionType", static_cast<int>(EBattleContext::Skill));
        monster->Mark_TurnLimitedSpecialUsed();
        return BNode::NODE_STATE::SUCCESS;
    }

    return BNode::NODE_STATE::FAILED;
}

void BT_ForceSkillOnDeadHero::OnFinish()
{
}

Shared<BT_ForceSkillOnDeadHero> BT_ForceSkillOnDeadHero::Create()
{
    return make_shared<BT_ForceSkillOnDeadHero>();
}

void BT_ForceSkillOnDeadHero::Free()
{
    Task::Free();
}

NS_END
