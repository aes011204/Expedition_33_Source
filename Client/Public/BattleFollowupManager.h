#pragma once

#include "Base.h"
#include "BattleMonsterSpawnDesc.h"

NS_BEGIN(Client)

struct BattleFollowupDesc
{
    uint32 battleFieldId = 0;
    bool isPlayerAdvantage = false;
    vector<HeroId> heroIds{};
    vector<BattleMonsterSpawnDesc> monsterSpawnDescs{};
    Shared<GameObject> sourceWorldObject{};
};

class BattleFollowupManager final : public Base
{
public:
    HRESULT Initialize();

    void Handle_BattleVictory(
        const vector<string>& defeatedEnemyKeys,
        const vector<HeroId>& heroIds,
        uint32 battleFieldId,
        const Shared<GameObject>& sourceWorldObject);

    bool Consume_PendingFollowup(BattleFollowupDesc& outDesc);
    bool Has_PendingFollowup() const { return _hasPendingFollowup; }
    void Clear_PendingFollowup();

private:
    bool Should_StartSimonPhase2(
        const vector<string>& defeatedEnemyKeys,
        const Shared<GameObject>& sourceWorldObject) const;

    void Queue_SimonPhase2(
        const vector<HeroId>& heroIds,
        uint32 battleFieldId,
        const Shared<GameObject>& sourceWorldObject);

private:
    bool _hasPendingFollowup = false;
    BattleFollowupDesc _pendingFollowup{};

public:
    static Unique<BattleFollowupManager> Create();
    void Free() override;
};

NS_END
