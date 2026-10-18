#include "MainTitleMenuSound.h"

#include "Client_Constants.h"
#include "GameInstance.h"
#include "UI_Button.h"

void MainTitleMenuSound::Play_HoverSound()
{
    GAME->Play_SoundCue(Constants::SoundCue::UI_MainTitle_ButtonHover);
}

void MainTitleMenuSound::Update_ButtonHoverSound(const Shared<UI_Button>& button, bool& wasHovered)
{
    const bool isHovered = button && button->Is_Hovered();

    if (!wasHovered && isHovered)
        Play_HoverSound();

    wasHovered = isHovered;
}
