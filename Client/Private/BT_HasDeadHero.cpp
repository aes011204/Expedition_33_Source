#include "BT_HasDeadHero.h"

#include "AIController.h"
#include "BlackBoard.h"
#include "MonsterStatCom.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(BT_HasDeadHero)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    return true;
}

BT_HasDeadHero::BT_HasDeadHero()
{
}

bool BT_HasDeadHero::Check_Condition()
{
    const Shared<AIController> owner = Get_Owner();
    if (owner == nullptr)
        return false;

    const Shared<BlackBoard> bb = owner->Get_BlackBoard();
    if (bb == nullptr)
        return false;

    const any* heroesValue = bb->Find_Value("DeadCheckHeroes");
    if (heroesValue == nullptr)
        heroesValue = bb->Find_Value("Heroes");

    if (heroesValue == nullptr)
        return false;

    const auto& heroes = any_cast<const vector<BattleChar>&>(*heroesValue);
    if (heroes.empty())
        return false;

    for (const BattleChar& hero : heroes)
    {
        if (hero.statCom == nullptr)
            continue;

        if (hero.statCom->Is_ReviveBlocked())
            continue;

        if (hero.statCom->Get_AliveStatus() == EAliveStatus::Dead)
            return true;
    }

    return false;
}

Shared<BT_HasDeadHero> BT_HasDeadHero::Create()
{
    return make_shared<BT_HasDeadHero>();
}

void BT_HasDeadHero::Free()
{
    Decorator::Free();
}

NS_END
