#include "pch.h"
#include "GamePlayMenuFade.h"

#include "ClientInstance.h"

bool GamePlayMenuFade::Request_MenuTransition(
    GameplayMenu From,
    GameplayMenu To,
    const function<void()>& ApplyMenuChange) const
{
    if (!Should_UseFade(From, To))
    {
        if (ApplyMenuChange)
            ApplyMenuChange();
        return true;
    }

    return CLIENT->Request_ScreenFadeTransition(ApplyMenuChange);
}

bool GamePlayMenuFade::Should_UseFade(GameplayMenu From, GameplayMenu To) const
{
    if (From == GameplayMenu::SavePoint &&
        (To == GameplayMenu::SkillTree || To == GameplayMenu::WeaponUpgrade || To == GameplayMenu::CharacterStatus ))
    {
        return true;
    }

    if ((From == GameplayMenu::SkillTree || From == GameplayMenu::WeaponUpgrade || From == GameplayMenu::CharacterStatus) &&
        To == GameplayMenu::SavePoint)
    {
        return true;
    }

    return false;
}
