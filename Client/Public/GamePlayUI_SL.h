#pragma once

#include "Base.h"
#include "GameplayUI_Types.h"
#include "UI_BattleVictory.h"

NS_BEGIN(Client)

class UI_BossHpBar;
class UI_BattleSystemHUD;
class UI_ReactionResult;
class GamePlayUI_SL : public Base
{
public:
    GamePlayUI_SL() = default;
    ~GamePlayUI_SL() override = default;

public:
    HRESULT Initialize(uint32 levelIndex);
    void    Update(float timeDelta);
    void    Handle_EditorStopRuntime();
    void    Show_Victory();
    void    Show_ParryResult(const Vec3& worldPos);

    vector<RuntimeUIEntry> Get_RuntimeUIEntries() const;

private:
    bool Ensure_BossHpBar();
    void Update_BossHpBar(float timeDelta);
    bool Ensure_BattleSystemHUD();
    void Update_BattleSystemHUD(float timeDelta);
    bool Ensure_BattleVictory();
    void Update_BattleVictory(float timeDelta);
    bool Ensure_ReactionResult();
    void Update_ReactionResult(float timeDelta);
    vector<UI_BattleVictory::VictoryPlayerStatDesc> Build_VictoryPlayerStats() const;
    UI_BattleVictory::VictorySummaryDesc Build_VictorySummary() const;

private:
    uint32 _levelIndex = 0;
    Unique<UI_BossHpBar> _bossHpBar{};
    Unique<UI_BattleSystemHUD> _battleSystemHUD{};
    Unique<UI_BattleVictory> _battleVictory{};
    Unique<UI_ReactionResult> _reactionResult{};
    bool _isVictoryUiActive = false;
    bool _isVictoryConfirmPending = false;

public:
    static Unique<GamePlayUI_SL> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
