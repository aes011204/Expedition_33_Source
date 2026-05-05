#pragma once

#include "MapEditorDefines.h"
#include "Base.h"
#include "SceneContext.h"

NS_BEGIN(Engine)
class RenderTarget;
class ShaderCom;
NS_END

NS_BEGIN(MapEditor)

class MEImGuiManager;
class MEManager;
class MECamera;
class PanelViewport;

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

    SceneContext _scene;

private:
    void DrawSceneDebug();

    void UpdateRenderHandles();

public:
    static Unique<MapEditorApp> Create();
    virtual void Free() override;
};

NS_END
