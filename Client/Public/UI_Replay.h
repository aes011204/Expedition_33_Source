#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Button;
NS_END

NS_BEGIN(Client)

class UI_Replay : public UI_RuntimePanelBase
{
public:
    enum class Action
    {
        None,
        Replay,
        GameExit
    };

public:
    UI_Replay() = default;
    ~UI_Replay() override = default;

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

    Shared<UI_Button> _replayButton{};
    Shared<UI_Button> _gameExitButton{};

public:
    static Unique<UI_Replay> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
