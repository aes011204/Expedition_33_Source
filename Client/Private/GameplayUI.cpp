#include "GameplayUI.h"

#include "ClientInstance.h"
#include "ClientScreenFxManager.h"
#include "GameplayUI_Battle.h"
#include "GameplayUI_World.h"

GameplayUI::~GameplayUI() = default;

HRESULT GameplayUI::Initialize(uint32 levelIndex)
{
    _worldUI = GameplayUI_World::Create(levelIndex);   // 월드 메뉴/입력 전용 컨트롤러
    CHECK_NULL(_worldUI, E_FAIL);

    _battleUI = GameplayUI_Battle::Create(levelIndex); // battle delegate/HUD/transition 전용 컨트롤러
    CHECK_NULL(_battleUI, E_FAIL);

    _lastInputMode = Resolve_InputMode();

    return S_OK;
}

void GameplayUI::Update(float timeDelta)
{
    if (_worldUI)
        _worldUI->Update(timeDelta);

    if (_battleUI)
        _battleUI->Update(timeDelta);

    const GameplayUIInputMode currentInputMode = Resolve_InputMode();
    if (currentInputMode != _lastInputMode)
    {
        // 월드 -> 배틀 진입 시 월드 메뉴를 경계에서 정리
        Handle_InputModeChanged(_lastInputMode, currentInputMode); 
        _lastInputMode = currentInputMode;
    }

    switch (currentInputMode)
    {
    case GameplayUIInputMode::World:
        if (_worldUI)
            _worldUI->Update_Input();
        break;

    case GameplayUIInputMode::Battle:
        if (_battleUI)
            _battleUI->Update_Input();
        break;

    default:
        break;
    }
}

void GameplayUI::Play_BattleEnterTransition()
{
    if (_battleUI)
        _battleUI->Play_BattleEnterTransition();
}

void GameplayUI::Stop_BattleEnterTransition()
{
    if (_battleUI)
        _battleUI->Stop_BattleEnterTransition();
}

bool GameplayUI::Is_BattleEnterTransitionPlaying() const
{
    return _battleUI && _battleUI->Is_BattleEnterTransitionPlaying();
}

bool GameplayUI::Is_AnyMenuOpen() const
{
    return _worldUI && _worldUI->Is_AnyMenuOpen();
}

GameplayMenu GameplayUI::Get_CurrentMenu() const
{
    if (_worldUI == nullptr)
        return GameplayMenu::None;

    return _worldUI->Get_CurrentMenu();
}

GameplayUI::GameplayUIInputMode GameplayUI::Resolve_InputMode() const
{
    if (CLIENT->Is_BattleEntering() || CLIENT->Is_BattleActive())
        return GameplayUIInputMode::Battle;

    return GameplayUIInputMode::World;
}

void GameplayUI::Handle_InputModeChanged(GameplayUIInputMode previousMode, GameplayUIInputMode currentMode)
{
    if (previousMode == GameplayUIInputMode::World && currentMode == GameplayUIInputMode::Battle)
    {
        if (_worldUI)
            _worldUI->Force_CloseAllMenus();
    }
}

Shared<GameplayUI> GameplayUI::Create(uint32 levelIndex)
{
    auto instance = make_shared<GameplayUI>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : GameplayUI");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void GameplayUI::Free()
{
    auto Release_UI = [](auto& ui)
        {
            if (ui)
            {
                ui->Free();
                ui.reset();
            }
        };

    Release_UI(_worldUI);
    Release_UI(_battleUI);

    Base::Free();
}
