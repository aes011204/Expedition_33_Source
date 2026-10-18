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
    static void Play_HoverSound();
};

NS_END
