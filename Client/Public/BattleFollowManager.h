#pragma once

#include "Base.h"

NS_BEGIN(Client)

// BattleController가 Victory 시점에 만든 전투 결과
// 시몽 1페 -> Battle Victory -> 2페
struct BattleVictoryFollowupContext
{
    vector<string> defeatedEnemyKeys{};
    vector<HeroId> heroIds{};
    uint32 battleFieldId = 0;
    Shared<GameObject> sourceWorldObject{};
};

class BattleFollowManager : public Base
{

};

NS_END
