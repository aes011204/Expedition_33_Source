#pragma once

#include "Client_Defines.h"
#include "Base.h"


NS_BEGIN(Engine)
class GameInstance;
NS_END

NS_BEGIN(Client)

class MainApp final : public Base
{
private:
	MainApp();
	virtual ~MainApp() = default;

public:
	HRESULT Initialize();
	void Update(float fTimeDelta);
	HRESULT Render();
	
private:
	/* 엔진 싱글톤 생명주기를 따르는 비소유 접근 포인터. */
	GameInstance* _gameInstance = { nullptr };
	ComPtr<Device> _device = { nullptr };
	ComPtr<Context> _context = { nullptr };

private:
	HRESULT Ready_Fonts();
	HRESULT Ready_Prototype_For_Static_Level();
	HRESULT Ready_StartLevel(LevelType eStartLevelID);

public:
	static Shared<MainApp> Create();
	virtual void Free() override;
};

NS_END



