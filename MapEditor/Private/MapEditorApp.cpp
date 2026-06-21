#include "pch.h"
#include "MapEditorApp.h"
#include "GameInstance.h"
#include "PostProcessPipeline.h"
#include "RenderTarget.h"

#include "MEPickingSystem.h"
#include "MEDebugRenderer.h"
#include "MEImGuiManager.h"
#include "MEManager.h"
#include "MECamera.h"
#include "PanelViewport.h"
#include "MEStaticMeshActor.h"
#include "MEModelManager.h"
#include "CmdAddObject.h"
#include "MEInstancedMeshActor.h"
#include "MEBridgeCamera.h"
#include "ComputeShaderCom.h"
#include "MELandscapeActor.h"
#include "MELandscapeManager.h"
#include "MEMaterialApplicator.h"
#include "MEWaterActor.h"
#include "CmdDeleteObject.h"

// test
#include "METerrainActor.h"

extern HWND g_hWnd;
extern HINSTANCE g_hInst;

MapEditorApp::~MapEditorApp()
{

}

HRESULT MapEditorApp::Initialize()
{
    ENGINE_DESC desc{};
    desc.hInstance = g_hInst;
    desc.hWnd = g_hWnd;
    desc.winMode = WinMode::Win;
    desc.numLevels = 1;
    desc.viewportWidth = g_iWinSizeX;
    desc.viewportHeight = g_iWinSizeY;
    desc.projectName = "MapEditor";

    CHECK_FAILED(GAME->Initialize_Engine(desc, _device, _context), E_FAIL);

    const fs::path assetRootPath(Get_ClientResourceRoot());
    CHECK_FAILED(GAME->Initialize_AssetManager(assetRootPath.wstring()), E_FAIL);

    _imguiManager = MEImGuiManager::Create(g_hWnd, _device, _context);
    CHECK_NULL(_imguiManager, E_FAIL);

    _camera = MECamera::Create(g_hWnd, 60.f, static_cast<float>(g_iWinSizeX) / g_iWinSizeY, 0.1f, 1000.f);

    _bridgeCamera = MEBridgeCamera::Create(_device, _context);
    CHECK_NULL(_bridgeCamera, E_FAIL);
    GAME->Add_Camera(_bridgeCamera);
    GAME->Change_ActiveCamera(_bridgeCamera);

    _rt = RenderTarget::Create(L"RenderTarget", _device, _context, g_iWinSizeX, g_iWinSizeY, DXGI_FORMAT_R8G8B8A8_UNORM, Vec4(0.1f, 0.1f, 0.1f, 1.f));
    CHECK_NULL(_rt, E_FAIL);

    _modelManager = MEModelManager::Create();
    CHECK_NULL(_modelManager, E_FAIL);

    _viewport = PanelViewport::Create(_camera, _rt, &_scene);
    CHECK_NULL(_viewport, E_FAIL);

    _sculptState = make_shared<LandscapeSculptState>();
    _viewport->Set_SculpState(_sculptState);

    MELandscapeManager::LandMG_Desc landDesc{};
    landDesc.device = _device;
    landDesc.context = _context;
    landDesc.scene = &_scene;
    landDesc.camera = _camera.get();
    landDesc.sculptState = _sculptState;
    landDesc.resourceRoot = Get_ClientResourceRoot();

    _landscapeManager = MELandscapeManager::Create(landDesc);
    CHECK_NULL(_landscapeManager, E_FAIL);

    MEMaterialApplicator::Material_Desc matDesc{};
    matDesc.scene = &_scene;
    _materialApplicator = MEMaterialApplicator::Create(matDesc);
    CHECK_NULL(_materialApplicator, E_FAIL);

    LandscapePanelContext landscapeCtx{};
    landscapeCtx.sculptState = _sculptState;
    landscapeCtx.onAdd = [this](const LandscapeConfig& cfg) { _landscapeManager->Add_Landscape(cfg); };
    landscapeCtx.onRemove = [this](int32 idx) { _landscapeManager->Remove_Landscape(idx); };
    landscapeCtx.onSave = [this](int32 idx, const WCHAR* path) { _landscapeManager->Save_Landscape(idx, path); };
    landscapeCtx.onLoad = [this](int32 idx, const WCHAR* path) { _landscapeManager->Load_Landscape(idx, path); };
    landscapeCtx.onApplyNoise = [this](int32 idx, const LandscapeNoiseParams& params) { _landscapeManager->Apply_Noise(idx, params); };
    landscapeCtx.onApplyErosion = [this](int32 idx, const LandscapeErosionParams& params) { _landscapeManager->Apply_Erosion(idx, params); };
    landscapeCtx.onAddLayer = [this](int32 idx, const WCHAR* albedo, const WCHAR* normal, const WCHAR* roughness) { _landscapeManager->Add_LandscapeLayer(idx, albedo, normal, roughness); };
    landscapeCtx.onRemoveLayer = [this](int32 landscapeIdx, uint32 layerIdx) { _landscapeManager->Remove_LandscapeLayer(landscapeIdx, layerIdx); };
    landscapeCtx.onSetBaseLayer = [this](int32 landscapeIdx, uint32 layerIdx) { _landscapeManager->Set_LandscapeBaseLayer(landscapeIdx, layerIdx); };
    landscapeCtx.onModify = [this](int32 idx, const LandscapeConfig& cfg) { _landscapeManager->Modify_Landscape(idx, cfg); };

    _manager = MEManager::Create(_viewport, &_scene, _camera.get(), landscapeCtx);
    CHECK_NULL(_manager, E_FAIL);

    _pickingSystem = MEPickingSystem::Create(_device, _context, g_iWinSizeX, g_iWinSizeY);
    CHECK_NULL(_pickingSystem, E_FAIL);
    _viewport->Set_PickingSystem(_pickingSystem.get());

    _debugRenderer = MEDebugRenderer::Create();
    CHECK_NULL(_debugRenderer, E_FAIL);

    CHECK_FAILED(GAME->Add_Prototype(0, L"Shader_VtxMesh_Static",
        ShaderCom::Create(_device, _context,
            L"../../../Client/Bin/ShaderFiles/Shader_VtxMesh.hlsl",
            VTXMESH::Elements, VTXMESH::numElements, ShaderType::VtxMesh)), E_FAIL);

    auto instanceShader = ShaderCom::Create(_device, _context,
        L"../../../Client/Bin/ShaderFiles/Shader_VtxMeshInstanceExact.hlsl",
        VtxMeshInstanceExactLayout::Elements, VtxMeshInstanceExactLayout::numElements, ShaderType::VtxInstanceMeshExact);
    CHECK_NULL(instanceShader, E_FAIL);
    CHECK_FAILED(GAME->Add_Prototype(0, L"Shader_VtxMeshInstanceExact", instanceShader), E_FAIL);

    CHECK_FAILED(GAME->Add_Prototype(0, L"Shader_VtxNorTex",
        ShaderCom::Create(_device, _context,
            L"../../../Client/Bin/ShaderFiles/Shader_VtxNorTex.hlsl",
            VTXNORTEX::Elements, VTXNORTEX::numElements, ShaderType::VtxNorTex)), E_FAIL);

    CHECK_FAILED(GAME->Add_Prototype(0, L"Shader_Water",
        ShaderCom::Create(_device, _context,
            L"../../../Client/Bin/ShaderFiles/Shader_Water.hlsl",
            VTXNORTEX::Elements, VTXNORTEX::numElements, ShaderType::VtxNorTex)), E_FAIL);

    CHECK_FAILED(GAME->Add_Prototype(0, L"MEInstancedMeshActor",
        MEInstancedMeshActor::Create(_device, _context)), E_FAIL);

    _instanceCullShader = ComputeShaderCom::Create(_device, _context,
        L"../../../Client/Bin/ShaderFiles/Compute/Shader_Compute_InstanceCull.hlsl",
        "CS_Main");
    CHECK_NULL(_instanceCullShader, E_FAIL);


    CHECK_FAILED(Ready_Light(), E_FAIL);
    Apply_DefaultPostProcessSettings();
    Apply_DefaultShadowSettings();

    return S_OK;
}

void MapEditorApp::Priority_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void MapEditorApp::Update(float timeDelta)
{
    GAME->Update_Engine(timeDelta);

    _timeDelta = timeDelta;

    _imguiManager->Update(timeDelta);

    if (!ImGui::GetIO().WantTextInput)
    {
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z))
        {
            _scene.UndoCommand();
        }
        else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y))
        {
            _scene.RedoCommand();
        }
    }

    _manager->Update(timeDelta);
    Update_RenderHandles();

    if (_scene._terrainDirty)
    {
        _scene._terrainDirty = false;
        _terrain.reset();
        Build_TerrainFromScene();
    }

    if (_scene._octreeDirty)
    {
        if (_scene._octreeRebuildTimer > 0.f)
        {
            _scene._octreeRebuildTimer -= timeDelta;
        }
        else
        {
            RebuildOctreeIfDirty();
        }
    }

    for (auto& obj : _scene._staticMeshes)
    {
        if (!obj->_renderHandle)
        {
            continue;
        }

        auto tf = obj->_renderHandle->Get_Transform();
        tf->Set_Position(obj->_position);
        tf->Set_Scale(obj->_scale);
        tf->Set_RotationQuaternion(obj->_rotation);
    }
}

void MapEditorApp::Late_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

HRESULT MapEditorApp::Render()
{
    _camera->Apply();

    ImVec2 mousePos{}, vpPos{}, vpSize{};
    if (_viewport->ConsumePick(mousePos, vpPos, vpSize))
    {
        _pickingSystem->DoPickAt(_scene, mousePos, vpPos, vpSize, *_rt);
    }

    if (_viewport->ConsumeDelete())
    {
        if (_scene._selected)
        {
            auto it = find(_scene._objects.begin(), _scene._objects.end(), _scene._selected);
            if (it != _scene._objects.end())
            {
                int index = static_cast<int>(distance(_scene._objects.begin(), it));
                _scene.PushCommand(CmdDeleteObject::Create(_scene._selected, index));
            }
        }
    }

    if (_viewport->ConsumeCursorSet(mousePos, vpPos, vpSize))
    {
        _pickingSystem->SetCursorAt(_scene, *_camera, mousePos, vpPos, vpSize, *_rt);
    }

    ImVec2 sculptMouse{}, sculptVpPos{}, sculptVpSize{};
    if (_viewport->ConsumeSculpt(sculptMouse, sculptVpPos, sculptVpSize))
    {
        _landscapeManager->Do_SculptAt(sculptMouse, sculptVpPos, sculptVpSize, 0.016f);
    }

    string spawnPath{};
    ImVec2 spawnMouse{}, spawnVpPos{}, spawnVpSize{};
    if (_viewport->ConsumeSpawn(spawnPath, spawnMouse, spawnVpPos, spawnVpSize))
    {
        DoSpawnAt(spawnPath);
    }

    SyncSceneLights();
    SyncFogZones();

    GAME->UpdateFrustum(_camera->GetView(), _camera->GetProj());

    vector<uint32> visibleIndices;
    GAME->QueryVisible(visibleIndices);

    if (_scene._objectBBoxMode)
    {
        for (uint32 idx : visibleIndices)
        {
            auto& obj = _scene._staticMeshes[idx];
            if (!obj->_renderHandle)
            {
                continue;
            }

            const auto& model = obj->_renderHandle->GetModel();
            Vec3 localMin{}, localMax{};
            BoundingBox worldBox{};
            if (model->Try_Get_LocalBounds(localMin, localMax))
            {
                BoundingBox localBox;
                BoundingBox::CreateFromPoints(localBox, localMin, localMax);
                localBox.Transform(worldBox, model->Get_PreLocalTransformMatrix() * obj->_worldMatrix);
            }
            else
            {
                worldBox.Center = obj->_position;
                worldBox.Extents = Vec3(0.3f, 0.3f, 0.3f);
            }

            DebugBoxDesc box;
            box.center = worldBox.Center;
            box.extents = worldBox.Extents;
            box.style.color = (obj == _scene._selected) ? Color(1.f, 1.f, 0.f, 1.f) : Color(0.5f, 0.8f, 1.f, 1.f);
            box.style.depthEnabled = true;
            GAME->Draw_DebugBox(box);
        }
    }
    else
    {
        for (uint32 idx : visibleIndices)
        {
            if (idx >= static_cast<uint32>(_scene._staticMeshes.size()))
            {
                continue;
            }
            auto& obj = _scene._staticMeshes[idx];
            if (!obj->_renderHandle)
            {
                continue;
            }

            obj->_renderHandle->Set_CameraFar(_camera->GetFar());
            obj->_renderHandle->Late_Update(0.f);
        }
    }

    if (!_scene._hideInstancedMesh)
    {
        BoundingFrustum frustum;
        BoundingFrustum::CreateFromMatrix(frustum, _camera->GetProj());
        frustum.Transform(frustum, XMMatrixInverse(nullptr, _camera->GetView()));

        for (auto& obj : _scene._instancedMeshes)
        {
            for (size_t ci = 0; ci < obj->_instancedChunkHandles.size(); ++ci)
            {
                if (!obj->_instancedChunkAABBs[ci].Intersects(frustum))
                {
                    continue;
                }

                obj->_instancedChunkHandles[ci]->Set_CameraFar(_camera->GetFar());
                obj->_instancedChunkHandles[ci]->Late_Update(_timeDelta);
            }
        }
    }


    // test
    if (_terrain)
    {
        _terrain->Set_CameraFar(_camera->GetFar());
        _terrain->Late_Update(0.f);
    }
    //

    for (auto& landscape : _scene._landscapes)
    {
        landscape->Set_CameraFar(_camera->GetFar());
        landscape->Set_CameraPos(_camera->GetPosition());
        landscape->Late_Update(0.f);
    }

    for (auto& obj : _scene._waters)
    {
        if (!obj->_waterRenderHandle)
        {
            continue;
        }

        auto tf = obj->_waterRenderHandle->Get_Transform();
        tf->Set_Position(obj->_position);
        tf->Set_Scale(obj->_scale);

        obj->_waterRenderHandle->Set_CameraInfo(_camera->GetFar(),
            Vec4(_camera->GetPosition().x, _camera->GetPosition().y, _camera->GetPosition().z, 1.f));
        obj->_waterRenderHandle->Update(_timeDelta);
        obj->_waterRenderHandle->Late_Update(_timeDelta);
    }

    GAME->Bind_BackBuffer();
    CHECK_FAILED(GAME->Clear_Buffers(Vec4(0.1f, 0.1f, 0.1f, 1.f)), E_FAIL);
    CHECK_FAILED(GAME->Draw_RendererOnly(true), E_FAIL);

    _debugRenderer->Draw(_scene);
    GAME->Render_DebugDepth();
    GAME->Render_DebugOverlay();

    _imguiManager->Capture_BackBuffer_ToViewport();
    _viewport->Set_SceneSRV(_imguiManager->Get_SharedViewportSRV());

    _rt->Begin();
    GAME->Bind_BackBuffer();

    _manager->Render();
    _imguiManager->Render();

    CHECK_FAILED(GAME->Present(), E_FAIL);

    return S_OK;
}

void MapEditorApp::Update_RenderHandles()
{
    Update_StaticMeshHandles();
    Update_InstancedMeshHandles();
    Update_WaterHandles();

    _materialApplicator->Apply_PassOverrides();
}

void MapEditorApp::Update_StaticMeshHandles()
{
    if (!_scene._dirtyRenderHandles)
    {
        return;
    }

    _scene._dirtyRenderHandles = false;

    for (auto& obj : _scene._staticMeshes)
    {
        if (obj->_renderHandleLoaded || obj->_isInstanced || obj->_modelPath.empty())
        {
            continue;
        }

        obj->_renderHandleLoaded = true;

        auto model = _modelManager->GetOrLoad(_device, _context, obj->_modelPath);
        if (!model)
        {
            continue;
        }

        auto actor = MEStaticMeshActor::Create(_device, _context);
        if (!actor)
        {
            continue;
        }

        actor->Set_Model(model);

        const string bundleKey = _materialApplicator->Resolve_BundleKey(obj->_modelPath);
        _materialApplicator->Apply_MaterialFromJson(obj->_modelPath, bundleKey, model);

        const string modelFilename = String::ToString(fs::path(obj->_modelPath).filename().wstring());
        _materialApplicator->Apply_PassOverrideToModel(*model, modelFilename, obj->_position, String::ToString(obj->_modelPath));

        obj->_renderHandle = actor;
        _scene._octreeDirty = true;
    }
}

void MapEditorApp::Update_InstancedMeshHandles()
{
    for (auto& obj : _scene._instancedMeshes)
    {
        if (!obj->_instancedParamsDirty)
        {
            continue;
        }

        obj->_instancedParamsDirty = false;
        obj->_instancedChunkHandles.clear();
        obj->_instancedChunkAABBs.clear();
        obj->_instancedChunkGlobalIndices.clear();

        if (obj->_instancedModelPath.empty())
        {
            continue;
        }

        map<pair<int32, int32>, vector<pair<InstanceTransform, uint32>>> chunkMap;
        uint32 instTransSize = static_cast<uint32>(obj->_instancedTransforms.size());
        for (uint32 i = 0; i < instTransSize; ++i)
        {
            const auto& t = obj->_instancedTransforms[i];
            int32 cx = static_cast<int32>(floorf(t._position.x / CHUNK_SIZE));
            int32 cz = static_cast<int32>(floorf(t._position.z / CHUNK_SIZE));
            chunkMap[{cx, cz}].push_back({ t, i });
        }

        for (auto& [key, transformPairs] : chunkMap)
        {
            vector<InstanceTransform> transforms;
            vector<uint32> globalIndices;
            transforms.reserve(transformPairs.size());
            globalIndices.reserve(transformPairs.size());
            for (auto& [t, idx] : transformPairs)
            {
                transforms.push_back(t);
                globalIndices.push_back(idx);
            }

            auto actor = static_pointer_cast<MEInstancedMeshActor>(
                GAME->Clone_Prototype(Prototype::GameObject, 0, L"MEInstancedMeshActor", nullptr));
            if (!actor)
            {
                continue;
            }

            actor->Set_CullShader(_instanceCullShader);
            if (FAILED(actor->UpdateInstances(obj->_instancedModelPath, transforms, _modelManager.get())))
            {
                continue;
            }

            Vec3 chunkMin(FLT_MAX, FLT_MAX, FLT_MAX);
            Vec3 chunkMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
            for (const auto& t : transforms)
            {
                float pad = max({ t._scale.x, t._scale.y, t._scale.z }) * 10.f;
                chunkMin.x = min(chunkMin.x, t._position.x - pad);
                chunkMin.y = min(chunkMin.y, t._position.y - pad);
                chunkMin.z = min(chunkMin.z, t._position.z - pad);
                chunkMax.x = max(chunkMax.x, t._position.x + pad);
                chunkMax.y = max(chunkMax.y, t._position.y + pad);
                chunkMax.z = max(chunkMax.z, t._position.z + pad);
            }

            BoundingBox aabb;
            BoundingBox::CreateFromPoints(aabb,
                XMVectorSet(chunkMin.x, chunkMin.y, chunkMin.z, 0.f),
                XMVectorSet(chunkMax.x, chunkMax.y, chunkMax.z, 0.f));

            auto model = actor->GetModel();
            if (model)
            {
                const string bundleKey = _materialApplicator->Resolve_BundleKey(obj->_instancedModelPath);
                _materialApplicator->Apply_MaterialFromJson(obj->_instancedModelPath, bundleKey, model);
                const string modelFilename = String::ToString(fs::path(obj->_instancedModelPath).filename().wstring());
                _materialApplicator->Apply_PassOverrideToModel(*model, modelFilename);

                actor->Refresh_HasGlow();
            }

            obj->_instancedChunkHandles.push_back(actor);
            obj->_instancedChunkAABBs.push_back(aabb);
            obj->_instancedChunkGlobalIndices.push_back(move(globalIndices));
        }
    }
}

void MapEditorApp::Update_WaterHandles()
{
    for (auto& obj : _scene._waters)
    {
        if (obj->_waterRenderHandleLoaded)
        {
            continue;
        }

        obj->_waterRenderHandleLoaded = true;

        VIBuffer_Plane::DESC planeDesc;
        planeDesc.countX = obj->_waterDesc.countX;
        planeDesc.countZ = obj->_waterDesc.countZ;
        planeDesc.sizeX = obj->_waterDesc.sizeX;
        planeDesc.sizeZ = obj->_waterDesc.sizeZ;
        auto actor = MEWaterActor::Create(_device, _context, planeDesc);
        if (!actor)
        {
            continue;
        }

        auto LoadSRV = [&](const wstring& path) -> ComPtr<ID3D11ShaderResourceView>
            {
                if (path.empty())
                {
                    return nullptr;
                }
                WCHAR ext[MAX_PATH] = {};
                _wsplitpath_s(path.c_str(), nullptr, 0, nullptr, 0, nullptr, 0, ext, MAX_PATH);
                ComPtr<ID3D11ShaderResourceView> srv;
                HRESULT hr = (lstrcmp(ext, L".dds") == 0)
                    ? CreateDDSTextureFromFile(_device.Get(), path.c_str(), nullptr, srv.GetAddressOf())
                    : CreateWICTextureFromFile(_device.Get(), path.c_str(), nullptr, srv.GetAddressOf());
                if (FAILED(hr))
                {
                    return nullptr;
                }
                return srv;
            };

        if (auto srv = LoadSRV(obj->_waterLargeWaveNormalPath))
        {
            actor->Set_LargeWaveNormalSRV(srv.Get());
        }
        if (auto srv = LoadSRV(obj->_waterSmallWaveNormalPath))
        {
            actor->Set_SmallWaveNormalSRV(srv.Get());
        }
        if (auto srv = LoadSRV(obj->_waterSeafoamPath))
        {
            actor->Set_SeafoamSRV(srv.Get());
        }
        if (auto srv = LoadSRV(obj->_waterCubemapPath))
        {
            actor->Set_CubemapSRV(srv.Get());
        }

        actor->Set_WaterParams(obj->_waterParams);
        obj->_waterRenderHandle = actor;
    }
}

void MapEditorApp::DoSpawnAt(const string& modelPath)
{
    if (!_scene._cursorVisible)
    {
        return;
    }

    string ext = fs::path(modelPath).extension().string();

    Shared<EditorObject> spawned;
    if (ext == ".prefab")
    {
        spawned = _scene.SpawnFromPrefab(modelPath, _scene._cursorPos);
    }
    else
    {
        if (_viewport->IsSpawnAsInstanced())
        {
            spawned = _scene.SpawnInstancedObject(String::ToWString(modelPath), _scene._cursorPos);
        }
        else
        {
            spawned = _scene.SpawnObject(String::ToWString(modelPath), _scene._cursorPos);
        }
    }

    if (spawned)
    {
        _scene.Remove(spawned);
        _scene.PushCommand(CmdAddObject::Create(spawned));
    }
}

void MapEditorApp::RebuildOctreeIfDirty()
{
    if (!_scene._octreeDirty)
    {
        return;
    }

    vector<BoundingBox> worldAABBs;
    worldAABBs.reserve(_scene._staticMeshes.size());

    for (auto& obj : _scene._staticMeshes)
    {
        BoundingBox worldBox{};
        if (obj->_renderHandle)
        {
            const auto& model = obj->_renderHandle->GetModel();
            Vec3 localMin{}, localMax{};

            if (model->Try_Get_LocalBounds(localMin, localMax))
            {
                BoundingBox localBox;
                BoundingBox::CreateFromPoints(localBox, localMin, localMax);
                localBox.Transform(worldBox, model->Get_PreLocalTransformMatrix() * obj->_worldMatrix);
            }
            else
            {
                worldBox.Center = obj->_position;
                worldBox.Extents = Vec3(0.01f, 0.01f, 0.01f);
            }
        }
        else
        {
            worldBox.Center = obj->_position;
            worldBox.Extents = Vec3(0.01f, 0.01f, 0.01f);
        }
        worldAABBs.push_back(worldBox);
    }

    GAME->BuildOctree(worldAABBs);
    _scene._octreeDirty = false;
}

void MapEditorApp::SyncSceneLights()
{
    if (!_scene._lightDirty)
    {
        return;
    }

    _scene._lightDirty = false;

    GAME->Clear_Lights();

    bool hasPrimaryDirectional = false;
    LIGHT_DESC primaryDirectionalDesc{};

    for (const auto& obj : _scene._lights)
    {
        LIGHT_DESC desc{};
        desc.type = obj->_lightType;
        desc.direction = obj->_lightDirection;
        desc.position = Vec4(obj->_position.x, obj->_position.y, obj->_position.z, 1.f);
        desc.range = obj->_lightRange;
        desc.diffuse = obj->_lightDiffuse;
        desc.ambient = obj->_lightAmbient;
        desc.specular = obj->_lightSpecular;
        GAME->Add_Light(desc);

        if (!hasPrimaryDirectional && desc.type == LightType::Directional)
        {
            primaryDirectionalDesc = desc;
            primaryDirectionalDesc.castShadow = true;
            hasPrimaryDirectional = true;
        }
    }

    if (hasPrimaryDirectional)
    {
        GAME->Update_PrimaryShadowLightDesc(primaryDirectionalDesc);
        Apply_DefaultShadowSettings();
    }
}

void MapEditorApp::SyncFogZones()
{
    FogZoneCB cb{};
    bool hasGroundFogGlobals = false;

    for (const auto& obj : _scene._postProcessVolumes)
    {
        if (!obj || obj->_ppFogDensity <= 0.f)
        {
            continue;
        }

        if (!hasGroundFogGlobals)
        {
            cb.groundFogMaxDistance = obj->_ppGroundFogMaxDistance;
            cb.groundFogMinAccumulationDistance = obj->_ppGroundFogMinAccumulationDistance;
            cb.groundFogDistanceInfluence = obj->_ppGroundFogDistanceInfluence;
            cb.groundFogNearFadeDistance = obj->_ppGroundFogNearFadeDistance;
            cb.groundFogNearMinFactor = obj->_ppGroundFogNearMinFactor;
            hasGroundFogGlobals = true;
        }

        FogZoneEntry entry{};
        entry.fogColor = Color(obj->_ppFogColor.x, obj->_ppFogColor.y, obj->_ppFogColor.z, 1.f);
        entry.fogDensity = obj->_ppFogDensity;
        entry.baseHeight = obj->_ppFogBaseHeight;
        entry.topFadeStart = obj->_ppFogTopFadeStart;
        entry.topHeight = obj->_ppFogTopHeight;
        entry.edgeFade = obj->_ppFogEdgeFade;
        entry.noiseScale = obj->_ppFogNoiseScale;
        entry.noiseSpeed = obj->_ppFogNoiseSpeed;
        entry.noiseStrength = obj->_ppFogNoiseStrength;
        entry.noiseMin = obj->_ppFogNoiseMin;

        if (cb.aabbCount >= FogZoneCB::MAX_FOG_ZONES)
        {
            continue;
        }

        entry.zoneMin = obj->_position - obj->_zoneExtent;
        entry.zoneMax = obj->_position + obj->_zoneExtent;
        cb.zones[cb.aabbCount++] = entry;
    }

    GAME->Set_FogZones(cb);
}

HRESULT MapEditorApp::Ready_Light()
{
    auto obj = EditorObject::Create(L"Directional Light", EObjectType::Light);
    obj->_lightType = LightType::Directional;
    obj->_lightDirection = Vec4(1.f, -1.f, 1.f, 0.f);
    obj->_lightDiffuse = Vec4(1.f, 1.f, 1.f, 1.f);
    obj->_lightAmbient = Vec4(0.4f, 0.4f, 0.4f, 1.f);
    obj->_lightSpecular = Vec4(1.f, 1.f, 1.f, 1.f);
    obj->_isDefaultLight = true;
    _scene.Add(obj);
    return S_OK;
}

void MapEditorApp::Apply_DefaultPostProcessSettings()
{
    const Shared<PostProcessPipeline> pipeline = GAME->Get_PostProcessPipeline();
    if (!pipeline)
    {
        return;
    }

    pipeline->Set_PostProcessFeatures(
        PostProcessFeature::HDR |
        PostProcessFeature::Exposure |
        PostProcessFeature::ToneMapping |
        PostProcessFeature::Bloom |
        PostProcessFeature::Gamma |
        PostProcessFeature::LightShaft);

    pipeline->Set_PostProcessParams(pipeline->Get_PostProcessParams());
    pipeline->Set_LightShaftParams(pipeline->Get_LightShaftParams());
}

void MapEditorApp::Apply_DefaultShadowSettings()
{
    ShadowLightDesc shadowDesc{};

    shadowDesc.eye = Vec3(0.f, 15.f, -7.f);
    shadowDesc.at = Vec3(0.f, 0.f, 0.f);

    // Camera 따라다니게
    shadowDesc.followCameraFrustum = true;
    shadowDesc.focusNear = 0.1f;
    shadowDesc.focusFar = 110.f;
    shadowDesc.lightDistancePadding = 30.f;
    // 그림자용 ortho projection 범위 여유 값
    shadowDesc.xyPadding = 2.f;
    shadowDesc.zPadding = 40.f;

    // Shadow map 해상도. 4096 * 4096 * 4 * 4byte = 256mb
    shadowDesc.shadowMapWidth = 4096;
    shadowDesc.shadowMapHeight = 4096;

    shadowDesc.cascadeSplitLambda = 0.8f;
    shadowDesc.bias = 0.0002f;
    shadowDesc.intensity = 0.45f;

    GAME->Update_ShadowLight(shadowDesc);

    // 카메라 close/normal/far 모드에 따라서 그림자 구간을 동적 조절하기 위한 프리셋 설정
    ShadowCascadePresetSet cascadePresets{};
    cascadePresets.close.focusNear = 0.1f;
    cascadePresets.close.focusFar = 28.f;
    cascadePresets.close.cascadeSplitLambda = 0.95f;
    cascadePresets.close.cascadeBlendDistance = 0.8f;

    cascadePresets.normal.focusNear = 0.1f;
    cascadePresets.normal.focusFar = 110.f;
    cascadePresets.normal.cascadeSplitLambda = 0.8f;
    cascadePresets.normal.cascadeBlendDistance = 1.7f;

    cascadePresets.farRange.focusNear = 0.1f;
    cascadePresets.farRange.focusFar = 180.f;
    cascadePresets.farRange.cascadeSplitLambda = 0.55f;
    cascadePresets.farRange.cascadeBlendDistance = 3.f;

    GAME->Set_ShadowCascadePresets(cascadePresets);
    GAME->Set_ShadowCascadeLevel(CascadeLevel::Normal);
}

wstring MapEditorApp::Get_ClientResourceRoot() const
{
    wchar_t modulePath[MAX_PATH] = {};
    if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
    {
        return {};
    }

    const fs::path runtimeRoot = fs::path(modulePath).parent_path();
    return fs::absolute(runtimeRoot / L"../../../Client/Bin/Resources/").lexically_normal().wstring();
}

HRESULT MapEditorApp::Build_TerrainFromScene()
{
    if (!_scene._terrainConfig.IsValid())
    {
        return S_OK;
    }

    const auto& cfg = _scene._terrainConfig;

    Engine::VIBuffer_TerrainUE::TerrainDesc desc{};
    desc.worldOffsetX = cfg.worldOffsetX;
    desc.worldOffsetY = cfg.worldOffsetY;
    desc.worldOffsetZ = cfg.worldOffsetZ;
    desc.scaleXZ = cfg.scaleXZ;
    desc.scaleY = cfg.scaleY;

    const wstring folder = String::ToWString(cfg.heightmapFolder);
    for (const auto& t : cfg.tiles)
    {
        Engine::VIBuffer_TerrainUE::TileDesc td;
        td.heightmapPath = folder + String::ToWString(t.file);
        td.gridX = t.gridX;
        td.gridY = t.gridY;
        desc.tiles.push_back(td);
    }

    _terrain = METerrainActor::Create(_device, _context, desc);
    if (!_terrain)
    {
        LOG_WARN("Build_TerrainFromScene: Failed to create METerrainActor");
        return E_FAIL;
    }

    return S_OK;
}

Unique<MapEditorApp> MapEditorApp::Create()
{
    auto instance = make_unique<MapEditorApp>();

    if (FAILED(instance->Initialize()))
    {
        MSG_BOX("Failed to Create MapEditorApp");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void MapEditorApp::Free()
{
    _instanceCullShader.reset();
    _landscapeManager.reset();
    _materialApplicator.reset();
    //test
    _terrain.reset();
    //
    _scene.ClearScene();
    _modelManager.reset();

    __super::Free();

    _viewport.reset();
    _rt.reset();
    _pickingSystem.reset();
    _debugRenderer.reset();
    _bridgeCamera.reset();
    _camera.reset();
    _manager.reset();
    _imguiManager.reset();
    _context.Reset();
    _device.Reset();

    GameInstance::DestroyInstance();
}
