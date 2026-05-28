#pragma once

#include "MapEditorDefines.h"
#include "Base.h"
#include "SceneContext.h"

NS_BEGIN(Engine)
class RenderTarget;
class ModelCom;
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

private:
    HRESULT Ready_Light();

    void SyncSceneLights();
    void Update_RenderHandles();
    void Update_StaticMeshHandles();
    void Update_InstancedMeshHandles();
    void DoSpawnAt(const string& modelPath);
    void RebuildOctreeIfDirty();

    wstring Get_ClientResourceRoot() const;
    void Apply_MaterialFromJson(const fs::path& modelPath, const string& bundleKey, Shared<ModelCom>& model);
    string Resolve_BundleKey(const fs::path& modelPath) const;
    void Apply_PassOverrideToModel(ModelCom& model, const string& modelFilename, Vec3 worldPos = Vec3(0.f, 0.f, 0.f), const string& fullModelPath = "");
    void Apply_PassOverrides();

    void Add_Landscape(const LandscapeConfig& cfg);
    void Remove_Landscape(int32 idx);

    HRESULT Build_TerrainFromScene();

    void Do_SculptAt(ImVec2 mousePos, ImVec2 vpPos, ImVec2 vpSize, float timeDelta);

    void Save_Landscape(int32 idx, const WCHAR* path);
    void Load_Landscape(int32 idx, const WCHAR* path);

    void Apply_Noise(int32 idx, const LandscapeNoiseParams& params);
    void Apply_Erosion(int32 idx, const LandscapeErosionParams& params);

public:
    static Unique<MapEditorApp> Create();
    virtual void Free() override;
};

NS_END
