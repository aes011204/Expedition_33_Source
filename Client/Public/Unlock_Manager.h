#pragma once

#include "Base.h"

NS_BEGIN(Client)

class Unlock_Manager : public Base
{
public:
    enum class TriggerType
    {
        EnemyKilled,
    };

    enum class RewardType
    {
        UnlockCostumePreset,
        UnlockSkill,
        SpawnWorldObject,
    };

    struct UnlockReward
    {
        RewardType type = RewardType::UnlockCostumePreset;

        // 코스튬 해금용
        HeroId heroId = HeroId::END;
        string presetId{};

        // 스킬 해금용 - 그래디언트
        string skillId{};

        // 월드 오브젝트 스폰용. 이거 필요할지?
        // LevelType target

    };
};

NS_END
