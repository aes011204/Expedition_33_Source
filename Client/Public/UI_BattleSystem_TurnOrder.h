#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Image;
NS_END

NS_BEGIN(Client)

class UI_BattleSystem_TurnOrder final : public UI_RuntimePanelBase
{
public:
    struct TurnOrderEntryDesc
    {
        string  portraitGuid{};
        bool    isEnemy = false;
    };

    struct TurnOrderSlot
    {
        Shared<UI_Base> root{};
        Shared<UI_Image> portrait{};
    };

public:
    UI_BattleSystem_TurnOrder() = default;
    ~UI_BattleSystem_TurnOrder() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;

public:
    void    Set_TurnOrderPreview(const vector<TurnOrderEntryDesc>& entries);
    void    Clear_TurnOrderPreview();

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

private:
    // 턴 슬롯은 최대 8개
    array<TurnOrderSlot, 8> _slots {};
    Shared<UI_Image>        _currentTurnArrow{};

public:
    static Unique<UI_BattleSystem_TurnOrder> Create(uint32 levelIndex);
    void Free() override;

};

NS_END
