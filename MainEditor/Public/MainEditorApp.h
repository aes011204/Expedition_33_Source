#pragma once

#include "Client_Defines.h"
#include "Base.h"

NS_BEGIN(Engine)
class GameInstance;
NS_END

NS_BEGIN(MainEditor)

class MainEditorApp final : public Engine::Base
{
public:
    MainEditorApp();
    virtual ~MainEditorApp() = default;

public:
    HRESULT Initialize();
    void    Priority_Update(float fTimeDelta);
    void    Update(float fTimeDelta);
    void    Late_Update(float fTimeDelta);
    HRESULT Render();

private:
    HRESULT Ready_Fonts();
    HRESULT Ready_Prototype_For_Static_Level();
    HRESULT Ready_StartLevel(LevelType eStartLevelID);

private:
    GameInstance*   _gameInstance = nullptr;
    ComPtr<Device>  _device = nullptr;
    ComPtr<Context> _context = nullptr;

public:
    static Unique<MainEditorApp> Create();
    virtual void Free() override;
};

NS_END
