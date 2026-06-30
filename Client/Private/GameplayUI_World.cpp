#include "GameplayUI_World.h"

#include "ChangeSkill_Panel.h"
#include "CharacterStatus_Panel.h"
#include "GameInstance.h"
#include "SkillTree_Panel.h"
#include "ChangeWeapon_Panel.h"
#include "UpgradeWeapon_Panel.h"
#include "Merchant_Panel.h"
#include "ExpeditionStat_Panel.h"
#include "UI_CrossHair.h"
#include "UI_GetCharacter.h"
#include "UI_GetClothes.h"
#include "UI_GetWeapon.h"
#include "UI_GetPictos.h"
#include "UI_GetChroma.h"
#include "ClientInstance.h"
#include "GameplayUI.h"
#include "Player_FreeAim.h"
#include "PlayerStateMachineCom.h"
#include "Player_WorldBase.h"
#include "SoulLikeController.h"
#include "Unlock_Manager.h"
#include "UI_ArenaCharacterSelect.h"
#include "Wardrobe_Panel.h"
#include "Pictos_Panel.h"
#include "UI_GradientSkillPreviewTest.h"
#include "UI_InteractionPrompt.h"
#include "UI_Replay.h"

#include "Client_Constants.h"
#include "GameObject.h"
#include "Spawn_Prefab.h"
#include "TransformCom.h"

GameplayUI_World::~GameplayUI_World() = default;

namespace
{
    constexpr char kSoulLikeMaelleTargetId[] = "SL_Maelle";

    template <typename TPanel>
    void Append_RuntimePanelEntry(
        vector<RuntimeUIEntry>& outEntries,
        const char* category,
        const char* name,
        const TPanel* panel,
        bool positionDrivenByCode = false)
    {
        if (panel == nullptr)
            return;

        Shared<UI_Base> root = panel->Get_RootUI();
        if (root == nullptr)
            return;

        outEntries.push_back({ category, name, root, nullptr, positionDrivenByCode });
    }
}

HRESULT GameplayUI_World::Initialize(uint32 levelIndex)
{
    _characterProgressionPanel = CharacterProgression_Panel::Create(levelIndex);
    CHECK_NULL(_characterProgressionPanel, E_FAIL);

    _changeSkillPanel = ChangeSkill_Panel::Create(levelIndex);
    CHECK_NULL(_changeSkillPanel, E_FAIL);

    _wardrobePanel = Wardrobe_Panel::Create(levelIndex);
    CHECK_NULL(_wardrobePanel, E_FAIL);

    _pictosPanel = Pictos_Panel::Create(levelIndex);
    CHECK_NULL(_pictosPanel, E_FAIL);

    _characterStatusPanel = CharacterStatus_Panel::Create(levelIndex);
    CHECK_NULL(_characterStatusPanel, E_FAIL);

    _skillTreePanel = SkillTree_Panel::Create(levelIndex);
    CHECK_NULL(_skillTreePanel, E_FAIL);

    _savePointPanel = SavePoint_Panel::Create(levelIndex);
    CHECK_NULL(_savePointPanel, E_FAIL);

    _changeWeaponPanel = ChangeWeapon_Panel::Create(levelIndex);
    CHECK_NULL(_changeWeaponPanel, E_FAIL);

    _upgradeWeaponPanel = UpgradeWeapon_Panel::Create(levelIndex);
    CHECK_NULL(_upgradeWeaponPanel, E_FAIL);

    _merchantPanel = Merchant_Panel::Create(levelIndex);
    CHECK_NULL(_merchantPanel, E_FAIL);

    _characterProgressionPanel->Bind_ContentPanels(
        _changeWeaponPanel.get(),
        _changeSkillPanel.get(),
        _wardrobePanel.get(),
        _pictosPanel.get());

    if (FAILED(_characterProgressionPanel->Prepare_OpenResources()))
        LOG_WARN("GameplayUI_World: failed to prewarm CharacterProgression open resources");

    if (FAILED(_wardrobePanel->Prepare_OpenResources()))
        LOG_WARN("GameplayUI_World: failed to prewarm Wardrobe open resources");

    if (const Shared<UIPreviewManager> previewManager = CLIENT->Get_PreviewManager())
    {
        if (FAILED(previewManager->Prepare_PreviewResources()))
            LOG_WARN("GameplayUI_World: failed to prepare preview resources");
    }

    _expeditionStatPanel = ExpeditionStat_Panel::Create(levelIndex);
    CHECK_NULL(_expeditionStatPanel, E_FAIL);

    _crossHair = UI_CrossHair::Create(levelIndex);
    CHECK_NULL(_crossHair, E_FAIL);

    _interactionPrompt = UI_InteractionPrompt::Create(levelIndex);
    CHECK_NULL(_interactionPrompt, E_FAIL);

    _getCharacterPanel = UI_GetCharacter::Create(levelIndex);
    CHECK_NULL(_getCharacterPanel, E_FAIL);

    _getClothesPanel = UI_GetClothes::Create(levelIndex);
    CHECK_NULL(_getClothesPanel, E_FAIL);

    _getWeaponPanel = UI_GetWeapon::Create(levelIndex);
    CHECK_NULL(_getWeaponPanel, E_FAIL);

    _getPictosPanel = UI_GetPictos::Create(levelIndex);
    CHECK_NULL(_getPictosPanel, E_FAIL);

    _getChromaPanel = UI_GetChroma::Create(levelIndex);
    CHECK_NULL(_getChromaPanel, E_FAIL);

    _arenaCharacterSelectPanel = UI_ArenaCharacterSelect::Create(levelIndex);
    CHECK_NULL(_arenaCharacterSelectPanel, E_FAIL);

    _gradientSkillPreviewTestPanel = UI_GradientSkillPreviewTest::Create(levelIndex);
    CHECK_NULL(_gradientSkillPreviewTestPanel, E_FAIL);

    return S_OK;
}

void GameplayUI_World::Update(float timeDelta)
{
    if (_characterProgressionPanel)
        _characterProgressionPanel->Update(timeDelta);

    if (_changeSkillPanel)
        _changeSkillPanel->Update(timeDelta);

    if (_wardrobePanel)
        _wardrobePanel->Update(timeDelta);

    if (_pictosPanel)
        _pictosPanel->Update(timeDelta);

    if (_changeWeaponPanel)
        _changeWeaponPanel->Update(timeDelta);

    if (_upgradeWeaponPanel)
        _upgradeWeaponPanel->Update(timeDelta);

    if (_merchantPanel)
        _merchantPanel->Update(timeDelta);

    if (_savePointPanel)
        _savePointPanel->Update(timeDelta);

    if (_characterStatusPanel)
        _characterStatusPanel->Update(timeDelta);

    if (_skillTreePanel)
        _skillTreePanel->Update(timeDelta);

    if (_expeditionStatPanel)
        _expeditionStatPanel->Update(timeDelta);

    if (_getCharacterPanel)
        _getCharacterPanel->Update(timeDelta);

    if (_getClothesPanel)
        _getClothesPanel->Update(timeDelta);

    if (_getWeaponPanel)
        _getWeaponPanel->Update(timeDelta);

    if (_getPictosPanel)
        _getPictosPanel->Update(timeDelta);

    if (_getChromaPanel)
        _getChromaPanel->Update(timeDelta);

    if (_arenaCharacterSelectPanel)
        _arenaCharacterSelectPanel->Update(timeDelta);

     if (_gradientSkillPreviewTestPanel)
        _gradientSkillPreviewTestPanel->Update(timeDelta);

    if (_replayPanel)
        _replayPanel->Update(timeDelta);

    Update_CrossHair(timeDelta);
    Update_InteractionPrompt();

    if (!CLIENT->Is_FadeTransitionPlaying())
    {
        Try_ShowPendingGetCharacter();

        if (Is_GetCharacterOpen())
            return;

        Try_ShowPendingGetClothes();

        if (Is_GetClothesOpen())
            return;

        Try_ShowPendingGetWeapon();

        if (Is_GetWeaponOpen())
            return;

        Try_ShowPendingGetPictos();

        if (Is_GetPictosOpen())
            return;

        Try_ShowPendingGetChroma();

        if (Is_GetChromaOpen())
            return;

        if (_arenaCharacterSelectPanel && _arenaCharacterSelectPanel->Is_Open())
            Handle_ArenaCharacterSelectAction(_arenaCharacterSelectPanel->Consume_LastAction());

        if (_replayPanel && _replayPanel->Is_Open())
            Handle_ReplayAction(_replayPanel->Consume_LastAction());

        if (_savePointPanel && _savePointPanel->Is_Open() && !_savePointPanel->Is_ClosingWithOutro())
            Handle_SavePointAction(_savePointPanel->Consume_LastAction());

        if (_characterProgressionPanel && _characterProgressionPanel->Is_Open())
            Handle_CharacterProgressionAction(_characterProgressionPanel->Consume_LastAction());
    }
}

void GameplayUI_World::Update_Input()
{
    if (_replayPanel && _replayPanel->Is_Open())
    {
        Handle_BackInput();
        return;
    }

    if (CLIENT->Is_FadeTransitionPlaying())
        return;

    if (_savePointPanel && _savePointPanel->Is_ClosingWithOutro())
        return;

    if (Is_GetCharacterOpen())
        return;

    if (Is_GetClothesOpen())
        return;

    if (Is_GetWeaponOpen())
        return;

    if (Is_GetPictosOpen())
        return;

    if (Is_GetChromaOpen())
        return;

    Handle_GradientSkillPreviewTestInput();

    if (Is_GradientSkillPreviewTestOpen())
        return;

    Handle_ArenaCharacterSelectInput();
    Handle_SavePointInput();
    Handle_TabInput();
    Handle_BackInput();
    Handle_ExpeditionStat();
}

vector<RuntimeUIEntry> GameplayUI_World::Get_RuntimeUIEntries() const
{
    vector<RuntimeUIEntry> entries{};
    entries.reserve(64);

    if (_characterProgressionPanel)
        _characterProgressionPanel->Append_RuntimeUIEntries(entries, "World", "CharacterProgressionPanel");

    if (_changeSkillPanel)
        _changeSkillPanel->Append_RuntimeUIEntries(entries, "World", "ChangeSkillPanel");

    if (_wardrobePanel)
        _wardrobePanel->Append_RuntimeUIEntries(entries, "World", "WardrobePanel");

    if (_pictosPanel)
        _pictosPanel->Append_RuntimeUIEntries(entries, "World", "PictosPanel");

    if (_savePointPanel)
        _savePointPanel->Append_RuntimeUIEntries(entries, "World", "SavePointPanel");

    if (_characterStatusPanel)
        _characterStatusPanel->Append_RuntimeUIEntries(entries, "World", "CharacterStatusPanel");

    Append_RuntimePanelEntry(entries, "World", "SkillTreePanel", _skillTreePanel.get());

    if (_changeWeaponPanel)
        _changeWeaponPanel->Append_RuntimeUIEntries(entries, "World", "ChangeWeaponPanel");

    if (_upgradeWeaponPanel)
        _upgradeWeaponPanel->Append_RuntimeUIEntries(entries, "World", "UpgradeWeaponPanel");

    if (_merchantPanel)
        _merchantPanel->Append_RuntimeUIEntries(entries, "World", "MerchantPanel");

    if (_expeditionStatPanel)
        _expeditionStatPanel->Append_RuntimeUIEntries(entries, "World", "ExpeditionStatPanel");

    if (_getCharacterPanel)
        _getCharacterPanel->Append_RuntimeUIEntries(entries, "World", "GetCharacterPanel");

    if (_getClothesPanel)
        _getClothesPanel->Append_RuntimeUIEntries(entries, "World", "GetClothesPanel");

    if (_getWeaponPanel)
        _getWeaponPanel->Append_RuntimeUIEntries(entries, "World", "GetWeaponPanel");

    if (_getPictosPanel)
        _getPictosPanel->Append_RuntimeUIEntries(entries, "World", "GetPictosPanel");

    if (_getChromaPanel)
        _getChromaPanel->Append_RuntimeUIEntries(entries, "World", "GetChromaPanel");

    if (_arenaCharacterSelectPanel)
        _arenaCharacterSelectPanel->Append_RuntimeUIEntries(entries, "World", "ArenaCharacterSelectPanel");

    if (_replayPanel)
        _replayPanel->Append_RuntimeUIEntries(entries, "World", "ReplayPanel");

    Append_RuntimePanelEntry(entries, "World", "CrossHair", _crossHair.get());
    Append_RuntimePanelEntry(entries, "World", "InteractionPrompt", _interactionPrompt.get());
    Append_RuntimePanelEntry(entries, "World", "GradientSkillPreviewTest", _gradientSkillPreviewTestPanel.get());

    return entries;
}

bool GameplayUI_World::Is_AnyMenuOpen() const
{
    return Is_GetCharacterOpen() ||
        Is_GetClothesOpen() ||
        Is_GetWeaponOpen() ||
        Is_GetPictosOpen() ||
        Is_GetChromaOpen() ||
        !_menuStack.empty() ||
        (_replayPanel && _replayPanel->Is_Open()) ||
        (_savePointPanel && _savePointPanel->Is_ClosingWithOutro());
}

GameplayMenu GameplayUI_World::Get_CurrentMenu() const
{
    if (_menuStack.empty())
        return GameplayMenu::None;

    return _menuStack.back();
}

void GameplayUI_World::Force_CloseAllMenus()
{
    Close_AllMenus(true); // 배틀 진입 시 월드 메뉴가 남아 있지 않게 강제 정리

    if (_getCharacterPanel && _getCharacterPanel->Is_Open())
        _getCharacterPanel->Close_Immediately();

    if (_getClothesPanel && _getClothesPanel->Is_Open())
        _getClothesPanel->Close_Immediately();

    if (_getWeaponPanel && _getWeaponPanel->Is_Open())
        _getWeaponPanel->Close_Immediately();

    if (_getPictosPanel && _getPictosPanel->Is_Open())
        _getPictosPanel->Close_Immediately();

    if (_getChromaPanel && _getChromaPanel->Is_Open())
        _getChromaPanel->Close_Immediately();

    if (_arenaCharacterSelectPanel && _arenaCharacterSelectPanel->Is_Open())
        _arenaCharacterSelectPanel->Close_Immediately();

    if (_merchantPanel && _merchantPanel->Is_Open())
        _merchantPanel->Close_Immediately();

    if (_replayPanel && _replayPanel->Is_Open())
        _replayPanel->Close_Immediately();

    if (_gradientSkillPreviewTestPanel && _gradientSkillPreviewTestPanel->Is_Open())
        _gradientSkillPreviewTestPanel->Close_Immediately();
}

void GameplayUI_World::Handle_EditorStopRuntime()
{
    Force_CloseAllMenus();
    GAME->Set_GameplayInputMode(GameplayInputMode::Normal);
}

bool GameplayUI_World::Request_ArenaCharacterSelect()
{
    if (_arenaCharacterSelectPanel == nullptr)
        return false;

    if (_expeditionStatPanel && _expeditionStatPanel->Is_Open())
        return false;

    if (Get_CurrentMenu() != GameplayMenu::None)
        return false;

    if (CLIENT->Is_WorldControlSuspended() || CLIENT->Is_BattleEntering())
        return false;

    if (GAME->Is_WorldGameplayInputBlocked())
        return false;

    Push_Menu(GameplayMenu::ArenaCharacterSelect);

    return true;
}

bool GameplayUI_World::Request_MerchantShop()
{
    if (_merchantPanel == nullptr)
        return false;

    const bool requestedFromDialogue = CLIENT->IsDialogueInteractLocked();

    if (_expeditionStatPanel && _expeditionStatPanel->Is_Open())
    {
        if (!requestedFromDialogue)
            return false;

        _expeditionStatPanel->Close_Immediately();
    }

    if (Get_CurrentMenu() != GameplayMenu::None)
        return false;

    if (CLIENT->Is_WorldControlSuspended() || CLIENT->Is_BattleEntering())
        return false;

    if (GAME->Is_WorldGameplayInputBlocked() && !requestedFromDialogue)
        return false;

    Push_Menu(GameplayMenu::Merchant);
    return true;
}

bool GameplayUI_World::Request_WeaponUpgrade()
{
    if (_upgradeWeaponPanel == nullptr)
        return false;

    const bool requestedFromDialogue = CLIENT->IsDialogueInteractLocked();

    if (_expeditionStatPanel && _expeditionStatPanel->Is_Open())
    {
        if (!requestedFromDialogue)
            return false;

        _expeditionStatPanel->Close_Immediately();
    }

    if (Get_CurrentMenu() != GameplayMenu::None)
        return false;

    if (CLIENT->Is_WorldControlSuspended() || CLIENT->Is_BattleEntering())
        return false;

    if (GAME->Is_WorldGameplayInputBlocked() && !requestedFromDialogue)
        return false;

    Push_Menu(GameplayMenu::WeaponUpgrade);
    return true;
}

void GameplayUI_World::Queue_GetChromaPresentation(uint32 chromaAmount)
{
    if (chromaAmount == 0)
        return;

    _pendingGetChromaAmounts.emplace_back(chromaAmount);
}

Shared<SoulLikeController> GameplayUI_World::Spawn_SoulLikeControllerForArena()
{
    if (const Shared<SoulLikeController> existingController = CLIENT->Get_SoulLikeController())
    {
        if (existingController->Is_BattleActive())
            return nullptr;

        return existingController;
    }

    const uint32 levelIndex = GAME->Current_LevelIndex();

    const Shared<GameObject> controllerObject = Spawn_Prefab::Prefab(Constants::Prefab::SoulLikeController)
        .AtLevel(levelIndex)
        .InLayer(TEXT("Layer_ETC"))
        .Spawn();

    if (controllerObject == nullptr)
    {
        LOG_WARN("ArenaCharacterSelect: failed to spawn SoulLikeController.");
        return nullptr;
    }

    const Shared<SoulLikeController> soulLikeController =
        dynamic_pointer_cast<SoulLikeController>(controllerObject);

    if (soulLikeController == nullptr)
    {
        LOG_WARN("ArenaCharacterSelect: spawned object is not SoulLikeController.");
        return nullptr;
    }

    CLIENT->Set_SoulLikeController(soulLikeController);
    return soulLikeController;
}

void GameplayUI_World::Handle_SavePointAction(SavePoint_Panel::SavePointAction action)
{
    switch (action)
    {
    case SavePoint_Panel::SavePointAction::Rest:
        Close_AllMenus(); // 현재는 SavePoint 닫기만. 나중에 Rest 처리 연결
        break;

    case SavePoint_Panel::SavePointAction::UpgradeStat:
        Push_Menu(GameplayMenu::CharacterStatus);   // SavePoint 위에 CharacterStatus push
        break;

    case SavePoint_Panel::SavePointAction::UpgradeSkill:
        Push_Menu(GameplayMenu::SkillTree);         // SavePoint 위에 SkillTree push
        break;

    case SavePoint_Panel::SavePointAction::UpgradeWeapon:
        Open_CharacterProgressionFlow(CharacterProgression_Panel::ContentTab::Weapon);
        break;

    case SavePoint_Panel::SavePointAction::None:
    default:
        break;
    }
}

void GameplayUI_World::Handle_SavePointInput()
{
    if (!GAME->KeyDown(KEY_TYPE::G))
        return;

    if (_savePointPanel == nullptr)
        return;

    const Shared<Player_WorldBase> activeHero = CLIENT->Get_ActiveHero();
    if (activeHero != nullptr &&
        (activeHero->Is_WorldInteracting() || activeHero->Get_WorldInteractionCandidate() != nullptr))
    {
        return;
    }

    if (_expeditionStatPanel && _expeditionStatPanel->Is_Open())
        return;

    if (Get_CurrentMenu() != GameplayMenu::None)
        return; // 다른 월드 메뉴가 열려 있으면 SavePoint 중복 진입을 막는다.

    Push_Menu(GameplayMenu::SavePoint);
}

void GameplayUI_World::Handle_ArenaCharacterSelectInput()
{
    if (!GAME->KeyDown(KEY_TYPE::KEY_3))
        return;

    Request_ArenaCharacterSelect();
}

void GameplayUI_World::Handle_TabInput()
{
    if (!GAME->KeyDown(KEY_TYPE::TAB))
        return;

    if (_expeditionStatPanel && _expeditionStatPanel->Is_Open())
        return;

    const GameplayMenu currentMenu = Get_CurrentMenu();

    if (Is_CharacterProgressionFlowMenu(currentMenu))
    {
        Close_CharacterProgressionFlow();
        return;
    }

    if (currentMenu != GameplayMenu::None)
        return;

    Open_CharacterProgressionFlow();
}

void GameplayUI_World::Handle_ExpeditionStat()
{
    if (!GAME->KeyDown(KEY_TYPE::Q))
        return;

    if (_expeditionStatPanel == nullptr)
        return;

    if (!_menuStack.empty())
        return; // 다른 월드 메뉴가 열려 있으면 막기

    _expeditionStatPanel->Toggle();
}

void GameplayUI_World::Handle_BackInput()
{
    if (!GAME->KeyDown(KEY_TYPE::ESCAPE))
        return;

    if (_replayPanel && _replayPanel->Is_Open())
    {
        Close_AllMenus(true);

        if (_replayPanel->Is_Open())
            _replayPanel->Close_Immediately();

        return;
    }

    if (_expeditionStatPanel && _expeditionStatPanel->Is_Open())
    {
        _expeditionStatPanel->Close();
        return;
    }

    if (_menuStack.empty())
    {
        if (CLIENT->Is_WorldControlSuspended() || CLIENT->Is_BattleEntering())
            return;

        if (GAME->Is_WorldGameplayInputBlocked())
            return;

        if (_replayPanel == nullptr)
        {
            _replayPanel = UI_Replay::Create(GAME->Current_LevelIndex());
            if (_replayPanel == nullptr)
                return;
        }

        Push_Menu(GameplayMenu::Replay);
        GAME->Set_GameplayInputMode(GameplayInputMode::UIOnly);
        GAME->Set_PlayerInputBlocked(true);
        GAME->Unlock_Mouse();
        return;
    }

    const GameplayMenu currentMenu = _menuStack.back();
    if (currentMenu == GameplayMenu::WeaponUpgrade &&
        _upgradeWeaponPanel &&
        _upgradeWeaponPanel->Consume_BackInput())
        return;

    if (currentMenu == GameplayMenu::Replay)
    {
        Close_AllMenus();
        return;
    }

    if (Is_CharacterProgressionFlowMenu(currentMenu) && currentMenu == GameplayMenu::CharacterProgression)
    {
        if (_characterProgressionPanel &&
            _characterProgressionPanel->Get_ActiveContentTab() != CharacterProgression_Panel::ContentTab::Overview)
        {
            _characterProgressionPanel->Switch_ContentTab(CharacterProgression_Panel::ContentTab::Overview);
            return;
        }

        Close_CharacterProgressionFlow(); // 루트에서 Esc는 전체 종료
        return;
    }

    if (_menuStack.size() == 1)
    {
        const auto applyMenuChange = [this, currentMenu]()
            {
                Close_Menu(currentMenu, true); // 단독 루트 메뉴 종료이므로 월드 입력을 복구한다.
                _menuStack.pop_back();
            };

        _menuFade.Request_MenuTransition(currentMenu, GameplayMenu::None, applyMenuChange);
        return;
    }

    Pop_Menu(); // 하위 메뉴에서 Esc는 이전 메뉴 복귀
}

void GameplayUI_World::Handle_CharacterProgressionAction(CharacterProgression_Panel::Action action)
{
    switch (action)
    {
    case CharacterProgression_Panel::Action::OpenChangeWeapon:
        if (_characterProgressionPanel)
            _characterProgressionPanel->Switch_ContentTab(CharacterProgression_Panel::ContentTab::Weapon);
        break;

    case CharacterProgression_Panel::Action::OpenChangeSkill:
        if (_characterProgressionPanel)
            _characterProgressionPanel->Switch_ContentTab(CharacterProgression_Panel::ContentTab::Skill);
        break;

    case CharacterProgression_Panel::Action::OpenWardrobe:
        if (_characterProgressionPanel)
            _characterProgressionPanel->Switch_ContentTab(CharacterProgression_Panel::ContentTab::Wardrobe);
        break;

    case CharacterProgression_Panel::Action::OpenPictos:
        if (_characterProgressionPanel)
            _characterProgressionPanel->Switch_ContentTab(CharacterProgression_Panel::ContentTab::Pictos);
        break;

    case CharacterProgression_Panel::Action::None:
    default:
        break;
    }
}

void GameplayUI_World::Handle_ArenaCharacterSelectAction(UI_ArenaCharacterSelect::Action action)
{
    switch (action)
    {
    case UI_ArenaCharacterSelect::Action::SelectGustave:
        LOG_INFO("ArenaCharacterSelect: Gustave selected. SoulLike entry is disabled.");
        break;

    case UI_ArenaCharacterSelect::Action::SelectLune:
        LOG_INFO("ArenaCharacterSelect: Lune selected. SoulLike entry is disabled.");
        break;

    case UI_ArenaCharacterSelect::Action::SelectMaelle:
    {
        const Shared<SoulLikeController> controller = Spawn_SoulLikeControllerForArena();
        if (controller == nullptr)
        {
            LOG_WARN("ArenaCharacterSelect: SoulLikeController is missing.");
            return;
        }

        Close_AllMenus(true);

        const wstring sequencePath =
            wstring(Constants::Cinematic::RuntimeJson_Dir) +
            Constants::Cinematic::Dualliste_Intro;

        Weak<SoulLikeController> weakController = controller;

        CLIENT->Clear_CinematicCompletedCallbacks();
        CLIENT->Add_CinematicCompletedCallback(
            [weakController]()
            {
                const Shared<SoulLikeController> controller = weakController.lock();
                if (controller == nullptr)
                    return;

                const Shared<GameObject> cinematicMaelle = CLIENT->Get_CinematicBoundActor(kSoulLikeMaelleTargetId);
                const Shared<GameObject> cinematicDualliste = CLIENT->Get_CinematicBoundActor(Constants::Prefab::SL_Dualliste);
                const Shared<TransformCom> maelleTransform = cinematicMaelle ? cinematicMaelle->Get_Transform() : nullptr;
                const Shared<TransformCom> duallisteTransform = cinematicDualliste ? cinematicDualliste->Get_Transform() : nullptr;
                if (maelleTransform != nullptr && duallisteTransform != nullptr)
                {
                    controller->Set_BattleSpawnPoseOverride(
                        maelleTransform->Get_WorldPosition(),
                        maelleTransform->Get_WorldRotationEuler(),
                        duallisteTransform->Get_WorldPosition(),
                        duallisteTransform->Get_WorldRotationEuler());
                }

                controller->Set_NextMonsterActionDelay(2.f);

                if (!controller->Begin_Battle())
                    LOG_WARN("ArenaCharacterSelect: Maelle selected, but SoulLike battle did not start.");
            });

        controller->Start_DuallistePhase1BGM();

        if (!CLIENT->Play_Sequence(sequencePath, 0))
        {
            CLIENT->Clear_CinematicCompletedCallbacks();

            if (!controller->Begin_Battle())
                LOG_WARN("ArenaCharacterSelect: Maelle selected, but SoulLike battle did not start.");
        }

        break;
    }

    case UI_ArenaCharacterSelect::Action::Back:
        Close_AllMenus();
        break;

    case UI_ArenaCharacterSelect::Action::None:
    default:
        break;
    }
}

void GameplayUI_World::Handle_ReplayAction(UI_Replay::Action action)
{
    switch (action)
    {
    case UI_Replay::Action::Replay:
        CLIENT->Reset_LastDialogueNpcAnimationToStart();
        Close_AllMenus(true);

        if (!CLIENT->Request_CurrentLevelReplay())
            LOG_WARN("UI_Replay: failed to request current level replay.");
        break;

    case UI_Replay::Action::GameExit:
        PostQuitMessage(0);
        break;

    case UI_Replay::Action::None:
    default:
        break;
    }
}

void GameplayUI_World::Handle_GradientSkillPreviewTestInput()
{
    if (_gradientSkillPreviewTestPanel == nullptr)
        return;

    //if (GAME->KeyDown(KEY_TYPE::KEY_4))
    //{
    //    if (_gradientSkillPreviewTestPanel->Is_Open())
    //        _gradientSkillPreviewTestPanel->Close();
    //    else if (!Is_AnyMenuOpen())
    //        _gradientSkillPreviewTestPanel->Open();
    //
    //    return;
    //}

    if (_gradientSkillPreviewTestPanel->Is_Open() && GAME->KeyDown(KEY_TYPE::ESCAPE))
    {
        _gradientSkillPreviewTestPanel->Close();
    }
}

bool GameplayUI_World::Is_GradientSkillPreviewTestOpen() const
{
    return _gradientSkillPreviewTestPanel != nullptr &&
        _gradientSkillPreviewTestPanel->Is_Open();
}

void GameplayUI_World::Open_CharacterProgressionFlow()
{
    Open_CharacterProgressionFlow(CharacterProgression_Panel::ContentTab::Overview);
}

void GameplayUI_World::Open_CharacterProgressionFlow(CharacterProgression_Panel::ContentTab initialTab)
{
    HeroId selectedHeroId = CLIENT->Get_HeroId(CLIENT->Get_ActiveHeroSlot());
    if (_characterProgressionPanel)
    {
        _characterProgressionPanel->Set_SelectedHeroId(selectedHeroId);
    }

    const GameplayMenu currentMenu = Get_CurrentMenu();
    if (currentMenu == GameplayMenu::CharacterProgression)
    {
        if (_characterProgressionPanel)
            _characterProgressionPanel->Switch_ContentTab(initialTab);
        return;
    }

    const auto applyMenuChange = [this, currentMenu, initialTab]()
        {
            if (currentMenu != GameplayMenu::None)
            {
                if (currentMenu == GameplayMenu::SavePoint && _savePointPanel)
                    _savePointPanel->Close_ForMenuTransition();
                else
                    Close_Menu(currentMenu, false);

                if (!_menuStack.empty())
                    _menuStack.pop_back();
            }

            _menuStack.push_back(GameplayMenu::CharacterProgression);

            if (_characterProgressionPanel)
                _characterProgressionPanel->Open(initialTab);
        };

    _menuFade.Request_MenuTransition(currentMenu, GameplayMenu::CharacterProgression, applyMenuChange);
}

void GameplayUI_World::Close_CharacterProgressionFlow()
{
    if (_menuStack.empty())
        return;

    const GameplayMenu currentMenu = _menuStack.back();
    if (!Is_CharacterProgressionFlowMenu(currentMenu))
        return;

    const auto applyMenuChange = [this, currentMenu]()
        {
            while (!_menuStack.empty() && Is_CharacterProgressionFlowMenu(_menuStack.back()))
            {
                const GameplayMenu flowMenu = _menuStack.back();
                Close_Menu(flowMenu, flowMenu == currentMenu); // top 메뉴만 월드 입력을 복구하고, 아래 Progression 루트도 실제로 닫는다.
                _menuStack.pop_back();
            }
        };

    _menuFade.Request_MenuTransition(currentMenu, GameplayMenu::None, applyMenuChange);
}

bool GameplayUI_World::Is_CharacterProgressionFlowMenu(GameplayMenu menu) const
{
    return menu == GameplayMenu::CharacterProgression ||
        menu == GameplayMenu::WeaponUpgrade ||
        menu == GameplayMenu::ChangeSkill ||
        menu == GameplayMenu::Wardrobe;
}

void GameplayUI_World::Close_Menu(GameplayMenu menu, bool restoreWorldControl)
{
    switch (menu)
    {
    case GameplayMenu::CharacterProgression:
        if (_characterProgressionPanel)
        {
            if (restoreWorldControl)
                _characterProgressionPanel->Close();
            else
                _characterProgressionPanel->Close_ForMenuTransition();
        }
        break;

    case GameplayMenu::ChangeSkill:
        if (_changeSkillPanel)
        {
            if (restoreWorldControl)
                _changeSkillPanel->Close();
            else
                _changeSkillPanel->Close_ForMenuTransition();
        }
        break;

    case GameplayMenu::Wardrobe:
        if (_wardrobePanel)
        {
            if (restoreWorldControl)
                _wardrobePanel->Close();
            else
                _wardrobePanel->Close_ForMenuTransition();
        }
        break;

    case GameplayMenu::WeaponUpgrade:
        if (_upgradeWeaponPanel)
        {
            if (restoreWorldControl)
                _upgradeWeaponPanel->Close(); // 최종 종료이므로 월드 입력 복구
            else
                _upgradeWeaponPanel->Close_ForMenuTransition(); // Character Progression으로 복귀하는 전환
        }
        break;

    case GameplayMenu::Merchant:
        if (_merchantPanel)
        {
            if (restoreWorldControl)
                _merchantPanel->Close();
            else
                _merchantPanel->Close_ForMenuTransition();

            vector<string> purchasedPictosIds = _merchantPanel->Consume_PurchasedPictosIds();
            _pendingGetPictosIds.insert(
                _pendingGetPictosIds.end(),
                purchasedPictosIds.begin(),
                purchasedPictosIds.end());
        }
        break;

    case GameplayMenu::SavePoint:
        if (_savePointPanel)
        {
            if (restoreWorldControl)
                _savePointPanel->Close(); // SavePoint 자체 종료
            else
                _savePointPanel->Close_ForMenuTransition();
        }
        break;

    case GameplayMenu::CharacterStatus:
        if (_characterStatusPanel)
            _characterStatusPanel->Close();
        break;

    case GameplayMenu::SkillTree:
        if (_skillTreePanel)
            _skillTreePanel->Close();
        break;

    case GameplayMenu::ArenaCharacterSelect:
        if (_arenaCharacterSelectPanel)
        {
            if (restoreWorldControl)
                _arenaCharacterSelectPanel->Close();
            else
                _arenaCharacterSelectPanel->Close_ForMenuTransition();
        }
        break;

    case GameplayMenu::Replay:
        if (_replayPanel)
        {
            if (restoreWorldControl)
                _replayPanel->Close();
            else
                _replayPanel->Close_ForMenuTransition();
        }
        break;

    case GameplayMenu::None:
    default:
        break;
    }


}

void GameplayUI_World::Try_ShowPendingGetCharacter()
{
    if (_getCharacterPanel == nullptr || _getCharacterPanel->Is_Open())
        return;

    if (CLIENT->Is_FadeTransitionPlaying())
        return;

    if (CLIENT->Is_WorldControlSuspended() || CLIENT->Is_BattleEntering())
        return;

    if (GAME->Is_WorldGameplayInputBlocked())
        return; // 다른 시스템이 입력을 막은 상태에서는 pending을 소비하지 않는다.

    if (!_menuStack.empty())
        return;

    if (_savePointPanel && (_savePointPanel->Is_Open() || _savePointPanel->Is_ClosingWithOutro()))
        return;

    if (_expeditionStatPanel && _expeditionStatPanel->Is_Open())
        return;

    Unlock_Manager* unlockManager = CLIENT->Get_UnlockManager();
    if (unlockManager == nullptr)
        return;

    vector<HeroId> pendingHeroes = unlockManager->Consume_GetCharacterHeroes();
    if (pendingHeroes.empty())
        return;

    _getCharacterPanel->Open_ForHeroes(pendingHeroes);
}

bool GameplayUI_World::Is_GetCharacterOpen() const
{
    return _getCharacterPanel && _getCharacterPanel->Is_Open();
}

void GameplayUI_World::Try_ShowPendingGetClothes()
{
    if (_getClothesPanel == nullptr || _getClothesPanel->Is_Open())
        return;

    if (Is_GetCharacterOpen())
        return;

    if (CLIENT->Is_FadeTransitionPlaying())
        return;

    if (CLIENT->Is_WorldControlSuspended() || CLIENT->Is_BattleEntering())
        return;

    if (GAME->Is_WorldGameplayInputBlocked())
        return; // 다른 시스템이 입력을 막은 상태에서는 pending을 소비하지 않는다.

    if (!_menuStack.empty())
        return;

    if (_savePointPanel && (_savePointPanel->Is_Open() || _savePointPanel->Is_ClosingWithOutro()))
        return;

    if (_expeditionStatPanel && _expeditionStatPanel->Is_Open())
        return;

    Unlock_Manager* unlockManager = CLIENT->Get_UnlockManager();
    if (unlockManager == nullptr)
        return;

    vector<string> pendingPresetIds = unlockManager->Consume_GetClothesPresetIds();
    if (pendingPresetIds.empty())
        return;

    _getClothesPanel->Open_ForPresetIds(pendingPresetIds);
}

void GameplayUI_World::Try_ShowPendingGetWeapon()
{
    if (_getWeaponPanel == nullptr || _getWeaponPanel->Is_Open())
        return;

    if (Is_GetCharacterOpen() || Is_GetClothesOpen())
        return;

    if (CLIENT->Is_FadeTransitionPlaying())
        return;

    if (CLIENT->Is_WorldControlSuspended() || CLIENT->Is_BattleEntering())
        return;

    if (GAME->Is_WorldGameplayInputBlocked())
        return;

    if (!_menuStack.empty())
        return;

    if (_savePointPanel && (_savePointPanel->Is_Open() || _savePointPanel->Is_ClosingWithOutro()))
        return;

    if (_expeditionStatPanel && _expeditionStatPanel->Is_Open())
        return;

    Unlock_Manager* unlockManager = CLIENT->Get_UnlockManager();
    if (unlockManager == nullptr)
        return;

    vector<string> pendingWeaponIds = unlockManager->Consume_GetWeaponPresentationIds();
    if (pendingWeaponIds.empty())
        return;

    _getWeaponPanel->Open_ForWeaponIds(pendingWeaponIds);
}

void GameplayUI_World::Try_ShowPendingGetPictos()
{
    if (_getPictosPanel == nullptr || _getPictosPanel->Is_Open())
        return;

    if (Is_GetCharacterOpen() || Is_GetClothesOpen() || Is_GetWeaponOpen())
        return;

    if (CLIENT->Is_FadeTransitionPlaying())
        return;

    if (CLIENT->Is_WorldControlSuspended() || CLIENT->Is_BattleEntering())
        return;

    if (GAME->Is_WorldGameplayInputBlocked())
        return;

    if (!_menuStack.empty())
        return;

    if (_savePointPanel && (_savePointPanel->Is_Open() || _savePointPanel->Is_ClosingWithOutro()))
        return;

    if (_expeditionStatPanel && _expeditionStatPanel->Is_Open())
        return;

    vector<string> pendingPictosIds = std::move(_pendingGetPictosIds);
    _pendingGetPictosIds.clear();

    if (Unlock_Manager* unlockManager = CLIENT->Get_UnlockManager())
    {
        vector<string> queuedPictosIds = unlockManager->Consume_GetPictosPresentationIds();
        pendingPictosIds.insert(
            pendingPictosIds.end(),
            queuedPictosIds.begin(),
            queuedPictosIds.end());
    }

    if (pendingPictosIds.empty())
        return;

    _getPictosPanel->Open_ForPictosIds(pendingPictosIds);
}

void GameplayUI_World::Try_ShowPendingGetChroma()
{
    if (_getChromaPanel == nullptr || _getChromaPanel->Is_Open())
        return;

    if (Is_GetCharacterOpen() || Is_GetClothesOpen() || Is_GetWeaponOpen() || Is_GetPictosOpen())
        return;

    if (CLIENT->Is_FadeTransitionPlaying())
        return;

    if (CLIENT->Is_WorldControlSuspended() || CLIENT->Is_BattleEntering())
        return;

    if (GAME->Is_WorldGameplayInputBlocked())
        return;

    if (!_menuStack.empty())
        return;

    if (_savePointPanel && (_savePointPanel->Is_Open() || _savePointPanel->Is_ClosingWithOutro()))
        return;

    if (_expeditionStatPanel && _expeditionStatPanel->Is_Open())
        return;

    vector<uint32> pendingChromaAmounts = std::move(_pendingGetChromaAmounts);
    _pendingGetChromaAmounts.clear();

    if (pendingChromaAmounts.empty())
        return;

    _getChromaPanel->Open_ForChromaAmounts(pendingChromaAmounts);
}

bool GameplayUI_World::Is_GetClothesOpen() const
{
    return _getClothesPanel && _getClothesPanel->Is_Open();
}

bool GameplayUI_World::Is_GetWeaponOpen() const
{
    return _getWeaponPanel && _getWeaponPanel->Is_Open();
}

bool GameplayUI_World::Is_GetPictosOpen() const
{
    return _getPictosPanel && _getPictosPanel->Is_Open();
}

bool GameplayUI_World::Is_GetChromaOpen() const
{
    return _getChromaPanel && _getChromaPanel->Is_Open();
}

void GameplayUI_World::Update_CrossHair(float timeDelta)
{
    if (_crossHair == nullptr)
        return;

    if (!Is_WorldFreeAimActive())
    {
        _crossHair->Close();
        return;
    }

    const Shared<Player_WorldBase> activeHero = CLIENT->Get_ActiveHero();
    if (activeHero == nullptr)
    {
        _crossHair->Close();
        return;
    }

    const Shared<PlayerStateMachineCom> stateCom = activeHero->Get_Component<PlayerStateMachineCom>();
    if (stateCom == nullptr)
    {
        _crossHair->Close();
        return;
    }

    const Shared<Player_FreeAim> freeAimState =
        dynamic_pointer_cast<Player_FreeAim>(stateCom->Get_State(PlayerState::FreeAim));
    if (freeAimState == nullptr)
    {
        _crossHair->Close();
        return;
    }

    _crossHair->Open();
    _crossHair->Set_HitState(freeAimState->Is_HitObject());
    _crossHair->Update(timeDelta);
}

void GameplayUI_World::Update_InteractionPrompt()
{
    if (_interactionPrompt == nullptr)
        return;

    Vec3 worldAnchor{};
    if (Is_AnyMenuOpen() ||
        CLIENT->IsDialogueActive() ||
        CLIENT->IsDialogueInteractLocked() ||
        !CLIENT->Try_GetInteractionPromptAnchor(worldAnchor))
    {
        _interactionPrompt->Clear_Target();
        return;
    }

    _interactionPrompt->Show_AtWorldPosition(worldAnchor);
}

void GameplayUI_World::Push_Menu(GameplayMenu menu)
{
    if (menu == GameplayMenu::None)
        return;

    const GameplayMenu currentMenu = Get_CurrentMenu();
    if (currentMenu == menu)
        return;

    const auto applyMenuChange = [this, currentMenu, menu]()
        {
            if (currentMenu != GameplayMenu::None)
            {
                if (currentMenu == GameplayMenu::SavePoint && _savePointPanel)
                    _savePointPanel->Close_ForMenuTransition();
                else
                    Close_Menu(currentMenu, false); // 같은 메뉴 흐름 안 이동은 월드 입력 유지
            }

            _menuStack.push_back(menu);
            Open_Menu(menu);
        };

    _menuFade.Request_MenuTransition(currentMenu, menu, applyMenuChange);
}

void GameplayUI_World::Pop_Menu()
{
    if (_menuStack.empty())
        return;

    const GameplayMenu currentMenu = _menuStack.back();
    const GameplayMenu previousMenu = _menuStack.size() >= 2 ? _menuStack[_menuStack.size() - 2] : GameplayMenu::None;

    const auto applyMenuChange = [this, currentMenu]()
        {
            Close_Menu(currentMenu, false); // 뒤에 다시 열 메뉴가 있으므로 월드 입력 복구 금지
            _menuStack.pop_back();

            if (_menuStack.empty())
                return;

            Open_Menu(_menuStack.back());
        };

    _menuFade.Request_MenuTransition(currentMenu, previousMenu, applyMenuChange);
}

void GameplayUI_World::Close_AllMenus(bool immediate)
{
    while (!_menuStack.empty())
    {
        const GameplayMenu menu = _menuStack.back();

        if (immediate && menu == GameplayMenu::SavePoint && _savePointPanel)
            _savePointPanel->Close_Immediately();
        else if (immediate && menu == GameplayMenu::CharacterProgression && _characterProgressionPanel)
            _characterProgressionPanel->Close_Immediately();
        else if (immediate && menu == GameplayMenu::ChangeSkill && _changeSkillPanel)
            _changeSkillPanel->Close_Immediately();
        else if (immediate && menu == GameplayMenu::Wardrobe && _wardrobePanel)
            _wardrobePanel->Close_Immediately();
        else if (immediate && menu == GameplayMenu::WeaponUpgrade && _upgradeWeaponPanel)
            _upgradeWeaponPanel->Close_Immediately();
        else if (immediate && menu == GameplayMenu::Merchant && _merchantPanel)
            _merchantPanel->Close_Immediately();
        else if (immediate && menu == GameplayMenu::ArenaCharacterSelect && _arenaCharacterSelectPanel)
            _arenaCharacterSelectPanel->Close_Immediately();
        else if (immediate && menu == GameplayMenu::Replay && _replayPanel)
            _replayPanel->Close_Immediately();
        else
            Close_Menu(menu, true);

        _menuStack.pop_back();
    }

    if (immediate &&
        _savePointPanel &&
        (_savePointPanel->Is_Open() || _savePointPanel->Is_ClosingWithOutro()))
    {
        _savePointPanel->Close_Immediately();
    }

    if (_expeditionStatPanel && _expeditionStatPanel->Is_Open())
        _expeditionStatPanel->Close_Immediately();
}

void GameplayUI_World::Open_Menu(GameplayMenu menu)
{
    switch (menu)
    {
    case GameplayMenu::CharacterProgression:
        if (_characterProgressionPanel)
            _characterProgressionPanel->Open();
        break;

    case GameplayMenu::ChangeSkill:
        if (_changeSkillPanel)
            _changeSkillPanel->Open();
        break;

    case GameplayMenu::Wardrobe:
        if (_wardrobePanel)
            _wardrobePanel->Open();
        break;


    case GameplayMenu::WeaponUpgrade:
        if (_characterProgressionPanel)
            _characterProgressionPanel->Close_ForMenuTransition(); // Weapon 화면에서는 Progression의 기술/시트/픽토스 UI를 모두 숨긴다.
        if (_changeSkillPanel)
            _changeSkillPanel->Close_ForMenuTransition(); // ChangeSkill에서 넘어온 경우 Available/Current Skill UI를 정리한다.
        if (_upgradeWeaponPanel)
            _upgradeWeaponPanel->Open();
        break;

    case GameplayMenu::Merchant:
        if (_merchantPanel)
            _merchantPanel->Open();
        break;

    case GameplayMenu::SavePoint:
        if (_savePointPanel)
            _savePointPanel->Open();
        break;

    case GameplayMenu::CharacterStatus:
        if (_characterStatusPanel)
            _characterStatusPanel->Open();
        break;

    case GameplayMenu::SkillTree:
        if (_skillTreePanel)
            _skillTreePanel->Open();
        break;

    case GameplayMenu::ArenaCharacterSelect:
        if (_arenaCharacterSelectPanel)
            _arenaCharacterSelectPanel->Open();
        break;

    case GameplayMenu::Replay:
        if (_replayPanel)
            _replayPanel->Open();
        break;

    case GameplayMenu::None:
    default:
        break;
    }
}

bool GameplayUI_World::Is_WorldFreeAimActive() const
{
    // 배틀 중 아닐 때.
    if (CLIENT->Is_WorldControlSuspended() || CLIENT->Is_BattleEntering())
        return false;

    // 월드 입력이 막혀 있으면 조준 UI도 같이 숨긴다.
    if (GAME->Is_WorldGameplayInputBlocked())
        return false;

    // 월드 메뉴가 열려 있으면 조준 UI를 덮지 않는다.
    if (!_menuStack.empty())
        return false;

    if (_expeditionStatPanel && _expeditionStatPanel->Is_Open())
        return false;

    const Shared<Player_WorldBase> activeHero = CLIENT->Get_ActiveHero();
    if (activeHero == nullptr)
        return false;

    const Shared<PlayerStateMachineCom> stateCom = activeHero->Get_Component<PlayerStateMachineCom>();
    if (stateCom == nullptr)
        return false;

    return stateCom->Get_CurrentPlayerState() == PlayerState::FreeAim;
}

Unique<GameplayUI_World> GameplayUI_World::Create(uint32 levelIndex)
{
    auto instance = make_unique<GameplayUI_World>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : GameplayUI_World");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void GameplayUI_World::Free()
{
    auto Release_Panel = [](auto& panel)
        {
            if (panel)
            {
                panel->Free();
                panel.reset();
            }
        };

    Release_Panel(_crossHair);
    Release_Panel(_characterProgressionPanel);
    Release_Panel(_changeSkillPanel);
    Release_Panel(_savePointPanel);
    Release_Panel(_wardrobePanel);
    Release_Panel(_pictosPanel);
    Release_Panel(_characterStatusPanel);
    Release_Panel(_skillTreePanel);
    Release_Panel(_changeWeaponPanel);
    Release_Panel(_upgradeWeaponPanel);
    Release_Panel(_merchantPanel);
    Release_Panel(_expeditionStatPanel);
    Release_Panel(_getCharacterPanel);
    Release_Panel(_getClothesPanel);
    Release_Panel(_getWeaponPanel);
    Release_Panel(_getPictosPanel);
    Release_Panel(_getChromaPanel);
    Release_Panel(_arenaCharacterSelectPanel);
    Release_Panel(_replayPanel);
    Release_Panel(_gradientSkillPreviewTestPanel);

    Base::Free();
}
