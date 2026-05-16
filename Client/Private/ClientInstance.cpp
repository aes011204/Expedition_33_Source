#include "pch.h"
#include "ClientInstance.h"

#include "BattleController.h"
#include "BT_Node_Bootstrap.h"
#include "ComputePointParticleEffect.h"
#include "GameDataTable.h"
#include "GameInstance.h"
#include "PersistentObjectManager.h"
#include "Player.h"
#include "ResourceLoader.h"
#include "BT_Node_Bootstrap.h"
#include "GameDataTable.h"
#include "GameplayUI.h"
#include "PersistentObjectManager.h"
#include "Hero_Manager.h"

IMPLEMENT_SINGLETON(ClientInstance)

ClientInstance::ClientInstance()
{
}

HRESULT ClientInstance::Initialize_Client(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    if (_isInitialized)
        return S_OK;

    _device = device;
    _context = context;

    // 프로토타입 등록
    if (FAILED(Ready_ResourceLoader()))
        return E_FAIL;

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

	_isInitialized = true;

	return S_OK;
}

void ClientInstance::Update_Client(float timeDelta)
{
    if (_heroManager)
        _heroManager->Update_HeroManager(timeDelta);

    if (_gamePlayUI)
        _gamePlayUI->Update(timeDelta);

    if (_battleController)
        _battleController->Update(timeDelta);
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

HRESULT ClientInstance::Initialize_Hero(uint32 levelIndex, const wstring& layerTag, const Vec3& startPos)
{
    return _heroManager->Initialize_Hero(levelIndex, layerTag, startPos);
}

void ClientInstance::Update_HeroManager(float timeDelta)
{
    return _heroManager->Update_HeroManager(timeDelta);
}

Shared<Player> ClientInstance::Get_ActiveHero() const
{
    return _heroManager->Get_ActiveHero();
}

const vector<HeroPartyEntry>& ClientInstance::Get_HeroEntries() const
{
    return _heroManager->Get_HeroEntries();
}

Shared<Player> ClientInstance::Get_Hero(HeroPartySlot slot) const
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

Shared<Player> ClientInstance::Get_FrontHero() const
{
    return _heroManager->Get_FrontHero();
}

Shared<Player> ClientInstance::Find_Hero(HeroId heroId) const
{
    return _heroManager->Find_Hero(heroId);
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

void ClientInstance::OnEnterBattleField(uint32 FieldID, bool IsFirstHero, vector<string>& PlayerProtoTag,
    vector<string>& MonsterProtoTag)
{
    _battleController->OnEnterBattleField(FieldID, IsFirstHero, PlayerProtoTag, MonsterProtoTag);
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
    auto ReleaseManager = [](auto& manager)
    {
        if (manager)
        {
            manager->Free();
            manager.reset();
        }
    };

    ReleaseManager(_gameDataTable);
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
