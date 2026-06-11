#pragma once

#include "Base.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

// 전투 기록 모아놓을 클래스. 최고 데미지, 전투 시간, 회피, 쳐내기 수 등등
// 여기서 기록하고 UI_BattleVictory에서 꺼내서 사용
class BattleResultRecoder : public Base
{
public:
    struct VictoryStats
    {
        float elapsedSec = 0.f;
        uint32 maxDamage = 0;
        uint32 totalDamageDealt = 0;
        uint32 totalDamageTaken = 0;
        uint32 parrySuccessCount = 0;
        uint32 dodgeSuccessCount = 0;
    };

public:
    BattleResultRecoder() = default;
    ~BattleResultRecoder() override = default;

public:
    void Reset_ForBattle();
    void Update(float unscaledDletaTime);
    void Record_DamageApplied(const DamageAppliedInfo& info);
    void Record_ReactionSucceeded(BattleReactionType reactionType);

    const VictoryStats& Get_VictoryStats() const { return _victoryStats; }

private:
    VictoryStats _victoryStats{};

public:
    static Shared<BattleResultRecoder> Create();
    void Free() override;

};

NS_END
