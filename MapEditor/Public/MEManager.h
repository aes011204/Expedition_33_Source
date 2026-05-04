#pragma once
#include "MapEditorDefines.h"
#include "MEWindow.h"

NS_BEGIN(MapEditor)

class PanelViewport;
class PanelSnap;
struct SceneContext;

class MEManager
{
public:
    MEManager() = default;
    ~MEManager();

public:
    HRESULT Initialize(Shared<PanelViewport> viewport, SceneContext* ctx);
    void Update(float timeDelta);
    void Render();

private:
    void RenderDockSpace();

private:
    map<wstring, Shared<MEWindow>> _windows;
    bool layoutInitialized = false;

    SceneContext* _ctx = {};
    string _currentMapPath = "Stage_01.json";
    char mapPathBuf[256] = {};

    Shared<SnapSettings> _snapSettings;

public:
    void AddWindow(Shared<MEWindow> w);

public:
    static Unique<MEManager> Create(Shared<PanelViewport> viewport, SceneContext* ctx);
};

NS_END
