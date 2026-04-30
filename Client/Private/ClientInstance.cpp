#include "pch.h"
#include "ClientInstance.h"

#include "ResourceLoader.h"

IMPLEMENT_SINGLETON(ClientInstance)

ClientInstance::ClientInstance()
{
}

HRESULT ClientInstance::Initialize_Client(ComPtr<Device> device, ComPtr<Context> context)
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

HRESULT ClientInstance::Ready_ResourceLoader()
{
    _resourceLoader = ResourceLoader::Create(_device, _context);
	CHECK_NULL(_resourceLoader, E_FAIL);

	return S_OK;
}

void ClientInstance::Release_Client()
{
    _resourceLoader.reset();
	_context.Reset();
	_device.Reset();
	_isInitialized = false;
}

void ClientInstance::Free()
{
    Release_Client();
}
