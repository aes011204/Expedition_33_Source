#include "BT_SetDeadHeroTarget.h"

#include "AIController.h"
#include "Battle_Enum.h"
#include "BlackBoard.h"
#include "CombatStatCom.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(BT_SetDeadHeroTarget)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    return true;
}

BT_SetDeadHeroTarget::BT_SetDeadHeroTarget()
{
}

void BT_SetDeadHeroTarget::OnStart()
{
}

BNode::NODE_STATE BT_SetDeadHeroTarget::Logic_Update(float deltaTime)
{
    UNREFERENCED_PARAMETER(deltaTime);

    const Shared<AIController> aiCtrl = _AICtrl.lock();
    CHECK_NULL(aiCtrl, BNode::NODE_STATE::FAILED);

    const Shared<BlackBoard> bb = aiCtrl->Get_BlackBoard();
    CHECK_NULL(bb, BNode::NODE_STATE::FAILED);

    const any* heroesValue = bb->Find_Value("DeadCheckHeroes");
    if (heroesValue == nullptr)
        heroesValue = bb->Find_Value("Heroes");

    if (heroesValue == nullptr)
        return BNode::NODE_STATE::FAILED;

    const auto& heroes = any_cast<const vector<BattleChar>&>(*heroesValue);
    if (heroes.empty())
        return BNode::NODE_STATE::FAILED;

    for (int32 i = 0; i < static_cast<int32>(heroes.size()); ++i)
    {
        const BattleChar& hero = heroes[i];
        if (hero.statCom == nullptr)
            continue;

        if (hero.statCom->Get_AliveStatus() != EAliveStatus::Alive)
            continue;

        const float maxHp = max(hero.statCom->Get_StatInfo().maxHp, 1.f);
        const float hpRatio = clamp(hero.statCom->Get_CurHp() / maxHp, 0.f, 1.f);
        if (hpRatio <= 0.5f)
            continue;

        bb->Set_Value("TargetIndex", i);
        bb->Set_Value("UseDeadCheckHeroesForTarget", true);
        return BNode::NODE_STATE::SUCCESS;
    }

    return BNode::NODE_STATE::FAILED;
}

void BT_SetDeadHeroTarget::OnFinish()
{
}

Shared<BT_SetDeadHeroTarget> BT_SetDeadHeroTarget::Create()
{
    return make_shared<BT_SetDeadHeroTarget>();
}

void BT_SetDeadHeroTarget::Free()
{
    Task::Free();
}

NS_END
