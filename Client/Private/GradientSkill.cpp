#include "GradientSkill.h"

const char* GradientSkill::Resolve_SkillKey(HeroId heroId)
{
    switch (heroId)
    {
    case HeroId::Gustave:
        return "AngelsEye";
    case HeroId::Lune:
        return "SkyBreak";
    case HeroId::Maelle:
        return "Stendhal";
    case HeroId::END:
    default:
        return "";
    }
}

bool GradientSkill::Is_GradientSkill(HeroId heroId, const string& skillKey)
{
    const char* gradientSkillKey = Resolve_SkillKey(heroId);

    return gradientSkillKey[0] != '\0' && skillKey == gradientSkillKey;
}
