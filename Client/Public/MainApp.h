#pragma once

#include "Client_Defines.h"
#include "Base.h"


NS_BEGIN(Engine)
class GameInstance;
NS_END

NS_BEGIN(Client)

class MainApp final : public Base
{
public:
	MainApp();
	virtual ~MainApp() { Free(); }

public:
	HRESULT Initialize();
	void Update(float fTimeDelta);
	HRESULT Render();
	
private:
	GameInstance*       _gameInstance = { nullptr };
	ComPtr<Device>      _device = { nullptr };
	ComPtr<Context>     _context = { nullptr };

private:
	HRESULT Ready_Fonts();
	HRESULT Ready_Prototype_For_Static_Level();
	HRESULT Ready_StartLevel(LevelType eStartLevelID);

public:
	static Shared<MainApp> Create();
	virtual void Free() override;
};

NS_END



