#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Button;
NS_END

NS_BEGIN(Client)

class UI_ArenaCharacterSelect : public UI_RuntimePanelBase
{
public:
    enum class Action
    {
        None,
        SelectGustave,
        SelectLune,
        SelectMaelle,
        Back
    };

public:
    UI_ArenaCharacterSelect() = default;
    ~UI_ArenaCharacterSelect() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

public:
    void Open() override;
    void Close() override;
    void Close_Immediately(bool restoreWorldControl = true);
    void Close_ForMenuTransition();

    Action Consume_LastAction();

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();
    void    Handle_ButtonInput();

private:
    Action _lastAction = Action::None;

    Shared<UI_Button> _gustaveButton{};
    Shared<UI_Button> _luneButton{};
    Shared<UI_Button> _maelleButton{};
    Shared<UI_Button> _backButton{};

public:
    static Unique<UI_ArenaCharacterSelect> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
