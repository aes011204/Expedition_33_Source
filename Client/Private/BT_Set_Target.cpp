#include "BT_Set_Target.h"

#include "AIController.h"
#include "BlackBoard.h"
#include "MonsterStatCom.h"
#include "GameInstance.h"

IMPLEMENT_REFLECTION(BT_Set_Target)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    return true;
}

BT_Set_Target::BT_Set_Target()
{
}

BT_Set_Target::~BT_Set_Target()
{
}

void BT_Set_Target::OnStart()
{
}

BNode::NODE_STATE BT_Set_Target::Logic_Update(float deltaTime)
{


    const Shared<BlackBoard> bb = _AICtrl.lock()->Get_BlackBoard();
    CHECK_NULL(bb, BNode::NODE_STATE::FAILED);

    const any* heroesValue = bb->Find_Value("Heroes");
    if (heroesValue == nullptr)
        return BNode::NODE_STATE::FAILED;

    const auto& heroes = any_cast<const vector<BattleChar>&>(*heroesValue);
    if (heroes.empty())
        return BNode::NODE_STATE::FAILED;

    int32 targetIndex = -1;

    targetIndex = Math::RandNum(0, static_cast<int>(heroes.size())-1);


    //float lowestHp = FLT_MAX;
    //
    //for (int i = 0; i < static_cast<int>(heroes.size()); ++i)
    //{
    //    const auto& hero = heroes[i];
    //    if (hero.statCom == nullptr)
    //        continue;
    //
    //    if (hero.statCom->Get_CurAliveState() != EAliveStatus::Alive)
    //        continue;
    //
    //    const float hp = hero.statCom->Get_CurHp();
    //    if (hp < lowestHp)
    //    {
    //        lowestHp = hp;
    //        targetIndex = i;
    //    }
    //}
    //
    //if (targetIndex < 0)
    //    return BNode::NODE_STATE::FAILED;

    bb->Set_Value("TargetIndex", targetIndex);
    return BNode::NODE_STATE::SUCCESS;
}

void BT_Set_Target::OnFinish()
{
}

Shared<BT_Set_Target> BT_Set_Target::Create()
{
    return make_shared<BT_Set_Target>();
}

void BT_Set_Target::Free()
{
    Task::Free();
}
