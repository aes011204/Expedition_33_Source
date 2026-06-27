#include "UI_Replay.h"

#include "Client_Constants.h"
#include "GameInstance.h"
#include "UI_Base.h"
#include "UI_Button.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

HRESULT UI_Replay::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close_Immediately(false);

    return S_OK;
}

void UI_Replay::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!_isOpen)
        return;

    Handle_ButtonInput();
}

void UI_Replay::Open()
{
    if (_root == nullptr)
        return;

    if (_isOpen)
        return;

    _lastAction = Action::None;

    UI_RuntimePanelBase::Open();

    GAME->Unlock_Mouse();
    GAME->Set_GameplayInputMode(GameplayInputMode::UIOnly);

    if (_replayButton)
        _replayButton->Suppress_HoverStateAnimationUntilMouseMove();
    if (_gameExitButton)
        _gameExitButton->Suppress_HoverStateAnimationUntilMouseMove();
}

void UI_Replay::Close()
{
    Close_Immediately(true);
}

void UI_Replay::Close_Immediately(bool restoreWorldControl)
{
    _lastAction = Action::None;

    UI_RuntimePanelBase::Close();

    if (restoreWorldControl)
    {
        GAME->Set_GameplayInputMode(GameplayInputMode::Normal);
        GAME->Lock_Mouse();
    }
}

void UI_Replay::Close_ForMenuTransition()
{
    Close_Immediately(false);
}

UI_Replay::Action UI_Replay::Consume_LastAction()
{
    const Action action = _lastAction;
    _lastAction = Action::None;

    return action;
}

HRESULT UI_Replay::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Replay_Path;
    desc.prototypeTagPrefix = L"Replay";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = UI_Utils::Find_UI(_uiByName, L"Canvas_RePlay");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_Replay::Bind_UI()
{
    _replayButton = UI_Utils::Find_TypedUI<UI_Button>(
        _uiByName, L"Button_RePlay", "UI_Replay");
    _gameExitButton = UI_Utils::Find_TypedUI<UI_Button>(
        _uiByName, L"Button_GameExit", "UI_Replay");

    CHECK_NULL(_replayButton, E_FAIL);
    CHECK_NULL(_gameExitButton, E_FAIL);

    return S_OK;
}

void UI_Replay::Handle_ButtonInput()
{
    if (_replayButton && _replayButton->Was_Clicked())
    {
        _lastAction = Action::Replay;
        return;
    }

    if (_gameExitButton && _gameExitButton->Was_Clicked())
    {
        _lastAction = Action::GameExit;
        return;
    }
}

Unique<UI_Replay> UI_Replay::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_Replay>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_Replay");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_Replay::Free()
{
    if (_root)
        Close_Immediately(false);

    _replayButton.reset();
    _gameExitButton.reset();
    _root.reset();
    _uiByName.clear();

    UI_RuntimePanelBase::Free();
}
