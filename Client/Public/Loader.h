#pragma once

#include "Client_Defines.h"
#include "Base.h"

/* 지정된 다음 레벨에 필요한 리소스를 백그라운드 스레드에서 로딩한다. */

NS_BEGIN(Engine)
class GameInstance;
NS_END

NS_BEGIN(Client)

class Loader final : public Base
{
public:
	Loader(ComPtr<Device> device, ComPtr<Context> context);
	virtual ~Loader() { Free(); }

public:
	HRESULT Initialize(LevelType eNextLevelID);
	HRESULT Loading();
	bool isFinished() {
		return _isFinished;
	}
#ifdef _DEBUG
public:
	HRESULT Print_LoadingText();
#endif

private:
	ComPtr<Device> _device = { nullptr };
	ComPtr<Context> _context = { nullptr };
	HANDLE						_threadHandle = {};
	CRITICAL_SECTION			_criticalSection = {};
	LevelType						_nextLevel = { LevelType::END };

	wchar_t						_loadingText[MAX_PATH] = {};
	bool						_isFinished = { false };
private:
	/* 로딩 스레드에서 json 테이블 기반 prototype을 등록할 때 호출한다. */
	HRESULT Load_ResourceTablesForLevel(LevelType eLevel);

	HRESULT Loading_For_LogoLevel();
	HRESULT Loading_For_GamePlayLevel();

public:
	static Shared<Loader> Create(ComPtr<Device> device, ComPtr<Context> context, LevelType eNextLevelID);
	virtual void Free() override;

};

NS_END





