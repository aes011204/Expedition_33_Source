#pragma once

#include "Base.h"
#include "GameplayUI_Types.h"

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

    vector<RuntimeUIEntry> Get_RuntimeUIEntries() const;

private:
    bool Ensure_BossHpBar();
    void Update_BossHpBar(float timeDelta);

private:
    uint32 _levelIndex = 0;
    Unique<UI_BossHpBar> _bossHpBar{};

public:
    static Unique<GamePlayUI_SL> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
