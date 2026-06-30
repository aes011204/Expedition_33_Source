#include "BattleEncounterProfile.h"

NS_BEGIN(Client)

namespace
{
    vector<HeroId> Make_DefaultBattleHeroIds()
    {
        return { HeroId::Gustave, HeroId::Lune, HeroId::Maelle };
    }

    const BattleEncounterProfile kBattleEncounterProfiles[] =
    {
        {
            BattleEncounterIds::LancelierTutorial,
            { HeroId::Gustave }, // 창병 튜토리얼은 구스타브 1:1 전투로 진행한다.
            true,       // 튜토리얼인지
            true // 플레이어 선 턴인지
        },
        {
            BattleEncounterIds::PotatoBagGradientTutorial,
            {},          // 파티 구성은 기본 전투 구성을 그대로 사용한다.
            false,       // Lancelier 튜토리얼 시퀀스와 섞지 않고 Gradient 카드만 별도 허용한다.
            false
        },

        // 나중에 필드별로 플레이어 스폰 구성이 달라지면 여기에 추가
    };
}

const BattleEncounterProfile* Find_BattleEncounterProfile(uint32 encounterId)
{
    for (const BattleEncounterProfile& profile : kBattleEncounterProfiles)
    {
        if (profile.encounterId == encounterId)
            return &profile;
    }

    return nullptr;
}

vector<HeroId> Resolve_BattleEncounterHeroIds(uint32 encounterId)
{
    const BattleEncounterProfile* profile = Find_BattleEncounterProfile(encounterId);
    if (profile != nullptr && !profile->heroIds.empty())
        return profile->heroIds; // 프로필이 지정한 조우 전용 파티 구성

    return Make_DefaultBattleHeroIds();
}

bool Is_TutorialBattleEncounter(uint32 encounterId)
{
    const BattleEncounterProfile* profile = Find_BattleEncounterProfile(encounterId);

    return profile != nullptr && profile->isTutorial;
}

bool Should_ForcePlayerAdvantage(uint32 encounterId)
{
    const BattleEncounterProfile* profile = Find_BattleEncounterProfile(encounterId);

    return profile != nullptr && profile->forcePlayerAdvantage;
}

NS_END
