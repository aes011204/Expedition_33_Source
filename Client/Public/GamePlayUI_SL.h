#pragma once

#include "Base.h"
#include "GameplayUI_Types.h"
#include "UI_BattleVictory.h"

NS_BEGIN(Client)

class UI_BossHpBar;
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

    vector<RuntimeUIEntry> Get_RuntimeUIEntries() const;

private:
    bool Ensure_BossHpBar();
    void Update_BossHpBar(float timeDelta);
    bool Ensure_BattleVictory();
    void Update_BattleVictory(float timeDelta);
    vector<UI_BattleVictory::VictoryPlayerStatDesc> Build_VictoryPlayerStats() const;
    UI_BattleVictory::VictorySummaryDesc Build_VictorySummary() const;

private:
    uint32 _levelIndex = 0;
    Unique<UI_BossHpBar> _bossHpBar{};
    Unique<UI_BattleVictory> _battleVictory{};
    bool _isVictoryUiActive = false;
    bool _isVictoryConfirmPending = false;

public:
    static Unique<GamePlayUI_SL> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
