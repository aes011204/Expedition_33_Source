#pragma once

#include "Base.h"

NS_BEGIN(Client)

struct BattleMonsterSpawnDesc
{
    string prefabName{};
    Vec3 offset = Vec3::Zero;
    Vec3 rotation = Vec3::Zero;
};

NS_END
