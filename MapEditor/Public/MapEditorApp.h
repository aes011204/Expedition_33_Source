#pragma once

#include "MapEditorDefines.h"
#include "Base.h"
#include "SceneContext.h"

NS_BEGIN(Engine)
class RenderTarget;
NS_END

NS_BEGIN(MapEditor)

class MEImGuiManager;
class MEManager;
class MECamera;
class PanelViewport;
class MEModelManager;
class MEPickingSystem;
class MEDebugRenderer;
class MEBridgeCamera;

class MapEditorApp final : public Base
{
public:
    MapEditorApp() = default;
    virtual ~MapEditorApp();

public:
    HRESULT Initialize();
    void Priority_Update(float timeDelta);
    void Update(float timeDelta);
    void Late_Update(float timeDelta);
    HRESULT Render();

private:
    ComPtr<Device> _device;
    ComPtr<Context> _context;
    Unique<MEImGuiManager> _imguiManager;
    Unique<MEManager> _manager;

    Shared<MECamera> _camera;
    Shared<MEBridgeCamera> _bridgeCamera;
    Shared<RenderTarget> _rt;
    Shared<PanelViewport> _viewport;

    Unique<MEModelManager> _modelManager;

    Unique<MEPickingSystem> _pickingSystem;
    Unique<MEDebugRenderer> _debugRenderer;

    SceneContext _scene;

    bool _octreeDirty = true;

private:
    HRESULT Ready_Light();

    void SyncSceneLights();
    void UpdateRenderHandles();
    void DoSpawnAt(const string& modelPath);
    void RebuildOctreeIfDirty();

    wstring Get_ClientResourceRoot() const;

public:
    static Unique<MapEditorApp> Create();
    virtual void Free() override;
};

NS_END
