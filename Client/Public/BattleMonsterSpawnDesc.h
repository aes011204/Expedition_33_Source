#pragma once

#include "Base.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

// MW, TW -> MB로 전달할 데이터
struct BattleMonsterStatSnapshot
{
    bool valid = false; // MW/TW에서 실제로 넘긴 전투 스탯이 있을 때만 MB에 적용

    float maxHp = 0.f;
    uint32 maxAp = 0;
    float physicalAttack = 0.f;
    float physicalDefense = 0.f;
    float speed = 0.f;
    float battleRunMoveSpeed = 0.f;
    float maxBreak = 0.f;
    float breakBarStunDuration = 0.f;

    uint32 shieldPointsAtSpawn = 0;
    uint32 exp = 0;
    uint32 chroma = 0;
    EEnemyClass enemyClass = EEnemyClass::Normal;

    wstring battleAnnounceName = L"";
};

struct BattleMonsterSpawnDesc
{
    string prefabName{};
    Vec3 offset = Vec3::Zero;
    Vec3 rotation = Vec3::Zero;
    float wideBackOffset = 0.f;
    float wideHeightOffset = 0.f;
    float wideLookHeightOffset = 0.f;

    BattleMonsterStatSnapshot statSnapshot{};
};

NS_END
