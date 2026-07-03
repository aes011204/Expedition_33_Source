#pragma once

#include "Base.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

class CombatStatCom;

class PictosBattleSystem : public Base
{
public:
    struct PictosActivationEvent
    {
        HeroId heroId = HeroId::END;
        string pictosId{};
        wstring displayName{};
        uint32 activationCount = 1;
    };

public:
    PictosBattleSystem() = default;
    ~PictosBattleSystem() override = default;

public:
    HRESULT Initialize();

    void Clear_RuntimeState();

    // 활력의 턴
    uint32 Resolve_TurnStartApBonus(const BattleChar& actor) const;
    void ProcessTurnStart(const BattleChar& actor);

    vector<PictosActivationEvent> Consume_PendingActivations();

    float Resolve_DamageMultiplier(
        const BattleChar& attacker,
        const BattleChar& target,
        AttackType attackType);

private:
    bool Has_EquippedPictosId(const Shared<CombatStatCom>& statCom, const string& pictosId) const;
    bool Is_PictosBaseDamageType(AttackType attackType) const;

    void Queue_Activation(const BattleChar& actor, const string& pictosId);
    wstring Resolve_PictosDisplayName(const string& pictosId) const;

private:
    umap<CombatStatCom*, float> _warmingUpDamageBonusByStat{};     // WarmingUp 누적 피해 증가율(%)
    umap<CombatStatCom*, float> _rouletteDamageMultiplierByStat{}; // Roulette 이번 턴 피해 배율

    vector<PictosActivationEvent> _pendingActivations{};

public:
    static Shared<PictosBattleSystem> Create();
    void Free() override;

};

NS_END
