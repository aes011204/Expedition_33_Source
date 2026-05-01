#pragma once

#include "Client_Defines.h"
#include "Base.h"

NS_BEGIN(Engine)
class GameInstance;
NS_END

NS_BEGIN(GameLauncherApp)

class MainApp final : public Engine::Base
{
public:
    MainApp();
    virtual ~MainApp() = default;

public:
    HRESULT Initialize();
    void Update(float fTimeDelta);
    HRESULT Render();

private:
    HRESULT Ready_Fonts();
    HRESULT Ready_Prototype_For_Static_Level();
    HRESULT Ready_StartLevel(LevelType eStartLevelID);
    wstring Get_ClientResourceRoot() const;
    wstring Get_ClientResourcePath(const wchar_t* relativePath) const;

private:
    Engine::GameInstance* _gameInstance = nullptr;
    ComPtr<Device> _device = nullptr;
    ComPtr<Context> _context = nullptr;

public:
    static Unique<MainApp> Create();
    virtual void Free() override;
};

NS_END
