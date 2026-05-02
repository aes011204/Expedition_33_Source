#pragma once

#include "Base.h"

NS_BEGIN(Engine)
class GameInstance;
NS_END

NS_BEGIN(Client)

class ResourceLoader;

class ClientInstance : public Base
{
    DECLARE_SINGLETON(ClientInstance)

public:
	ClientInstance();
	virtual ~ClientInstance() = default;

public:
	HRESULT Initialize_Client(const ComPtr<Device>& device, const ComPtr<Context>& context);
	void    Release_Client();

	/* 레벨 진입 전에 공용/정적 클라 리소스를 준비할 때 호출한다. */
	HRESULT Ready_StaticResources();

	/* 레벨 전환 시 레벨 전용 리소스를 준비할 때 호출한다. */
	HRESULT Ready_LevelResources(LevelType levelType);

	/* Client가 소유한 로더를 외부에서 사용할 때 호출한다. */
	Shared<ResourceLoader> Get_ResourceLoader() const;

public: /* 플레이어 찾기 -> 일단 임시 */
    Shared<GameObject> Find_Player(uint32 levelIndex);


private:
	HRESULT Ready_ResourceLoader();

private:
	ComPtr<Device> _device;
	ComPtr<Context> _context;
    Shared<ResourceLoader> _resourceLoader;

	bool _isInitialized = false;

public:
	void Free() override;

};

NS_END
