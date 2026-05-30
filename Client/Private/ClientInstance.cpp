#include "pch.h"
#include "ClientInstance.h"
#include "BattleController.h"
#include "BT_Node_Bootstrap.h"
#include "ComputeSpriteEmitter.h"
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

IMPLEMENT_SINGLETON(ClientInstance)

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

    _screenFadeTransition = ScreenFadeTransition::Create();
    if (_screenFadeTransition == nullptr)
        LOG_WARN("ScreenFadeTransition is not available. Screen fade requests will run immediately.");

    _levelTransitionFade = LevelTransitionFade::Create(_device, _context);
    if (_levelTransitionFade == nullptr)
        LOG_WARN("LevelTransitionFade is not available. Level transition requests will fall back to immediate changes.");

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
    if (_screenFadeTransition)
        _screenFadeTransition->Update(timeDelta);

    if (_levelTransitionFade)
        _levelTransitionFade->Update(timeDelta);

    if (_heroManager)
        _heroManager->Update_HeroManager(timeDelta);

    if (_battleController)
        _battleController->Update(timeDelta);

    if (_gamePlayUI)
        _gamePlayUI->Update(timeDelta);

    if (_previewManager)
        _previewManager->Update(timeDelta);
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
    return _previewManager->Hide_Preview();
}

PreviewShotPreset ClientInstance::Get_Preset(HeroId hero, PreviewShotPanel panel)
{
    return _previewShotTable->Get(hero, panel);
}

HRESULT ClientInstance::Initialize_Hero(uint32 levelIndex, const wstring& layerTag, const Vec3& startPos)
{
    return _heroManager->Initialize_Hero(levelIndex, layerTag, startPos);
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

void ClientInstance::Reset_AllHeroesToIdle()
{
    return _heroManager->Reset_AllHeroesToIdle();
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

bool ClientInstance::Is_FadeTransitionPlaying() const
{
    const bool isScreenFadePlaying = _screenFadeTransition && _screenFadeTransition->Is_Playing();
    const bool isLevelFadePlaying = _levelTransitionFade && _levelTransitionFade->Is_Playing();

    return isScreenFadePlaying || isLevelFadePlaying;
}

void ClientInstance::OnEnterBattleField(uint32 FieldID, bool IsFirstHero, const vector<HeroId>& heroIds,
                                        const vector<string>& MonsterProtoTag, const Shared<GameObject>& sourceObject)
{
    return _battleController->OnEnterBattleField(FieldID, IsFirstHero, heroIds, MonsterProtoTag, sourceObject);
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

void ClientInstance::Request_Preview()
{
    return _cameraDirector->Request_Preview();
}

void ClientInstance::Request_BattleTurn()
{
    return _cameraDirector->Request_BattleTurn();
}

void ClientInstance::Request_Cinematic()
{
    return _cameraDirector->Request_Cinematic();
}

void ClientInstance::Request_TargetCameraMode(TargetCameraMode cameraMode)
{
    return _cameraDirector->Request_ChangeTargetCameraMode(cameraMode);
}

void ClientInstance::Restore_PreviousCamera()
{
    return _cameraDirector->Restore_PreviousCamera();
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
    ReleaseManager(_battleController);
    ReleaseManager(_gamePlayUI);

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
