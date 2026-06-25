#pragma once

#include "Client_Defines.h"
#include "Base.h"
#include "RuntimeCheckpointIO.h"

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
    HRESULT Initialize(const wstring& commandLine);
    void Update(float fTimeDelta);
    HRESULT Render();

private:
    HRESULT Ready_Fonts();
    HRESULT Ready_Prototype_For_Static_Level();
    HRESULT Ready_StartLevel(LevelType eStartLevelID);
    void Parse_CommandLine(const wstring& commandLine);
    void Try_ApplyStartupCheckpoint();
    void Apply_RuntimeInputPolicy();
    wstring Get_ClientResourceRoot() const;
    wstring Get_ClientResourcePath(const wchar_t* relativePath) const;

private:
    ComPtr<Device> _device = nullptr;
    ComPtr<Context> _context = nullptr;
    fs::path _startupCheckpointPath = {};
    Client::RuntimeCheckpointIO::Checkpoint _startupCheckpoint = {};
    bool _hasStartupCheckpoint = false;
    bool _startupCheckpointApplied = false;

public:
    static Unique<MainApp> Create(const wstring& commandLine = L"");
    virtual void Free() override;
};

NS_END
