#pragma once

#include "MapEditorDefines.h"
#include "Base.h"
#include "SceneContext.h"

NS_BEGIN(Engine)
class RenderTarget;
class ShaderCom;
class VIBuffer_Rect;
NS_END

NS_BEGIN(MapEditor)

class MEImGuiManager;
class MEManager;
class MECamera;
class PanelViewport;
class MEModelManager;
class MEPickingSystem;
class MEDebugRenderer;

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
    Shared<RenderTarget> _rt;
    Shared<PanelViewport> _viewport;

    Shared<ShaderCom> _meshShader;
    Shared<ShaderCom> _deferredShader;
    Shared<VIBuffer_Rect> _rectBuffer;

    Unique<MEModelManager> _modelManager;

    Unique<MEPickingSystem> _pickingSystem;
    Unique<MEDebugRenderer> _debugRenderer;

    SceneContext _scene;

private:
    HRESULT Ready_Light();

    void SyncSceneLights();
    void UpdateRenderHandles();
    void DoSpawnAt(const string& modelPath);

public:
    static Unique<MapEditorApp> Create();
    virtual void Free() override;
};

NS_END
