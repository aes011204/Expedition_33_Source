#pragma once

#include "Base.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

class CombatStatCom;

class PictosBattleSystem : public Base
{
public:
    PictosBattleSystem() = default;
    ~PictosBattleSystem() override = default;

public:
    HRESULT Initialize();

    void Clear_RuntimeState();

    // 활력의 턴
    uint32 Resolve_TurnStartApBonus(const BattleChar& actor) const;
    void ProcessTurnStart(const BattleChar& actor);

    float Resolve_DamageMultiplier(
        const BattleChar& attacker,
        const BattleChar& target,
        AttackType attackType) const;

private:
    bool Has_EquippedPictosId(const Shared<CombatStatCom>& statCom, const string& pictosId) const;
    bool Is_PictosBaseDamageType(AttackType attackType) const;

private:
    umap<CombatStatCom*, float> _warmingUpDamageBonusByStat{};     // WarmingUp 누적 피해 증가율(%)
    umap<CombatStatCom*, float> _rouletteDamageMultiplierByStat{}; // Roulette 이번 턴 피해 배율

public:
    static Shared<PictosBattleSystem> Create();
    void Free() override;

};

NS_END
