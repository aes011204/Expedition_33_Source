#pragma once

#include "Base.h"

NS_BEGIN(Engine)
class GameInstance;
NS_END

NS_BEGIN(Client)

class ResourceLoader;
class PersistentObjectManager;

class ClientInstance : public Base
{
    DECLARE_SINGLETON(ClientInstance)

public:
	ClientInstance();
	virtual ~ClientInstance() = default;

public:
	HRESULT Initialize_Client(const ComPtr<Device>& device, const ComPtr<Context>& context);
    void    Update_Client(float timeDelta);
	void    Release_Client();

	/* 레벨 진입 전에 공용/정적 클라 리소스를 준비할 때 호출한다. */
	HRESULT Ready_StaticResources();
	HRESULT Ready_LevelResources(LevelType levelType);
    HRESULT Ready_StaticObjects();

	Shared<ResourceLoader> Get_ResourceLoader() const;
    bool    Is_StaticResourcesLoaded() const { return _isStaticResourcesLoaded.load(); }
    void    Set_StaticResourcesLoaded(bool isLoaded) { _isStaticResourcesLoaded.store(isLoaded); }

public: /* 플레이어 찾기 -> 일단 임시 */
    Shared<GameObject> Find_Player(uint32 levelIndex);


private:
	HRESULT Ready_ResourceLoader();

private:
	ComPtr<Device>  _device;
	ComPtr<Context> _context;
    Shared<ResourceLoader> _resourceLoader;

    Unique<PersistentObjectManager> _persistentObjectManager;

    atomic<bool> _isStaticResourcesLoaded = false;
    bool _isStaticObjectsSpawned = false;
	bool _isInitialized = false;

public:
	void Free() override;

};

NS_END
