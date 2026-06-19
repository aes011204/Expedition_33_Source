#include "AN_ReturnBattleIdle.h"

#include "BattleAnimutils.h"
#include "BattleController.h"
#include "ClientInstance.h"
#include "CombatStatCom.h"

IMPLEMENT_REFLECTION(AN_ReturnBattleIdle)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("Blend Time", _blendTime, 0.f, 1.f, 0.01f);

    return true;
}

void AN_ReturnBattleIdle::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview || context.model == nullptr)
        return;

    const auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr || !battle->Is_BattleStarted())
        return;

    const BattleChar* battleChar = nullptr;

    for (const BattleChar& hero : battle->Get_BattleHeroes())
    {
        if (hero.ModelCom.get() == context.model)
        {
            battleChar = &hero;
            break;
        }
    }

    if (battleChar == nullptr)
    {
        for (const BattleChar& enemy : battle->Get_BattleEnemies())
        {
            if (enemy.ModelCom.get() == context.model)
            {
                battleChar = &enemy;
                break;
            }
        }
    }

    if (battleChar == nullptr || battleChar->statCom == nullptr)
        return;

    if (battleChar->statCom->Get_CurAliveState() == EAliveStatus::Dead)
        return;

    BattleAnimUtils::Play_BattleIdle(*battleChar, _blendTime);
}
