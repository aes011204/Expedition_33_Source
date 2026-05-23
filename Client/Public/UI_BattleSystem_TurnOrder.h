#pragma once

#include "UI_RuntimePanelBase.h"

class UI_BattleSystem_TurnOrder final : public UI_RuntimePanelBase
{
public:
    UI_BattleSystem_TurnOrder();
    ~UI_BattleSystem_TurnOrder() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

public:
    static Unique<UI_BattleSystem_TurnOrder> Create(uint32 levelIndex);
    void Free() override;

};

