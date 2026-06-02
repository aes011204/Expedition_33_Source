#include "MainTitleMenuSound.h"

#include "GameInstance.h"
#include "UI_Button.h"

void MainTitleMenuSound::Play_RandomHoverSound()
{
    const float randomValue = GAME->Random(0.f, static_cast<float>(HoverSounds.size()));
    const size_t index = min(static_cast<size_t>(randomValue), HoverSounds.size() - 1);

    GAME->Play_Sound(HoverSounds[index], SoundChannel::UI, 1.f);
}

void MainTitleMenuSound::Update_ButtonHoverSound(const Shared<UI_Button>& button, bool& wasHovered)
{
    const bool isHovered = button && button->Is_Hovered();

    if (!wasHovered && isHovered)
        Play_RandomHoverSound();

    wasHovered = isHovered;
}
