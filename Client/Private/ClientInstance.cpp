#include "pch.h"
#include "ClientInstance.h"
#include "GameInstance.h"
#include "Player.h"
#include "ResourceLoader.h"
#include "BT_Node_Bootstrap.h"
#include "GameDataTable.h"
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

	_isInitialized = true;

    Client::Force_Register_ClientBTNodes();

    _persistentObjectManager = PersistentObjectManager::Create();
    CHECK_NULL(_persistentObjectManager, E_FAIL);

    _heroManager = Hero_Manager::Create();
    CHECK_NULL(_heroManager, E_FAIL);

	return S_OK;
}

void ClientInstance::Update_Client(float timeDelta)
{

}

HRESULT ClientInstance::Ready_StaticResources()
{
    CHECK_NULL(_resourceLoader, E_FAIL);

	if (FAILED(_resourceLoader->Load_LevelTables((LevelType::Static))))
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

Shared<GameObject> ClientInstance::Find_Player(uint32 levelIndex)
{
    auto gameObject = GAME->Find_FirstObject(levelIndex, TEXT("Layer_Player"));
    CHECK_NULL(gameObject, nullptr);

    return gameObject;
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
