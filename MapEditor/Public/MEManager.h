#pragma once
#include "MapEditorDefines.h"
#include "MEWindow.h"

NS_BEGIN(MapEditor)

class PanelViewport;
class PanelSnap;
class MECamera;
struct SceneContext;

class MEManager
{
public:
    MEManager() = default;
    ~MEManager();

public:
    HRESULT Initialize(Shared<PanelViewport> viewport, SceneContext* ctx, MECamera* camera, const LandscapePanelContext& landscapeCtx);
    void Update(float timeDelta);
    void Render();

private:
    void RenderDockSpace();
    void SaveMapDialog();

private:
    map<wstring, Shared<MEWindow>> _windows;
    bool layoutInitialized = false;

    SceneContext* _ctx = {};
    MECamera* _camera = {};

    string _currentMapPath = "Stage_01.json";
    char mapPathBuf[256] = {};

    Shared<SnapSettings> _snapSettings;

    bool _confirmClearOpen = false;
    bool _exportPopupOpen = false;
    char _exportNameBuf[256] = {};

public:
    void AddWindow(Shared<MEWindow> w);

public:
    static Unique<MEManager> Create(Shared<PanelViewport> viewport, SceneContext* ctx, MECamera* camera, const LandscapePanelContext& landscapeCtx);
};

NS_END
