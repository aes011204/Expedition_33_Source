#pragma once
#pragma once

#include "Battle_Enum.h"

NS_BEGIN(Client)

namespace BattleAnimUtils
{
    // 현재 actor가 idle로 복귀할 때 어떤 battle anim state를 써야 하는지 결정
    BattleAnimType Resolve_BattleIdleAnimType(const BattleChar& actor);

    // 위의 값으로 실제 Idle 재생. 이거 마엘 태세때문에 만듦
    void Play_BattleIdle(const BattleChar& actor, float blendTime);
}

NS_END
