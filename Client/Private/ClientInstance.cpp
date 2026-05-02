#include "pch.h"
#include "ClientInstance.h"
#include "GameInstance.h"
#include "Player.h"
#include "ResourceLoader.h"

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

	if (FAILED(Ready_ResourceLoader()))
		return E_FAIL;

	_isInitialized = true;
	return S_OK;
}

HRESULT ClientInstance::Ready_StaticResources()
{
    CHECK_NULL(_resourceLoader, E_FAIL);

	if (FAILED(_resourceLoader->Load_LevelTables(static_cast<LevelType>(LevelType::Static))))
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

Shared<GameObject> ClientInstance::Find_Player(uint32 levelIndex)
{
    auto gameObject = GAME->Find_FirstObject(levelIndex, TEXT("Layer_Player"));
    if (gameObject == nullptr)
        return nullptr;

    return dynamic_pointer_cast<Player>(gameObject);
}

HRESULT ClientInstance::Ready_ResourceLoader()
{
    _resourceLoader = ResourceLoader::Create(_device, _context);
	CHECK_NULL(_resourceLoader, E_FAIL);

	return S_OK;
}

void ClientInstance::Release_Client()
{
    if (_resourceLoader)
    {
        _resourceLoader->Free();
        _resourceLoader.reset();
    }

    _context.Reset();
    _device.Reset();
    _isInitialized = false;
}

void ClientInstance::Free()
{
    Release_Client();

    __super::Free();
}
