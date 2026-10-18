#include "BT_HasDeadHero.h"

#include "AIController.h"
#include "BlackBoard.h"
#include "CombatStatCom.h"
#include "GameObject.h"
#include "Monster_BattleBase.h"

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

    const Shared<GameObject> ownerObject = owner->Get_Owner();
    if (ownerObject == nullptr)
        return false;

    const Shared<Monster_BattleBase> monster = dynamic_pointer_cast<Monster_BattleBase>(ownerObject);
    if (monster == nullptr)
        return false;

    if (monster->Has_UsedTurnLimitedSpecialThisTurn())
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

        if (hero.statCom->Get_AliveStatus() != EAliveStatus::Alive)
            continue;

        const float maxHp = max(hero.statCom->Get_StatInfo().maxHp, 1.f);
        const float hpRatio = clamp(hero.statCom->Get_CurHp() / maxHp, 0.f, 1.f);
        if (hpRatio > 0.5f)
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
