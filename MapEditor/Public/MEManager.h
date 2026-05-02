#pragma once
#include "MapEditorDefines.h"
#include "MEWindow.h"

NS_BEGIN(MapEditor)

class PanelViewport;

class MEManager
{
public:
    MEManager() = default;
    ~MEManager();

public:
    HRESULT Initialize(Shared<PanelViewport> viewport);
    void Update(float timeDelta);
    void Render();

private:
    void RenderDockSpace();

private:
    map<wstring, Shared<MEWindow>> _windows;
    bool layoutInitialized = false;

public:
    static Unique<MEManager> Create(Shared<PanelViewport> viewport);
};

NS_END
