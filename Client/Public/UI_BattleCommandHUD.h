#pragma once

#include "Base.h"
#include "Battle_Enum.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Button;
NS_END

NS_BEGIN(Client)

class UI_BattleCommandHUD final : public Base
{
public:
    UI_BattleCommandHUD() = default;
    ~UI_BattleCommandHUD() override = default;

public:
    HRESULT Initialize(uint32 levelIndex);
    void    Update(float timeDelta);

    void    Open();
    void    Close();
    bool    Is_Open() const { return _isOpen; }

    void    Refresh_ForState(BattleTurnState state);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Apply_DefaultHighlight();
    void    Set_ButtonHighlighted(const Shared<UI_Base>& button, bool highlighted);

private:
    uint32 _levelIndex = 0;
    bool   _isOpen = false;

    umap<wstring, Shared<UI_Base>> _uiByName{};

    Shared<UI_Base> _root{};

    Shared<UI_Base> _attackButton{};
    Shared<UI_Base> _skillButton{};
    Shared<UI_Base> _itemButton{};
    Shared<UI_Base> _shotButton{};

    Vec2 _normalScale = Vec2(1.f, 1.f);
    Vec2 _highlightScale = Vec2(1.08f, 1.08f);

public:
    static Unique<UI_BattleCommandHUD> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
