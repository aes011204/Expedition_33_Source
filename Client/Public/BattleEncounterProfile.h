#pragma once

#include "Base.h"
#include "Client_Enum.h"

NS_BEGIN(Client)

namespace BattleEncounterIds
{
    inline constexpr uint32 LancelierTutorial = 99;
}

struct BattleEncounterProfile
{
    uint32 encounterId = 0;
    vector<HeroId> heroIds{}; // 이 조우에서 실제로 전투에 참가할 플레이어 목록
    bool isTutorial = false;
    bool forcePlayerAdvantage = false;
};

const BattleEncounterProfile* Find_BattleEncounterProfile(uint32 encounterId);
vector<HeroId> Resolve_BattleEncounterHeroIds(uint32 encounterId);
bool Is_TutorialBattleEncounter(uint32 encounterId);
bool Should_ForcePlayerAdvantage(uint32 encounterId);

NS_END
