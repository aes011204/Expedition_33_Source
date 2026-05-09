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
class MEModelManager;

class MapEditorApp final : public Base
{
private:
    static constexpr float _debugLinePos = 0.4f;
    static constexpr Color _debugLineCol = { 0.9f, 0.3f, 0.9f, 1.f };

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
    Unique<MEModelManager> _modelManager;

    Shared<RenderTarget> _idRT;
    ComPtr<ID3D11Texture2D> _idStagingTex;

    SceneContext _scene;

private:
    HRESULT Ready_Light();
    HRESULT Ready_PickingRT();

    void DrawSceneDebug();

    void UpdateRenderHandles();

    void DoPickAt(const ImVec2& mousePos, const ImVec2& vpPos, const ImVec2& vpSize);

    void SetCursorAt(const ImVec2& mousePos, const ImVec2& vpPos, const ImVec2& vpSize);
    void DoSpawnAt(const string& modelPath);

public:
    static Unique<MapEditorApp> Create();
    virtual void Free() override;
};

NS_END
