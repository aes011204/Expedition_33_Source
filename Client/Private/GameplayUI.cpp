#include "GameplayUI.h"

#include "ClientInstance.h"
#include "ClientScreenFxManager.h"
#include "GameplayUI_Battle.h"
#include "GamePlayUI_SL.h"
#include "GameplayUI_World.h"
#include "IRuntimeUI_EditorTarget.h"
#include "UI_Base.h"
#include "UI_RuntimePanelBase.h"

GameplayUI::~GameplayUI() = default;

HRESULT GameplayUI::Initialize(uint32 levelIndex)
{
    _worldUI = GameplayUI_World::Create(levelIndex);   // 월드 메뉴/입력 전용 컨트롤러
    CHECK_NULL(_worldUI, E_FAIL);

    _battleUI = GameplayUI_Battle::Create(levelIndex); // battle delegate/HUD/transition 전용 컨트롤러
    CHECK_NULL(_battleUI, E_FAIL);

    _soulLikeUI = GamePlayUI_SL::Create(levelIndex);
    CHECK_NULL(_soulLikeUI, E_FAIL);

    if (!_editorStopHandle.IsValid())
    {
        _editorStopHandle = GAME->Get_DelegateHub().OnEditorStopRuntime.Add(
            this,
            &GameplayUI::Handle_EditorStopRuntime);
    }

    _lastInputMode = Resolve_InputMode();
    Invalidate_RuntimeUIEntryCache();

    return S_OK;
}

void GameplayUI::Update(float timeDelta)
{
    ++_runtimeUIEntryFrameSerial;

    if (_isCinematicUIHidden)
    {
        Close_RuntimeUIForCinematic(); // 시네마틱 중 새로 열린 HUD/메뉴도 같은 프레임에 다시 숨긴다.
        return;
    }

    const GameplayUIInputMode currentInputMode = Resolve_InputMode();

    if (currentInputMode != _lastInputMode)
    {
        // 월드 -> 배틀 진입 시 월드 메뉴를 경계에서 정리
        Handle_InputModeChanged(_lastInputMode, currentInputMode); 
        _lastInputMode = currentInputMode;
    }

    if (_worldUI)
        _worldUI->Update(timeDelta);

    if (_battleUI && _battleUI->Needs_RuntimeUpdate())
        _battleUI->Update(timeDelta);

    if (_soulLikeUI)
        _soulLikeUI->Update(timeDelta);

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

    case GameplayUIInputMode::SoulLike:
        // SL 키 입력은 SoulLikeController::Update_Player()에서 처리한다.
        break;

    default:
        break;
    }
}

vector<RuntimeUIEntry> GameplayUI::Get_WorldRuntimeUIEntries() const
{
    return Get_CachedWorldRuntimeUIEntries();
}

vector<RuntimeUIEntry> GameplayUI::Get_BattleRuntimeUIEntries() const
{
    return Get_CachedBattleRuntimeUIEntries();
}

vector<RuntimeUIEntry> GameplayUI::Get_SoulLikeRuntimeUIEntries() const
{
    return Get_CachedSoulLikeRuntimeUIEntries();
}

const vector<RuntimeUIEntry>& GameplayUI::Get_CachedWorldRuntimeUIEntries() const
{
    Rebuild_RuntimeUIEntryCache();
    return _runtimeUIEntryCache.worldEntries;
}

const vector<RuntimeUIEntry>& GameplayUI::Get_CachedBattleRuntimeUIEntries() const
{
    Rebuild_RuntimeUIEntryCache();
    return _runtimeUIEntryCache.battleEntries;
}

const vector<RuntimeUIEntry>& GameplayUI::Get_CachedSoulLikeRuntimeUIEntries() const
{
    Rebuild_RuntimeUIEntryCache();
    return _runtimeUIEntryCache.soulLikeEntries;
}

void GameplayUI::Refresh_RuntimeUIEntryCache() const
{
    _runtimeUIEntryCache.dirty = true;
    Rebuild_RuntimeUIEntryCache();
}

void GameplayUI::Invalidate_RuntimeUIEntryCache()
{
    _runtimeUIEntryCache.dirty = true;
}

void GameplayUI::Rebuild_RuntimeUIEntryCache() const
{
    if (!_runtimeUIEntryCache.dirty &&
        _runtimeUIEntryCache.frameSerial == _runtimeUIEntryFrameSerial)
    {
        return;
    }

    _runtimeUIEntryCache.worldEntries = _worldUI ? _worldUI->Get_RuntimeUIEntries() : vector<RuntimeUIEntry>{};
    _runtimeUIEntryCache.battleEntries = _battleUI ? _battleUI->Get_RuntimeUIEntries() : vector<RuntimeUIEntry>{};
    _runtimeUIEntryCache.soulLikeEntries = _soulLikeUI ? _soulLikeUI->Get_RuntimeUIEntries() : vector<RuntimeUIEntry>{};

    _runtimeUIEntryCache.editorTargetsByRoot.clear();

    const auto appendEditorTargets = [this](const vector<RuntimeUIEntry>& entries)
    {
        for (const RuntimeUIEntry& entry : entries)
        {
            if (entry.ui == nullptr || entry.editorTarget == nullptr)
                continue;

            _runtimeUIEntryCache.editorTargetsByRoot[entry.ui.get()] = entry.editorTarget;
        }
    };

    appendEditorTargets(_runtimeUIEntryCache.worldEntries);
    appendEditorTargets(_runtimeUIEntryCache.battleEntries);
    appendEditorTargets(_runtimeUIEntryCache.soulLikeEntries);

    _runtimeUIEntryCache.frameSerial = _runtimeUIEntryFrameSerial;
    _runtimeUIEntryCache.dirty = false;
}

IRuntimeUI_EditorTarget* GameplayUI::Find_RuntimeUIEditorTarget(const Shared<Engine::UI_Base>& rootUI) const
{
    if (rootUI == nullptr)
        return nullptr;

    Rebuild_RuntimeUIEntryCache();

    const auto targetIter = _runtimeUIEntryCache.editorTargetsByRoot.find(rootUI.get());
    if (targetIter != _runtimeUIEntryCache.editorTargetsByRoot.end())
        return targetIter->second; // 선택된 루트 UI와 연결된 실제 오프셋 소유자를 반환한다.

    return nullptr;
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

bool GameplayUI::Open_MerchantShop()
{
    return _worldUI && _worldUI->Request_MerchantShop();
}

bool GameplayUI::Open_WeaponUpgrade()
{
    return _worldUI && _worldUI->Request_WeaponUpgrade();
}

bool GameplayUI::Open_ArenaCharacterSelect()
{
    return _worldUI && _worldUI->Request_ArenaCharacterSelect();
}

void GameplayUI::Queue_GetChromaPresentation(uint32 chromaAmount)
{
    if (_worldUI)
        _worldUI->Queue_GetChromaPresentation(chromaAmount);
}

void GameplayUI::Queue_LevelAnnounce(LevelType levelType)
{
    if (_worldUI)
        _worldUI->Queue_LevelAnnounce(levelType);
}

void GameplayUI::Push_WorldDialogueLine(const wstring& text, const wstring& portraitTexturePath)
{
    if (_worldUI)
        _worldUI->Push_WorldDialogueLine(text, portraitTexturePath);
}

void GameplayUI::Clear_WorldDialogue()
{
    if (_worldUI)
        _worldUI->Clear_WorldDialogue();
}

void GameplayUI::Close_WorldDialogue()
{
    if (_worldUI)
        _worldUI->Close_WorldDialogue();
}

void GameplayUI::Set_CinematicUIHidden(bool hidden)
{
    if (_isCinematicUIHidden == hidden)
        return;

    _isCinematicUIHidden = hidden;

    if (_isCinematicUIHidden)
        Close_RuntimeUIForCinematic();
}

void GameplayUI::Close_RuntimeUIForCinematic()
{
    Refresh_RuntimeUIEntryCache();

    const auto closeEntries = [](const vector<RuntimeUIEntry>& entries)
    {
        for (const RuntimeUIEntry& entry : entries)
        {
            if (entry.kind != RuntimeUIEntryKind::UI || entry.ui == nullptr)
                continue;

            if (entry.editorTarget != nullptr &&
                entry.ui != entry.editorTarget->Get_EditorRootUI())
            {
                continue; // 루트 아래 자식 UI까지 닫으면 시네마틱 종료 후 루트만 다시 열려 HUD가 비어 보인다.
            }

            if (entry.ui->Is_Open())
            {
                if (entry.editorTarget != nullptr &&
                    entry.ui == entry.editorTarget->Get_EditorRootUI())
                {
                    if (UI_RuntimePanelBase* panel = dynamic_cast<UI_RuntimePanelBase*>(entry.editorTarget))
                    {
                        panel->Close(); // 패널 Close를 타야 LockOn outline처럼 루트 밖 보조 UI까지 같이 닫힌다.
                        continue;
                    }
                }

                entry.ui->Close();
            }
        }
    };

    closeEntries(_runtimeUIEntryCache.worldEntries);
    closeEntries(_runtimeUIEntryCache.battleEntries);
    closeEntries(_runtimeUIEntryCache.soulLikeEntries);
}

GameplayUI::GameplayUIInputMode GameplayUI::Resolve_InputMode() const
{
    if (CLIENT->Is_BattleEntering() || CLIENT->Is_BattleActive())
        return GameplayUIInputMode::Battle;

    if (CLIENT->Is_SoulLikeBattleActive())
        return GameplayUIInputMode::SoulLike;

    return GameplayUIInputMode::World;
}

void GameplayUI::Handle_InputModeChanged(GameplayUIInputMode previousMode, GameplayUIInputMode currentMode)
{
    const bool leavingWorld =
        previousMode == GameplayUIInputMode::World &&
        currentMode != GameplayUIInputMode::World;

    if (leavingWorld && _worldUI)
        _worldUI->Force_CloseAllMenus();
}

void GameplayUI::Handle_EditorStopRuntime()
{
    _isCinematicUIHidden = false; // 시네마틱 중 F4/런타임 중단 시 UI 업데이트가 영구 차단되지 않게 복구한다.
    Invalidate_RuntimeUIEntryCache();
    CLIENT->Clear_ReplayWorldSnapshot();

    if (_worldUI)
    {
        _worldUI->Handle_EditorStopRuntime();
    }

    if (_battleUI)
    {
        _battleUI->Handle_EditorStopRuntime();
    }

    if (_soulLikeUI)
    {
        _soulLikeUI->Handle_EditorStopRuntime();
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
    Invalidate_RuntimeUIEntryCache();
    _runtimeUIEntryCache.worldEntries.clear();
    _runtimeUIEntryCache.battleEntries.clear();
    _runtimeUIEntryCache.soulLikeEntries.clear();
    _runtimeUIEntryCache.editorTargetsByRoot.clear();

    if (_editorStopHandle.IsValid())
    {
        GAME->Get_DelegateHub().OnEditorStopRuntime.Remove(_editorStopHandle);
        _editorStopHandle.Reset();
    }

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
    Release_UI(_soulLikeUI);

    Base::Free();
}
