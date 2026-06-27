#include "pch.h"
#include "Client_Constants.h"
#include "ClientInstance.h"
#include "BattleController.h"
#include "BT_Node_Bootstrap.h"
#include "ComputeSpriteEmitter.h"
#include "EffectCom.h"
#include "EffectEmitterRegistration.h"
#include "EffectInstancePool.h"
#include "GameDataTable.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "GameplayUI.h"
#include "Hero_Manager.h"
#include "Level_Loading.h"
#include "LevelTransitionFade.h"
#include "PersistentObjectManager.h"
#include "Player_WorldBase.h"
#include "ResourceLoader.h"
#include "ScreenFadeTransition.h"
#include "PreviewShotTable.h"
#include "UIPreviewManager.h"
#include "CameraDirector.h"
#include "MapLoader.h"
#include "ClientScreenFxManager.h"
#include "CinematicCamera.h"
#include "CinematicSequenceRunner.h"
#include "PlayerInventory.h"
#include "CostumeManager.h"
#include "DialogueDB.h"
#include "PlayerStatCom.h"
#include "SoulLikeController.h"
#include "TargetCamera.h"
#include "BattleTurnCamera.h"
#include "BattleEncounterProfile.h"
#include "CameraPresetIO.h"
#include "Unlock_Manager.h"
#include "DialogueDB.h"
#include "DialogueManager.h"
#include "DialogueCamera.h"
#include "BattleFollowupManager.h"
#include "Client_Constants.h"
#include "Monster_WorldBase.h"

IMPLEMENT_SINGLETON(ClientInstance)

namespace
{
    bool g_debugDisableDynamicShadowRenderPass = false;
    bool g_debugDisableMapInstancedMeshRender = false;
    bool g_debugDisableGrassInstancedRender = false;
    bool g_debugDisableMapStaticMeshRender = false;
    bool g_debugDisableMapInstancedMeshShadow = false;
    bool g_debugUseMapInstancedMeshShadowLOD = false;
    bool g_debugDisableGrassInstancedShadow = false;
    bool g_debugDisableMapStaticMeshShadow = false;

    string Get_WeaponCharacterId(HeroId heroId)
    {
        switch (heroId)
        {
        case HeroId::Gustave:
            return "Gustave";
        case HeroId::Lune:
            return "Lune";
        case HeroId::Maelle:
            return "Maelle";
        default:
            return "";
        }
    }

    string Get_DefaultEquippedWeaponId(HeroId heroId)
    {
        switch (heroId)
        {
        case HeroId::Gustave:
            return "Lanceram";
        case HeroId::Lune:
            return "Colim";
        case HeroId::Maelle:
            return "Medalum";
        default:
            return "";
        }
    }
}

ClientInstance::ClientInstance()
{
}

ClientInstance::~ClientInstance() = default;

HRESULT ClientInstance::Initialize_Client(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    if (_isInitialized)
        return S_OK;

    _device = device;
    _context = context;

    // 프로토타입 등록
    if (FAILED(Ready_ResourceLoader()))
        return E_FAIL;

    // Effect emitter 등록
    Register_ClientEffectEmitters();

    // 게임 플레이 데이터
    if (FAILED(Ready_GameDataTables()))
        return E_FAIL;

    Client::Force_Register_ClientBTNodes();

    _persistentObjectManager = PersistentObjectManager::Create();
    CHECK_NULL(_persistentObjectManager, E_FAIL);

    _heroManager = Hero_Manager::Create();
    CHECK_NULL(_heroManager, E_FAIL);

    _battleController = BattleController::Create();
    CHECK_NULL(_battleController, E_FAIL);

    _previewShotTable = PreviewShotTable::Create();
    CHECK_NULL(_previewShotTable, E_FAIL);

    _previewManager = UIPreviewManager::Create();
    CHECK_NULL(_previewManager, E_FAIL);

    _cameraDirector = CameraDirector::Create();
    CHECK_NULL(_cameraDirector, E_FAIL);
    _effectInstancePool = EffectInstancePool::Create();
    CHECK_NULL(_effectInstancePool, E_FAIL);

    _screenFxManager = ClientScreenFxManager::Create();
    CHECK_NULL(_screenFxManager, E_FAIL);

    _screenFadeTransition = ScreenFadeTransition::Create();
    if (_screenFadeTransition == nullptr)
        LOG_WARN("ScreenFadeTransition is not available. Screen fade requests will run immediately.");

    _levelTransitionFade = LevelTransitionFade::Create(_device, _context);
    if (_levelTransitionFade == nullptr)
        LOG_WARN("LevelTransitionFade is not available. Level transition requests will fall back to immediate changes.");

    _cinematicSequenceRunner = CinematicSequenceRunner::Create();
    CHECK_NULL(_cinematicSequenceRunner, E_FAIL);
    
    _playerInventory = PlayerInventory::Create();
    CHECK_NULL(_playerInventory, E_FAIL);

    // 일반 픽토스 일단은 전부 다 등록. 나중에 몬스터 잡기? 상점? 등에서 뭐 구매하게 만들 수 있으려나
    _playerInventory->Ensure_DefaultUnlockedPictos(Get_AllPictosIds());

    // 무기 세팅
    vector<PlayerInventory::WeaponInventoryEntry> defaultUnlockedWeapons{};
    const HeroId defaultWeaponHeroes[] = { HeroId::Gustave, HeroId::Lune, HeroId::Maelle };
    for (HeroId heroId : defaultWeaponHeroes)
    {
        const string characterId = Get_WeaponCharacterId(heroId);
        const string weaponId = Get_DefaultEquippedWeaponId(heroId);
        if (characterId.empty() || weaponId.empty())
            continue;

        const GameDataTable::WeaponDesc* weapon = Get_WeaponDesc(characterId, weaponId);

        PlayerInventory::WeaponInventoryEntry entry{};
        entry.characterId = weapon ? weapon->characterId : characterId;
        entry.weaponId = weapon ? weapon->weaponId : weaponId;
        entry.enhanceLevel = weapon ? weapon->level : 1;
        entry.enhanceExp = 0;
        defaultUnlockedWeapons.emplace_back(std::move(entry));
    }
    _playerInventory->Ensure_DefaultUnlockedWeapons(defaultUnlockedWeapons);

    _costumeManager = CostumeManager::Create();
    CHECK_NULL(_costumeManager, E_FAIL);

    _unlockManager = Unlock_Manager::Create();
    CHECK_NULL(_unlockManager, E_FAIL);

    _dialogueDB = DialogueDB::Create();
    CHECK_NULL(_dialogueDB, E_FAIL);

   _dialogueDB->LoadFromFile(String::ToString(Client::Constants::Dialogue::Dialogue_Dir));

   _dialogueManager = DialogueManager::Create();
    CHECK_NULL(_dialogueManager, E_FAIL);
    
    _battleFollowupManager = BattleFollowupManager::Create();
    CHECK_NULL(_battleFollowupManager, E_FAIL);

	_isInitialized = true;

	return S_OK;
}

HRESULT ClientInstance::Initialize_Client_ForEffectEditor(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    if (_isInitialized)
        return S_OK;

    _device = device;
    _context = context;

    CHECK_FAILED(Ready_ResourceLoader(), E_FAIL);

    Register_ClientEffectEmitters();

    _effectInstancePool = EffectInstancePool::Create();
    CHECK_NULL(_effectInstancePool, E_FAIL);

    _screenFadeTransition = ScreenFadeTransition::Create();
    if (_screenFadeTransition == nullptr)
        LOG_WARN("ScreenFadeTransition is not available. Screen fade requests will run immediately.");

    _levelTransitionFade = LevelTransitionFade::Create(_device, _context);
    if (_levelTransitionFade == nullptr)
        LOG_WARN("LevelTransitionFade is not available. Level transition requests will fall back to immediate changes.");

    _isInitialized = true;

    return S_OK;
}

void ClientInstance::Update_Client(float timeDelta)
{
    Update_FadeTransitions(timeDelta);
    Capture_ReplayMonsterSnapshotIfNeeded();

    if (_heroManager)
        _heroManager->Update_HeroManager(timeDelta);

    if (_battleController)
        _battleController->Update(timeDelta);

    if (_gamePlayUI)
        _gamePlayUI->Update(timeDelta);

    if (_previewManager)
        _previewManager->Update(timeDelta);

    if (_effectInstancePool)
        _effectInstancePool->Release_Finished();

    if(_cinematicSequenceRunner)
        _cinematicSequenceRunner->Update(timeDelta, true);

    if (_dialogueManager)
    {
        if (Is_BattleActive() || Is_BattleEntering() || Is_SoulLikeBattleActive())
        {
            if (_dialogueManager->IsActive())
                _dialogueManager->EndDialogue(false);
        }
        else
        {
            _dialogueManager->Update();
        }
    }
}

void ClientInstance::Update_FadeTransitions(float timeDelta)
{
    if (_screenFadeTransition)
        _screenFadeTransition->Update(timeDelta);

    if (_levelTransitionFade)
        _levelTransitionFade->Update(timeDelta);
}

void ClientInstance::Update_UIPreviewOnly(float timeDelta)
{
    if (_previewManager)
        _previewManager->Update(timeDelta);
}

bool ClientInstance::Is_DebugDisableMapInstancedMeshRender()
{
    return g_debugDisableMapInstancedMeshRender;
}

bool ClientInstance::Is_DebugDisableDynamicShadowRenderPass()
{
    return g_debugDisableDynamicShadowRenderPass;
}

void ClientInstance::Set_DebugDisableDynamicShadowRenderPass(bool disabled)
{
    g_debugDisableDynamicShadowRenderPass = disabled;
    GAME->Set_ShadowEnabled(!disabled);
}

void ClientInstance::Set_DebugDisableMapInstancedMeshRender(bool disabled)
{
    g_debugDisableMapInstancedMeshRender = disabled;
}

bool ClientInstance::Is_DebugDisableGrassInstancedRender()
{
    return g_debugDisableGrassInstancedRender;
}

void ClientInstance::Set_DebugDisableGrassInstancedRender(bool disabled)
{
    g_debugDisableGrassInstancedRender = disabled;
}

bool ClientInstance::Is_DebugDisableMapStaticMeshRender()
{
    return g_debugDisableMapStaticMeshRender;
}

void ClientInstance::Set_DebugDisableMapStaticMeshRender(bool disabled)
{
    g_debugDisableMapStaticMeshRender = disabled;
}

bool ClientInstance::Is_DebugDisableMapInstancedMeshShadow()
{
    return g_debugDisableMapInstancedMeshShadow;
}

void ClientInstance::Set_DebugDisableMapInstancedMeshShadow(bool disabled)
{
    g_debugDisableMapInstancedMeshShadow = disabled;
}

bool ClientInstance::Is_DebugUseMapInstancedMeshShadowLOD()
{
    return g_debugUseMapInstancedMeshShadowLOD;
}

void ClientInstance::Set_DebugUseMapInstancedMeshShadowLOD(bool enabled)
{
    g_debugUseMapInstancedMeshShadowLOD = enabled;
}

bool ClientInstance::Is_DebugDisableGrassInstancedShadow()
{
    return g_debugDisableGrassInstancedShadow;
}

void ClientInstance::Set_DebugDisableGrassInstancedShadow(bool disabled)
{
    g_debugDisableGrassInstancedShadow = disabled;
}

bool ClientInstance::Is_DebugDisableMapStaticMeshShadow()
{
    return g_debugDisableMapStaticMeshShadow;
}

void ClientInstance::Set_DebugDisableMapStaticMeshShadow(bool disabled)
{
    g_debugDisableMapStaticMeshShadow = disabled;
}

HRESULT ClientInstance::Ready_StaticResources()
{
    CHECK_NULL(_resourceLoader, E_FAIL);

    if (FAILED(_resourceLoader->Load_LevelTables(LevelType::Static)))
        return E_FAIL;

    return S_OK;
}

HRESULT ClientInstance::Ready_LevelResources(LevelType levelType)
{
    CHECK_NULL(_resourceLoader, E_FAIL);

    if (FAILED(_resourceLoader->Load_LevelTables(levelType)))
        return E_FAIL;

    return S_OK;
}

Shared<ResourceLoader> ClientInstance::Get_ResourceLoader() const
{
    return _resourceLoader;
}

uint32 ClientInstance::Get_NextExp(uint32 level) const
{
    if (_gameDataTable == nullptr)
        return 0;

    return _gameDataTable->Get_NextExp(level);
}

GameDataTable::SkillTreeDesc* ClientInstance::Get_SkillTreeDesc(const string& skillName)
{
    return _gameDataTable->Get_SkillTreeDesc(skillName);
}

const umap<string, vector<string>>*  ClientInstance::Get_SkillTreeRequirementData()
{
    return _gameDataTable->Get_SkillTreeRequirementData();
}

const GameDataTable::WeaponDesc* ClientInstance::Get_WeaponDesc(const string& characterId, const string& weaponId) const
{
    if (_gameDataTable == nullptr)
        return nullptr;

    return _gameDataTable->Get_WeaponDesc(characterId, weaponId);
}

const vector<GameDataTable::WeaponDesc>* ClientInstance::Get_WeaponDescs(const string& characterId) const
{
    if (_gameDataTable == nullptr)
        return nullptr;

    return _gameDataTable->Get_WeaponDescs(characterId);
}

const Skill_Data* ClientInstance::Get_Skill(const string& skillId)
{
    return _gameDataTable->Get_Skill(skillId);
}

const vector<string> ClientInstance::Get_AllSkillIds()
{
    return _gameDataTable->Get_AllSkillIds();
}

HRESULT ClientInstance::Reload_GameDataTables()
{
    if (_gameDataTable == nullptr)
        return Ready_GameDataTables();

    return _gameDataTable->Load_All();
}

HRESULT ClientInstance::Reload_CameraPresetData()
{
    if (auto targetCamera = dynamic_pointer_cast<TargetCamera>(GAME->Get_CameraByType(ObjectType::TargetCamera)))
    {
        TargetCameraPreset preset{};

        if (CameraPresetIO::LoadTargetPresetFromFile(TargetCameraMode::NORMAL, preset))
            targetCamera->Set_Preset(TargetCameraMode::NORMAL, preset);

        if (CameraPresetIO::LoadTargetPresetFromFile(TargetCameraMode::AIM, preset))
            targetCamera->Set_Preset(TargetCameraMode::AIM, preset);

        if (CameraPresetIO::LoadTargetPresetFromFile(TargetCameraMode::BATTLE_ENTER, preset))
            targetCamera->Set_Preset(TargetCameraMode::BATTLE_ENTER, preset);

        if (CameraPresetIO::LoadTargetPresetFromFile(TargetCameraMode::SOULLIKE, preset))
            targetCamera->Set_Preset(TargetCameraMode::SOULLIKE, preset);

        targetCamera->Change_Mode(targetCamera->Get_CurrentMode());
    }

    if (auto battleTurnCamera = dynamic_pointer_cast<BattleTurnCamera>(GAME->Get_CameraByType(ObjectType::BattleTurnCamera)))
        battleTurnCamera->Reload_BattleCameraAuthoringData();

    if (auto dialogueCamera = dynamic_pointer_cast<DialogueCamera>(GAME->Get_CameraByType(ObjectType::DialogueCamera)))
        dialogueCamera->Set_CameraPresetId(dialogueCamera->Get_CameraPresetId());

    return S_OK;
}

const Item_Data* ClientInstance::Get_Item(const string& itemId)
{
    if (!_gameDataTable)
        return nullptr;

    return _gameDataTable->Get_Item(itemId);
}

const Item_Data* ClientInstance::Get_Item(BattleItemType itemType)
{
     if (!_gameDataTable)
        return nullptr;

    return _gameDataTable->Get_Item(itemType);
}

bool ClientInstance::Consume_Item(BattleItemType itemType)
{
    if (_gameDataTable == nullptr)
        return false;

    return _gameDataTable->Consume_Item(itemType);
}

const GameDataTable::PictosDesc* ClientInstance::Get_PictosDesc(const string& pictosId) const
{
    if (_gameDataTable == nullptr)
        return nullptr;

    return _gameDataTable->Get_PictosDesc(pictosId);
}

const vector<GameDataTable::PictosDesc>& ClientInstance::Get_PictosDescs() const
{
    static const vector<GameDataTable::PictosDesc> empty{};
    if (_gameDataTable == nullptr)
        return empty;

    return _gameDataTable->Get_PictosDescs();
}

const vector<string> ClientInstance::Get_AllPictosIds() const
{
    if (_gameDataTable == nullptr)
        return {};

    return _gameDataTable->Get_AllPictosIds();
}

void ClientInstance::Apply_PreviewLayout(const vector<UIPreviewManager::PreviewSlotDesc>& layout)
{
    return _previewManager->Apply_Layout(layout);
}

void ClientInstance::Begin_Preview(PreviewShotPanel panel)
{
    return _previewManager->Begin_Preview(panel);
}

void ClientInstance::Show_Preview(HeroId heroId, PreviewShotPanel panel)
{
    return _previewManager->Show_Preview(heroId, panel);
}

void ClientInstance::Hide_Preview()
{
    if (_previewManager == nullptr)
    {
        return;
    }

    return _previewManager->Hide_Preview();
}

PreviewShotPreset ClientInstance::Get_Preset(HeroId hero, PreviewShotPanel panel)
{
    return _previewShotTable->Get(hero, panel);
}

HRESULT ClientInstance::Initialize_Hero(uint32 levelIndex, const wstring& layerTag, const Vec3& startPos, const Vec3& startRot)
{
    return _heroManager->Initialize_Hero(levelIndex, layerTag, startPos, startRot);
}

void ClientInstance::Update_HeroManager(float timeDelta)
{
    return _heroManager->Update_HeroManager(timeDelta);
}

Shared<Player_WorldBase> ClientInstance::Get_ActiveHero() const
{
    return _heroManager->Get_ActiveHero();
}

const vector<HeroPartyEntry>& ClientInstance::Get_HeroEntries() const
{
    return _heroManager->Get_HeroEntries();
}

Shared<Player_WorldBase> ClientInstance::Get_Hero(HeroPartySlot slot) const
{
    return _heroManager->Get_Hero(slot);
}

HeroId ClientInstance::Get_HeroId(HeroPartySlot slot) const
{
    return _heroManager->Get_HeroId(slot);
}

HeroPartySlot ClientInstance::Get_ActiveHeroSlot() const
{
    return _heroManager->Get_ActiveHeroSlot();
}

void ClientInstance::Set_ActiveHeroSlotForCheckpoint(HeroPartySlot slot)
{
    if (_heroManager == nullptr)
        return;

    _heroManager->Set_ActiveHeroSlotForCheckpoint(slot);
}

void ClientInstance::Set_DialogueFocusActive(bool active)
{
    if (_heroManager == nullptr)
        return;

    _heroManager->Set_DialogueFocusActive(active);
}

void ClientInstance::Set_TargetCamera(const Shared<TargetCamera>& camera)
{
    return _heroManager->Set_TargetCamera(camera);
}

void ClientInstance::Clear_Party()
{
    return _heroManager->Clear_Party();
}

Shared<Player_WorldBase> ClientInstance::Get_FrontHero() const
{
    return _heroManager->Get_FrontHero();
}

Shared<Player_WorldBase> ClientInstance::Find_Hero(HeroId heroId) const
{
    return _heroManager->Find_Hero(heroId);
}

const HeroSpawnDesc* ClientInstance::Find_HeroSpawnDesc(HeroId heroId)
{
    return _heroManager->Find_HeroSpawnDesc(heroId);
}

bool ClientInstance::Ensure_DefaultEquippedWeapon(HeroId heroId)
{
    const Shared<Player_WorldBase> hero = Find_Hero(heroId);
    const Shared<PlayerStatCom> statCom = hero ? hero->Get_Component<PlayerStatCom>() : nullptr;
    if (statCom == nullptr)
        return false;

    if (!statCom->Get_PlayerRuntimeStat().CurEquippedWeaponId.empty())
        return false;

    const string characterId = Get_WeaponCharacterId(heroId);
    const string weaponId = Get_DefaultEquippedWeaponId(heroId);
    if (characterId.empty() || weaponId.empty())
        return false;

    const auto inventory = Get_PlayerInventory();
    if (inventory && !inventory->Is_WeaponUnlocked(characterId, weaponId))
        return false;

    const GameDataTable::WeaponDesc* weaponDesc = Get_WeaponDesc(characterId, weaponId);
    if (weaponDesc == nullptr)
        return false;

    statCom->Set_EquippedWeapon(weaponDesc->weaponId, weaponDesc->power);
    return true;
}

void ClientInstance::Reset_AllHeroesToIdle()
{
    return _heroManager->Reset_AllHeroesToIdle();
}

void ClientInstance::Restore_PartyFormationAfterEditorStop()
{
    return _heroManager->Restore_PartyFormationAfterEditorStop();
}

bool ClientInstance::Is_ActiveHero(const Shared<GameObject>& target) const
{
    return _heroManager->Is_ActiveHero(target);
}

HRESULT ClientInstance::Initialize_GameplayUI(uint32 levelIndex)
{
    Clear_GameplayUI();

    _gamePlayUI = GameplayUI::Create(levelIndex);
    CHECK_NULL(_gamePlayUI, E_FAIL);

    return S_OK;
}

void ClientInstance::Clear_GameplayUI()
{
    if (_gamePlayUI)
    {
        _gamePlayUI->Free();
        _gamePlayUI.reset();
    }

    GAME->Set_GameplayPaused(false);
}

void ClientInstance::Play_BattleEnterTransition()
{
    if (_gamePlayUI)
        return _gamePlayUI->Play_BattleEnterTransition();
}

void ClientInstance::Stop_BattleEnterTransition()
{
    if (_gamePlayUI)
        return _gamePlayUI->Stop_BattleEnterTransition();
}

bool ClientInstance::Is_BattleEnterTransitionPlaying() const
{
    return _gamePlayUI && _gamePlayUI->Is_BattleEnterTransitionPlaying();
}

bool ClientInstance::Request_ScreenFadeTransition(const function<void()>& CoveredAction)
{
    if (_levelTransitionFade && _levelTransitionFade->Is_Playing())
        return false;

    if (_screenFadeTransition == nullptr)
    {
        if (CoveredAction)
            CoveredAction();
        return true;
    }

    return _screenFadeTransition->Request_Transition(CoveredAction);
}

bool ClientInstance::Request_LevelTransition(LevelType TargetLevel)
{
    if (_screenFadeTransition && _screenFadeTransition->Is_Playing())
        return false;

    if (_levelTransitionFade)
        return _levelTransitionFade->Request_LevelTransition(TargetLevel);

    Shared<Level_Loading> loadingLevel = Level_Loading::Create(_device.Get(), _context.Get(), TargetLevel);
    if (loadingLevel == nullptr)
        return false;

    return SUCCEEDED(GAME->Change_Level(ETOI(LevelType::Loading), loadingLevel));
}

bool ClientInstance::Request_LoadedLevelTransition(LevelType TargetLevel, Shared<Level> TargetLevelInstance)
{
    if (_screenFadeTransition && _screenFadeTransition->Is_Playing())
        return false;

    if (_levelTransitionFade)
        return _levelTransitionFade->Request_LoadedLevelTransition(TargetLevel, TargetLevelInstance);

    if (TargetLevelInstance == nullptr)
        return false;

    return SUCCEEDED(GAME->Change_Level(ETOI(TargetLevel), TargetLevelInstance));
}

bool ClientInstance::Request_CurrentLevelReplay()
{
    if (_screenFadeTransition && _screenFadeTransition->Is_Playing())
        return false;

    if (_levelTransitionFade && _levelTransitionFade->Is_Playing())
        return false;

    if (Is_BattleActive() || Is_BattleEntering() || Is_SoulLikeBattleActive())
        return false;

    return Request_ScreenFadeTransition(
        [this]()
        {
            Reset_CurrentLevelRuntimeState();
        });
}

void ClientInstance::Reset_CurrentLevelRuntimeState()
{
    if (_dialogueManager && _dialogueManager->IsActive())
        _dialogueManager->EndDialogue(false);

    vector<Shared<GameObject>> duplicatePlayers{};

    GAME->Visit_CurrentLevelObjects(
        [this, &duplicatePlayers](const wstring&, const Shared<GameObject>& gameObject)
        {
            const Shared<Player_WorldBase> player =
                dynamic_pointer_cast<Player_WorldBase>(gameObject);
            if (player == nullptr)
                return;

            bool managed = false;
            if (_heroManager)
            {
                for (const HeroPartyEntry& entry : _heroManager->Get_HeroEntries())
                {
                    if (entry.hero == player)
                    {
                        managed = true;
                        break;
                    }
                }
            }

            if (!managed)
                duplicatePlayers.push_back(gameObject);
        });

    for (const Shared<GameObject>& player : duplicatePlayers)
        GAME->Remove_GameObject(GAME->Current_LevelIndex(), player);

    if (!duplicatePlayers.empty())
        GAME->Flush_RemovedGameObjects();

    if (_heroManager)
        _heroManager->Reset_RuntimeToStart();

    Respawn_ReplayMonstersFromSnapshot();

    GAME->Set_GameplayInputMode(GameplayInputMode::Normal);
    GAME->Set_PlayerInputBlocked(false);
    GAME->Lock_Mouse();
}

void ClientInstance::Capture_ReplayMonsterSnapshotIfNeeded()
{
    const uint32 levelIndex = GAME->Current_LevelIndex();
    if (_replayMonsterSnapshotLevelIndex != levelIndex)
    {
        _replayMonsterSnapshotLevelIndex = levelIndex;
        _replayMonsterSpawnObjects.clear();
        _replayMonsterRuntimeAuthoringLoaded.clear();
        _replayMonsterSnapshotCaptured = false;
    }

    if (_replayMonsterSnapshotCaptured)
        return;

    GAME->Visit_CurrentLevelObjects(
        [this](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            if (dynamic_pointer_cast<Monster_WorldBase>(gameObject) == nullptr)
                return;

            RuntimeCheckpointIO::WorldObject object{};
            object.layerTag = layerTag;
            object.objectJson = gameObject->To_Json();
            _replayMonsterSpawnObjects.emplace_back(std::move(object));
            _replayMonsterRuntimeAuthoringLoaded.emplace_back(gameObject->Is_RuntimeAuthoringLoaded() ? 1 : 0);
        });

    _replayMonsterSnapshotCaptured = true;
}

void ClientInstance::Respawn_ReplayMonstersFromSnapshot()
{
    Capture_ReplayMonsterSnapshotIfNeeded();

    if (_replayMonsterSpawnObjects.empty())
        return;

    const uint32 levelIndex = GAME->Current_LevelIndex();
    vector<Shared<GameObject>> monstersToRemove{};

    GAME->Visit_CurrentLevelObjects(
        [&monstersToRemove](const wstring&, const Shared<GameObject>& gameObject)
        {
            if (dynamic_pointer_cast<Monster_WorldBase>(gameObject) != nullptr)
                monstersToRemove.emplace_back(gameObject);
        });

    for (const Shared<GameObject>& monster : monstersToRemove)
        GAME->Remove_GameObject(levelIndex, monster);

    if (!monstersToRemove.empty())
        GAME->Flush_RemovedGameObjects();

    for (size_t index = 0; index < _replayMonsterSpawnObjects.size(); ++index)
    {
        const RuntimeCheckpointIO::WorldObject& object = _replayMonsterSpawnObjects[index];
        Shared<GameObject> gameObject = GAME->Instantiate_GameObjectJson(object.objectJson);
        if (gameObject == nullptr)
            continue;

        const bool runtimeAuthoringLoaded =
            index < _replayMonsterRuntimeAuthoringLoaded.size() &&
            _replayMonsterRuntimeAuthoringLoaded[index] != 0;
        gameObject->Set_RuntimeAuthoringLoaded(runtimeAuthoringLoaded);

        if (const Shared<Monster_WorldBase> worldMonster = dynamic_pointer_cast<Monster_WorldBase>(gameObject))
            worldMonster->Restore_AfterBattleExit();

        (void)GAME->Add_GameObject(levelIndex, object.layerTag, gameObject);
    }
}

void ClientInstance::Clear_ReplayMonsterSnapshot()
{
    _replayMonsterSpawnObjects.clear();
    _replayMonsterRuntimeAuthoringLoaded.clear();
    _replayMonsterSnapshotLevelIndex = UINT32_MAX;
    _replayMonsterSnapshotCaptured = false;
}

bool ClientInstance::Is_FadeTransitionPlaying() const
{
    const bool isScreenFadePlaying = _screenFadeTransition && _screenFadeTransition->Is_Playing();
    const bool isLevelFadePlaying = _levelTransitionFade && _levelTransitionFade->Is_Playing();

    return isScreenFadePlaying || isLevelFadePlaying;
}

void ClientInstance::OnEnterBattleField(uint32 FieldID, bool isPlayerAdvantage, const vector<HeroId>& heroIds,
                                        const vector<BattleMonsterSpawnDesc>& battleMonsterSpawnDescs, const Shared<GameObject>& sourceObject,
                                        bool completeImmediately, int32 battleStartSpawnerId, const Vec3& battleStartOffset)
{
    if (_dialogueManager && _dialogueManager->IsActive())
        _dialogueManager->EndDialogue(false);

    if (Should_ForcePlayerAdvantage(FieldID))
        isPlayerAdvantage = true;

    return _battleController->OnEnterBattleField(
        FieldID,
        isPlayerAdvantage,
        heroIds,
        battleMonsterSpawnDescs,
        sourceObject,
        completeImmediately,
        battleStartSpawnerId,
        battleStartOffset);
}

bool ClientInstance::Request_DialogueBattlePotatoBag()
{
    if (Is_BattleActive() || Is_BattleEntering())
        return false;

    BattleMonsterSpawnDesc spawnDesc{};
    spawnDesc.prefabName = Constants::Prefab::MB_PotatoBag;

    vector<BattleMonsterSpawnDesc> battleMonsterSpawnDescs{};
    battleMonsterSpawnDescs.emplace_back(move(spawnDesc));

    if (_screenFxManager)
        _screenFxManager->Play_BattleInCounterFx();

    GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_Encounter);

    OnEnterBattleField(
        Constants::Value::potatoBagEncounterId,
        false,
        Resolve_BattleEncounterHeroIds(Constants::Value::potatoBagEncounterId),
        battleMonsterSpawnDescs,
        nullptr,
        false,
        Constants::Value::potatoBagBattleStartSpawnerId,
        Vec3::Zero);

    return true;
}

bool ClientInstance::Is_BattleActive() const
{
    return _battleController && _battleController->Is_BattleStarted();
}

bool ClientInstance::Is_BattleEntering() const
{
    return _battleController && _battleController->Is_BattleEntering();
}

void ClientInstance::Request_GamePlay()
{
    return _cameraDirector->Request_GamePlay();
}

void ClientInstance::Request_SoulLikeCamera()
{
    return _cameraDirector->Request_SoulLikeCamera();
}

void ClientInstance::Request_Preview()
{
    return _cameraDirector->Request_Preview();
}

void ClientInstance::Request_BattleTurn()
{
    return _cameraDirector->Request_BattleTurn();
}

Shared<CinematicCamera> ClientInstance::Request_Cinematic()
{
    return _cameraDirector->Request_Cinematic();
}

Shared<DialogueCamera> ClientInstance::Request_DialogueCamera()
{
    return _cameraDirector->Request_DialogueCamera();
}

void ClientInstance::Request_TargetCameraMode(TargetCameraMode cameraMode)
{
    return _cameraDirector->Request_ChangeTargetCameraMode(cameraMode);
}

void ClientInstance::Restore_PreviousCamera()
{
    return _cameraDirector->Restore_PreviousCamera();
}

bool ClientInstance::Play_Sequence(
    const wstring& filePath,
    int trackIndex,
    bool applyCameraTrack,
    bool applyEventTrack,
    bool applyActorTransformTrack)
{
    return _cinematicSequenceRunner->Play_Sequence(
        filePath,
        trackIndex,
        applyCameraTrack,
        applyEventTrack,
        applyActorTransformTrack);
}

bool ClientInstance::Play_Sequence(
    const CINEMATIC_SEQUENCE& sequence,
    int trackIndex,
    bool applyCameraTrack,
    bool applyEventTrack,
    bool applyActorTransformTrack)
{
    return _cinematicSequenceRunner->Play_Sequence(
        sequence,
        trackIndex,
        applyCameraTrack,
        applyEventTrack,
        applyActorTransformTrack);
}

void ClientInstance::Stop_Sequence()
{
    return  _cinematicSequenceRunner->Stop_Sequence();
}

void ClientInstance::Stop_SequenceKeepContext()
{
    return _cinematicSequenceRunner->Stop_SequenceKeepContext();
}

void ClientInstance::Pause_Sequence()
{
    if (_cinematicSequenceRunner == nullptr)
        return;

    _cinematicSequenceRunner->Pause_Sequence();
}

void ClientInstance::Resume_Sequence(
    bool applyCameraTrack,
    bool applyEventTrack,
    bool applyActorTransformTrack)
{
    if (_cinematicSequenceRunner == nullptr)
        return;

    _cinematicSequenceRunner->Resume_Sequence(
        applyCameraTrack,
        applyEventTrack,
        applyActorTransformTrack);
}

void ClientInstance::Set_CurrentSequenceFrame(int frame)
{
    if (_cinematicSequenceRunner == nullptr)
        return;

    _cinematicSequenceRunner->Set_CurrentFrame(frame);
}

void ClientInstance::Refresh_CurrentSequenceContext()
{
    if (_cinematicSequenceRunner == nullptr)
        return;

    _cinematicSequenceRunner->Refresh_CurrentFrameContext();
}

void ClientInstance::Preview_CurrentSequenceFrame(
    int frame,
    bool applyCameraTrack,
    bool applyEventTrack,
    bool applyActorTransformTrack,
    bool forceRebuildContext)
{
    if (_cinematicSequenceRunner == nullptr)
        return;

    _cinematicSequenceRunner->Preview_CurrentFrame(
        frame,
        applyCameraTrack,
        applyEventTrack,
        applyActorTransformTrack,
        forceRebuildContext);
}

bool ClientInstance::Sync_CinematicPreviewSequence(
    const CINEMATIC_SEQUENCE& sequence,
    int trackIndex)
{
    if (_cinematicSequenceRunner == nullptr)
        return false;

    return _cinematicSequenceRunner->Sync_PreviewSequence(sequence, trackIndex);
}

void ClientInstance::Set_ActorTransformTrackPreviewSuspended(bool suspended)
{
    if (_cinematicSequenceRunner == nullptr)
        return;

    _cinematicSequenceRunner->Set_ActorTransformTrackPreviewSuspended(suspended);
}

bool ClientInstance::Is_CinematicPlaying() const
{
    return _cinematicSequenceRunner && _cinematicSequenceRunner->Is_CinematicPlaying();
}

bool ClientInstance::Is_CinematicPaused() const
{
    return _cinematicSequenceRunner && _cinematicSequenceRunner->Is_CinematicPaused();
}

int ClientInstance::Get_CurrentSequenceFrame() const
{
    if (_cinematicSequenceRunner == nullptr)
        return 0;

    return _cinematicSequenceRunner->Get_CurrentFrame();
}

Shared<GameObject> ClientInstance::Get_CinematicBoundActor(const string& targetId) const
{
    if (_cinematicSequenceRunner == nullptr)
        return nullptr;

    return _cinematicSequenceRunner->Get_BoundActor(targetId);
}

bool ClientInstance::Is_ActorTransformTrackPreviewSuspended() const
{
    return _cinematicSequenceRunner && _cinematicSequenceRunner->Is_ActorTransformTrackPreviewSuspended();
}

void ClientInstance::Update_CinematicPreview(float timeDelta)
{
    if (_cinematicSequenceRunner == nullptr)
        return;

    _cinematicSequenceRunner->Update(timeDelta, true);
}

FDelegateHandle ClientInstance::Add_CinematicCompletedCallback(const FOnCinematicCompleted::FunctionType& callback)
{
    if (_cinematicSequenceRunner == nullptr)
        return {};

    return _cinematicSequenceRunner->Add_CompletedCallback(callback);
}

void ClientInstance::Clear_CinematicCompletedCallbacks()
{
    if (_cinematicSequenceRunner == nullptr)
        return;

    _cinematicSequenceRunner->Clear_CompletedCallbacks(); 
}

Shared<CinematicCamera> ClientInstance::Ensure_CinematicCamera()
{
    if(auto camera = dynamic_pointer_cast<CinematicCamera>(GAME->Get_CameraByType(ObjectType::CinematicCamera)))
    {
        return camera;  
    }

    Shared<GameObject> gameObject;

    CinematicCamera::CINEMATICCAMERA_DESC desc{};
    desc.speedPerSec = 10.f;
    desc.degreePerSec = 180.f;
    desc.eye = XMFLOAT4(0.f, 10.f, -7.f, 1.f);
    desc.at = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
    desc.fovy = XMConvertToRadians(60.f);
    desc.nearPlane = 0.1f;
    desc.farPlane = 500.f;
    desc.mouseSensor = 0.05f;

    if(FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), L"GameObject_CinematicCamera", ETOI(LevelType::Static), L"Layer_Camera", gameObject, &desc)))
    {
        return nullptr;
    }

    Shared<CinematicCamera> cinematicCamera = dynamic_pointer_cast<CinematicCamera>(gameObject);

    if(cinematicCamera == nullptr)
        return nullptr;

    GAME->Add_Camera(cinematicCamera);
    return cinematicCamera;
}

Shared<DialogueCamera> ClientInstance::Ensure_DialogueCamera()
{
    if(auto camera = dynamic_pointer_cast<DialogueCamera>(GAME->Get_CameraByType(ObjectType::DialogueCamera)))
    {
        return camera;  
    }

    Shared<GameObject> gameObject;

    DialogueCamera::DIALOGUECAMERA_DESC desc{};
    desc.speedPerSec = 10.f;
    desc.degreePerSec = 180.f;
    desc.eye = XMFLOAT4(0.f, 10.f, -7.f, 1.f);
    desc.at = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
    desc.fovy = XMConvertToRadians(60.f);
    desc.nearPlane = 0.1f;
    desc.farPlane = 500.f;
    desc.mouseSensor = 0.05f;

    if(FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), L"GameObject_DialogueCamera", ETOI(LevelType::Static), L"Layer_Camera", gameObject, &desc)))
    {
        return nullptr;
    }

    Shared<DialogueCamera> dialogueCamera = dynamic_pointer_cast<DialogueCamera>(gameObject);

    if(dialogueCamera == nullptr)
        return nullptr;

    GAME->Add_Camera(dialogueCamera);
    return dialogueCamera;
}

Shared<ClientScreenFxManager> ClientInstance::Get_ScreenFxManager() const
{
    return _screenFxManager;
}

bool ClientInstance::Prewarm_Effect(const wstring& effectName, const wstring& layerTag, uint32 count)
{
    if (_effectInstancePool == nullptr)
        return false;

    if (effectName.empty() || layerTag.empty() || count == 0)
        return false;

    wstring resolvedName = effectName;
    if (!resolvedName.ends_with(L".effect.json"))
        resolvedName += L".effect.json";

    EffectInstancePool::AcquireDesc desc{};
    desc.effectName = resolvedName;
    desc.layerLevelIndex = GAME->Current_LevelIndex();
    desc.layerTag = layerTag;
    desc.worldPosition = Vec3::Zero;
    desc.worldRotation = Quat::Identity;
    desc.scale = Vec3::One;

    return SUCCEEDED(_effectInstancePool->Prewarm(desc, count));
}

bool ClientInstance::Play_Effect(const PlayEffectDesc& desc)
{
    if (_effectInstancePool == nullptr)
        return false;

    if (desc.effectName.empty() || desc.layerTag.empty())
        return false;

    wstring resolvedName = desc.effectName;
    if (!resolvedName.ends_with(L".effect.json"))
        resolvedName += L".effect.json";

    Vec3 spawnWorldPosition = desc.worldPosition;
    const Vec3 spawnRotationRad(
        XMConvertToRadians(desc.localRotation.x),
        XMConvertToRadians(desc.localRotation.y),
        XMConvertToRadians(desc.localRotation.z));
    Quat spawnWorldRotation = Quat::CreateFromYawPitchRoll(
        spawnRotationRad.y,
        spawnRotationRad.x,
        spawnRotationRad.z);
    spawnWorldRotation.Normalize();

    if (desc.spawnBasisTarget)
    {
        const Shared<TransformCom> basisTransform = desc.spawnBasisTarget->Get_Transform();
        if (basisTransform == nullptr)
            return false;

        const Vec3 localRotationRad(
            XMConvertToRadians(desc.localRotation.x),
            XMConvertToRadians(desc.localRotation.y),
            XMConvertToRadians(desc.localRotation.z));

        // 단발 기준 local
        const Matrix localMatrix =
            Matrix::CreateFromYawPitchRoll(localRotationRad.y, localRotationRad.x, localRotationRad.z) *
            Matrix::CreateTranslation(desc.localOffset);

        const Matrix finalWorldMatrix = localMatrix * basisTransform->Get_WorldMatrix();

        Vec3 decomposedScale = Vec3::One;
        Quat decomposedRotation = Quat::Identity;
        Vec3 decomposedTranslation = Vec3::Zero;

        Matrix tempMat = finalWorldMatrix;

        if (!tempMat.Decompose(decomposedScale, decomposedRotation, decomposedTranslation))
            return false;

        spawnWorldPosition = decomposedTranslation;
        spawnWorldRotation = decomposedRotation;
        if (spawnWorldRotation.LengthSquared() <= 0.000001f)
            spawnWorldRotation = Quat::Identity;
        else
            spawnWorldRotation.Normalize();
    }

    EffectInstancePool::AcquireDesc acquireDesc{};
    acquireDesc.effectName = resolvedName;
    acquireDesc.layerLevelIndex = GAME->Current_LevelIndex();
    acquireDesc.layerTag = desc.layerTag;
    acquireDesc.worldPosition = spawnWorldPosition;
    acquireDesc.worldRotation = spawnWorldRotation;
    acquireDesc.scale = desc.scale;

    EffectInstancePool::AttachedHandle handle{};
    if (FAILED(_effectInstancePool->Acquire(acquireDesc, handle)))
    {
        LOG_WARN("Play effect failed. effect={}", String::ToString(resolvedName).c_str());
        return false;
    }

    return true;
}

HRESULT ClientInstance::Reload_EffectJsons()
{
    if (_effectInstancePool == nullptr)
        return E_FAIL;

    const wstring assetRoot = GAME->Get_AssetRoot();
    if (assetRoot.empty())
        return E_FAIL;

    const fs::path effectAssetRoot = fs::path(assetRoot) / L"Effects" / L"Assets";
    CHECK_FAILED(GAME->Refresh_AssetRegistryDirectory(effectAssetRoot.wstring(), effectAssetRoot.wstring()), E_FAIL);

    // 새/수정된 effect registry 반영 뒤, 다음 Play_Effect가 최신 .effect.json을 디스크에서 다시 접게 한다.
    _effectInstancePool->Discard_AllInactive();
    return S_OK;
}

bool ClientInstance::Play_AttachPartEffect(
    const Shared<GameObject>& effectOwner,
    const wstring& effectName,
    const wstring& partTag,
    const Vec3& localOffset,
    const Vec3& localRotation,
    const Vec3& scale)
{
    if (effectOwner == nullptr)
        return false;

    const Shared<EffectCom> effectCom = effectOwner->Get_Component<EffectCom>();
    if (effectCom == nullptr)
        return false;

    return SUCCEEDED(effectCom->Play_AttachedPartEffect(
        GAME->Current_LevelIndex(),
        effectName,
        partTag,
        L"Layer_Effect",
        localOffset,
        localRotation,
        scale));
}

bool ClientInstance::Play_AttachPartEffect(
    const Shared<GameObject>& effectOwner,
    const wstring& effectName,
    const wstring& partTag,
    const string& socketName,
    const Vec3& localOffset,
    const Vec3& localRotation,
    const Vec3& scale)
{
    if (effectOwner == nullptr)
        return false;

    const Shared<EffectCom> effectCom = effectOwner->Get_Component<EffectCom>();
    if (effectCom == nullptr)
        return false;

    return SUCCEEDED(effectCom->Play_AttachedPartEffect(
        GAME->Current_LevelIndex(),
        effectName,
        partTag,
        socketName,
        L"Layer_Effect",
        localOffset,
        localRotation,
        scale));
}

void ClientInstance::Clear_RuntimeEffects()
{
    if (_effectInstancePool == nullptr)
        return;

    // 월드에 남아있는 이팩트들 제거
    _effectInstancePool->Clear();
}

void ClientInstance::Notify_FreeAimShotHit()
{
    OnFreeAimShotHit.Broadcast();
}

bool ClientInstance::Is_GameplayMenuOpen() const
{
    return _gamePlayUI && _gamePlayUI->Is_AnyMenuOpen();
}

Shared<GameObject> ClientInstance::Find_Player(uint32 levelIndex)
{
    if (_heroManager)
    {
        auto activeHero = _heroManager->Get_ActiveHero();
        if (activeHero)
            return activeHero;
    }

    return GAME->Find_FirstObject(levelIndex, TEXT("Layer_Player"));
}

HRESULT ClientInstance::Ready_StaticObjects()
{
    if (_isStaticObjectsSpawned)
        return S_OK;

    CHECK_NULL(_persistentObjectManager, E_FAIL);

    CHECK_FAILED(_persistentObjectManager->Spawn_StaticObjects(), E_FAIL);

    _isStaticObjectsSpawned = true;
    return S_OK;
}

void ClientInstance::Sync_PreviewHeroRuntimeData(HeroId heroId)
{
    if (_previewManager == nullptr)
        return;

    _previewManager->Sync_PreviewHeroRuntimeData(heroId);
}

void ClientInstance::Play_PreviewAnimation(HeroId heroId, PreviewAnimState introAnim, PreviewAnimState idleAnim)
{
    if (_previewManager == nullptr)
        return;

    _previewManager->Play_PreviewAnimation(heroId, introAnim, idleAnim);
}

void ClientInstance::Set_SoulLikeController(const Shared<SoulLikeController>& controller)
{
    _soulLikeController = controller;
}

bool ClientInstance::Is_SoulLikeBattleActive() const
{
    return _soulLikeController != nullptr && _soulLikeController->Is_BattleActive();
}

bool ClientInstance::Is_WorldControlSuspended() const
{
    return Is_BattleActive() || Is_SoulLikeBattleActive();
}

const DialogueData* ClientInstance::FindDialogueData(const string& dialogueKey) const
{
    return _dialogueDB->FindDialogueData(dialogueKey);
}

void ClientInstance::StartDialogue(const DialogueRequest& request)
{
    _dialogueManager->StartDialogue(request);
}

void ClientInstance::EndDialogue(bool restoreWorldState)
{
    if (_dialogueManager && _dialogueManager->IsActive())
        _dialogueManager->EndDialogue(restoreWorldState);
}

bool ClientInstance::IsDialogueActive() const
{
    return _dialogueManager && _dialogueManager->IsActive();
}

bool ClientInstance::IsDialogueInteractLocked() const
{
    return _dialogueManager && _dialogueManager->IsInteractLocked();
}

void ClientInstance::AddDialogueInteractionOverlap()
{
    ++_dialogueInteractionOverlapCount;
}

void ClientInstance::RemoveDialogueInteractionOverlap()
{
    if (_dialogueInteractionOverlapCount > 0)
        --_dialogueInteractionOverlapCount;
}

HRESULT ClientInstance::Ready_ResourceLoader()
{
    _resourceLoader = ResourceLoader::Create(_device, _context);
    CHECK_NULL(_resourceLoader, E_FAIL);

    return S_OK;
}

HRESULT ClientInstance::Ready_GameDataTables()
{
    _gameDataTable = GameDataTable::Create();
    CHECK_NULL(_gameDataTable, E_FAIL);

    CHECK_FAILED(_gameDataTable->Load_All(), E_FAIL);

    return S_OK;
}

void ClientInstance::Release_Client()
{
    // 디바이스 리셋 전 정적 전역 맵 모델 및 콜리전 캐시 리소스를 완전히 방출
    MapLoader::Clear_GlobalCache();

    auto ReleaseManager = [](auto& manager)
    {
        if (manager)
        {
            manager->Free();
            manager.reset();
        }
    };

    ReleaseManager(_gameDataTable);
    ReleaseManager(_previewShotTable);
    ReleaseManager(_previewManager);
    ReleaseManager(_effectInstancePool);
    ReleaseManager(_screenFadeTransition);
    ReleaseManager(_levelTransitionFade);
    ReleaseManager(_persistentObjectManager);
    ReleaseManager(_resourceLoader);
    ReleaseManager(_heroManager);
    ReleaseManager(_playerInventory);
    ReleaseManager(_costumeManager);
    ReleaseManager(_battleController);
    ReleaseManager(_gamePlayUI);
    ReleaseManager(_screenFxManager);
    ReleaseManager(_soulLikeController);

    Clear_ReplayMonsterSnapshot();

    _context.Reset();
    _device.Reset();

    _isStaticResourcesLoaded.store(false);
    _isStaticObjectsSpawned = false;
    _isInitialized = false;
}

void ClientInstance::Free()
{
    Release_Client();

    __super::Free();
}
