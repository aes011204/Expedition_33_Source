#pragma once

#include "Client_Defines.h"

NS_BEGIN(Engine)
class UI_Button;
NS_END

NS_BEGIN(Client)

// MainTitle, GameEntry에서 사운드 공용 사용 이거 ButtonHoverSound 그냥 범용적으로 쓸 수 있을거같긴한데
class MainTitleMenuSound final
{
public:
    MainTitleMenuSound() = delete;

public:
    static void Update_ButtonHoverSound(const Shared<UI_Button>& button, bool& wasHovered);

private:
    static void Play_RandomHoverSound();

private:
    static constexpr array<const wchar_t*, 9> HoverSounds =
    {
        L"MainTitle/Button_Hover_01.wav",
        L"MainTitle/Button_Hover_02.wav",
        L"MainTitle/Button_Hover_03.wav",
        L"MainTitle/Button_Hover_04.wav",
        L"MainTitle/Button_Hover_05.wav",
        L"MainTitle/Button_Hover_06.wav",
        L"MainTitle/Button_Hover_07.wav",
        L"MainTitle/Button_Hover_08.wav",
        L"MainTitle/Button_Hover_09.wav",
    };
};

NS_END
