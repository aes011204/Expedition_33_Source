#pragma once

#include "MapEditorDefines.h"
#include "Base.h"
#include "SceneContext.h"

NS_BEGIN(Engine)
class RenderTarget;
class ComputeShaderCom;
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
class MELandscapeManager;
class MEMaterialApplicator;

// test
class METerrainActor;

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

    Shared<ComputeShaderCom> _instanceCullShader;

    bool _octreeDirty = true;

    Shared<METerrainActor> _terrain;
    Shared<LandscapeSculptState> _sculptState;

    Unique<MELandscapeManager> _landscapeManager;
    Unique<MEMaterialApplicator> _materialApplicator;

    float _timeDelta = 0.f;

private:
    HRESULT Ready_Light();
    void Apply_DefaultPostProcessSettings();
    void Apply_DefaultShadowSettings();

    void SyncSceneLights();
    void SyncFogZones();
    void Update_RenderHandles();
    void Update_StaticMeshHandles();
    void Update_InstancedMeshHandles();
    void Update_WaterHandles();
    void DoSpawnAt(const string& modelPath);
    void RebuildOctreeIfDirty();

    wstring Get_ClientResourceRoot() const;

    HRESULT Build_TerrainFromScene();

public:
    static Unique<MapEditorApp> Create();
    virtual void Free() override;
};

NS_END
