#include "pch.h"
#include "MapEditorApp.h"
#include "GameInstance.h"
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
#include "CmdDeleteObject.h"
#include "CmdTransformObject.h"
#include "MEInstancedMeshActor.h"
#include "MEBridgeCamera.h"
#include "ComputeShaderCom.h"
#include "MELandscapeActor.h"

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

    _camera = MECamera::Create(60.f, static_cast<float>(g_iWinSizeX) / g_iWinSizeY, 0.1f, 1000.f);

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

    LandscapePanelContext landscapeCtx{};
    landscapeCtx.sculptState = _sculptState;
    landscapeCtx.onAdd = [this](const LandscapeConfig& cfg) { Add_Landscape(cfg); };
    landscapeCtx.onRemove = [this](int32 idx) { Remove_Landscape(idx); };

    _manager = MEManager::Create(_viewport, &_scene, _camera.get(), landscapeCtx);
    CHECK_NULL(_manager, E_FAIL);

    _pickingSystem = MEPickingSystem::Create(_device, _context, g_iWinSizeX, g_iWinSizeY);
    CHECK_NULL(_pickingSystem, E_FAIL);
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


    CHECK_FAILED(GAME->Add_Prototype(0, L"MEInstancedMeshActor",
        MEInstancedMeshActor::Create(_device, _context)), E_FAIL);

    _instanceCullShader = ComputeShaderCom::Create(_device, _context,
        L"../../../Client/Bin/ShaderFiles/Compute/Shader_Compute_InstanceCull.hlsl",
        "CS_Main");
    CHECK_NULL(_instanceCullShader, E_FAIL);


    CHECK_FAILED(Ready_Light(), E_FAIL);

    return S_OK;
}

void MapEditorApp::Priority_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void MapEditorApp::Update(float timeDelta)
{
    GAME->Update_Engine(timeDelta);
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

    if (_viewport->ConsumeCursorSet(mousePos, vpPos, vpSize))
    {
        _pickingSystem->SetCursorAt(_scene, *_camera, mousePos, vpPos, vpSize, *_rt);
    }

    ImVec2 sculptMouse{}, sculptVpPos{}, sculptVpSize{};
    if (_viewport->ConsumeSculpt(sculptMouse, sculptVpPos, sculptVpSize))
    {
        Do_SculptAt(sculptMouse, sculptVpPos, sculptVpSize, 0.016f);
    }

    string spawnPath{};
    ImVec2 spawnMouse{}, spawnVpPos{}, spawnVpSize{};
    if (_viewport->ConsumeSpawn(spawnPath, spawnMouse, spawnVpPos, spawnVpSize))
    {
        DoSpawnAt(spawnPath);
    }

    SyncSceneLights();

    GAME->UpdateFrustum(_camera->GetView(), _camera->GetProj());

    vector<uint32> visibleIndices;
    GAME->QueryVisible(visibleIndices);

    for (uint32 idx : visibleIndices)
    {
        auto& obj = _scene._staticMeshes[idx];
        if (!obj->_renderHandle)
        {
            continue;
        }

        obj->_renderHandle->Set_CameraFar(_camera->GetFar());
        obj->_renderHandle->Late_Update(0.f);
    }

    for (auto& obj : _scene._instancedMeshes)
    {
        if (!obj->_instancedRenderHandle)
        {
            continue;
        }

        obj->_instancedRenderHandle->Set_CameraFar(_camera->GetFar());
        obj->_instancedRenderHandle->Late_Update(0.f);
    }

    // test
    if (_terrain)
    {
        _terrain->Set_CameraFar(_camera->GetFar());
        _terrain->Late_Update(0.f);
    }

    for (auto& landscape : _scene._landscapes)
    {
        landscape->Set_CameraFar(_camera->GetFar());
        landscape->Late_Update(0.f);
    }

    GAME->Bind_BackBuffer();
    CHECK_FAILED(GAME->Clear_Buffers(Vec4(0.1f, 0.1f, 0.1f, 1.f)), E_FAIL);
    CHECK_FAILED(GAME->Draw_RendererOnly(), E_FAIL);

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
    Apply_PassOverrides();
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

        const string bundleKey = Resolve_BundleKey(obj->_modelPath);
        Apply_MaterialFromJson(obj->_modelPath, bundleKey, model);

        const string modelFilename = String::ToString(fs::path(obj->_modelPath).filename().wstring());
        Apply_PassOverrideToModel(*model, modelFilename, obj->_position, String::ToString(obj->_modelPath));

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

        if (!obj->_instancedRenderHandle)
        {
            obj->_instancedRenderHandle = static_pointer_cast<MEInstancedMeshActor>(
                GAME->Clone_Prototype(Prototype::GameObject, 0, L"MEInstancedMeshActor", nullptr));

            if (!obj->_instancedRenderHandle)
            {
                continue;
            }

            obj->_instancedRenderHandle->Set_CullShader(_instanceCullShader);
        }

        if (obj->_instancedModelPath.empty())
        {
            continue;
        }

        obj->_instancedRenderHandle->UpdateInstances(
            obj->_instancedModelPath, obj->_instancedTransforms, _modelManager.get());

        auto model = obj->_instancedRenderHandle->GetModel();
        if (!model)
        {
            continue;
        }

        const string bundleKey = Resolve_BundleKey(obj->_instancedModelPath);
        Apply_MaterialFromJson(obj->_instancedModelPath, bundleKey, model);

        const string modelFilename = String::ToString(fs::path(obj->_instancedModelPath).filename().wstring());
        Apply_PassOverrideToModel(*model, modelFilename);
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
        spawned = _scene.SpawnObject(String::ToWString(modelPath), _scene._cursorPos);
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
    }
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

void MapEditorApp::Apply_MaterialFromJson(const fs::path& modelPath, const string& bundleKey, Shared<ModelCom>& model)
{
    fs::path jsonPath = modelPath;
    jsonPath.replace_extension(L".json");

    ifstream ifs(jsonPath);
    if (!ifs.is_open())
    {
        return;
    }

    try
    {
        json modelJson = json::parse(ifs);
        for (auto& entry : modelJson)
        {
            if (entry.value("Type", "") != "StaticMesh")
            {
                continue;
            }

            if (!entry.contains("Properties") || !entry["Properties"].contains("StaticMaterials"))
            {
                break;
            }

            const auto& staticMats = entry["Properties"]["StaticMaterials"];
            for (uint32 i = 0; i < static_cast<uint32>(staticMats.size()); ++i)
            {
                const auto& mat = staticMats[i];
                if (!mat.contains("MaterialInterface"))
                {
                    continue;
                }

                const auto& mi = mat["MaterialInterface"];
                if (!mi.is_object())
                {
                    continue;
                }

                string objectPath = mi.value("ObjectPath", "");
                if (objectPath.empty())
                {
                    continue;
                }

                size_t dot = objectPath.find('.');
                string packagePath = (dot != string::npos) ? objectPath.substr(0, dot) : objectPath;
                size_t slash = packagePath.rfind('/');
                string miName = (slash != string::npos) ? packagePath.substr(slash + 1) : packagePath;

                model->Apply_MaterialByAssetName(i, bundleKey, miName);
            }
            break;
        }
    }
    catch (...) {}
}

string MapEditorApp::Resolve_BundleKey(const fs::path& modelPath) const
{
    string bundleKey = {};
    if (!_scene._modelFolder.empty())
    {
        bundleKey = String::ToString(_scene._modelFolder);
    }
    else
    {
        bundleKey = String::ToString(modelPath.parent_path().filename().wstring());
    }
    transform(bundleKey.begin(), bundleKey.end(), bundleKey.begin(), ::tolower);
    return bundleKey;
}

void MapEditorApp::Apply_PassOverrideToModel(ModelCom& model, const string& modelFilename, Vec3 worldPos, const string& fullModelPath)
{
    auto it = _scene._modelOverrides.find(modelFilename);
    int32 passIndex = (it != _scene._modelOverrides.end()) ? it->second.passIndex : -1;

    uint32 matSize = static_cast<uint32>(model.Get_NumMaterials());
    for (uint32 i = 0; i < matSize; ++i)
    {
        auto mat = model.Get_Material(i);
        if (!mat)
        {
            continue;
        }

        mat->Set_ForcedPassIndex(passIndex);

        if (it != _scene._modelOverrides.end())
        {
            for (auto& [slotIdx, texPath] : it->second.textureSlots)
            {
                if (texPath.empty())
                {
                    continue;
                }
                string fullPath = string(_mapSavePath) + texPath;
                auto slot = static_cast<MaterialTextureSlot>(slotIdx);
                mat->Override_TextureSlot(slot, 0, String::ToWString(fullPath));
            }

            if (it->second.passIndex == static_cast<int32>(VtxMeshPassIndex::LandscapeBlendPass))
            {
                auto& cb = mat->Get_CB();
                cb.uvOffset = Vec2(worldPos.x, worldPos.z);

                string guid = fs::path(fullModelPath).parent_path().filename().string();
                if (!guid.empty())
                {
                    string guidJsonPath = string(_mapSavePath) + "OriginJson/" +
                        String::ToString(_scene._modelFolder) + "/_Generated_/" + guid + ".json";

                    ifstream jsonFile(guidJsonPath);
                    if (jsonFile.is_open())
                    {
                        try
                        {
                            json j = json::parse(jsonFile);

                            string wmRef = j["Textures"]["Weightmap0"].get<string>();
                            size_t lastDot = wmRef.rfind('.');
                            if (lastDot != string::npos)
                            {
                                string wmName = wmRef.substr(lastDot + 1);
                                string weightmapPath = string(_mapSavePath) + "OriginJson/" +
                                    String::ToString(_scene._modelFolder) + "/_Generated_/" + wmName + ".dds";
                                mat->Override_TextureSlot(static_cast<MaterialTextureSlot>(6), 0, String::ToWString(weightmapPath));
                            }

                            Vec4 layer1Mask = Vec4(1.f, 0.f, 0.f, 0.f);
                            Vec4 layer2Mask = Vec4(0.f, 0.f, 0.f, 0.f);

                            if (j.contains("Parameters") && j["Parameters"].contains("Colors"))
                            {
                                auto& colors = j["Parameters"]["Colors"];
                                if (colors.contains("LayerMask_Layer1"))
                                {
                                    auto& c = colors["LayerMask_Layer1"];
                                    layer1Mask = Vec4(c["R"].get<float>(), c["G"].get<float>(), c["B"].get<float>(), c["A"].get<float>());
                                }
                                if (colors.contains("LayerMask_Layer2"))
                                {
                                    auto& c = colors["LayerMask_Layer2"];
                                    layer2Mask = Vec4(c["R"].get<float>(), c["G"].get<float>(), c["B"].get<float>(), c["A"].get<float>());
                                }
                            }

                            cb.baseColorTint = layer1Mask;
                            cb.emissiveColor = layer2Mask;
                        }
                        catch (...) {}
                    }
                }
            }
        }
    }
}

void MapEditorApp::Apply_PassOverrides()
{
    if (!_scene._dirtyPassOverrides)
    {
        return;
    }

    _scene._dirtyPassOverrides = false;

    for (auto& obj : _scene._staticMeshes)
    {
        if (!obj->_renderHandle)
        {
            continue;
        }

        auto model = obj->_renderHandle->GetModel();
        if (!model)
        {
            continue;
        }

        const string modelFilename = String::ToString(fs::path(obj->_modelPath).filename().wstring());
        Apply_PassOverrideToModel(*model, modelFilename, obj->_position, String::ToString(obj->_modelPath));
    }

    for (auto& obj : _scene._instancedMeshes)
    {
        if (!obj->_instancedRenderHandle)
        {
            continue;
        }

        auto model = obj->_instancedRenderHandle->GetModel();
        if (!model)
        {
            continue;
        }

        const string modelFilename = String::ToString(fs::path(obj->_instancedModelPath).filename().wstring());
        Apply_PassOverrideToModel(*model, modelFilename);
    }
}

void MapEditorApp::Add_Landscape(const LandscapeConfig& cfg)
{
    VIBuffer_Landscape::LAND_DESC desc{};
    desc.countX = cfg.countX;
    desc.countZ = cfg.countZ;
    desc.sizeX = cfg.sizeX;
    desc.sizeZ = cfg.sizeZ;

    auto actor = MELandscapeActor::Create(_device, _context, desc);
    if (!actor)
    {
        LOG_WARN("Add_Landscape: Failed to create MELandscapeActor");
        return;
    }

    actor->Get_Transform()->Set_Position(Vec3(cfg.worldOffsetX, cfg.worldOffsetY, cfg.worldOffsetZ));

    if (!cfg.heights.empty())
    {
        auto& heights = actor->Get_VIBuffer()->Get_Heights();
        heights = cfg.heights;
        actor->Get_VIBuffer()->Flush();
    }

    _scene._landscapes.push_back(actor);
    _scene._activeLandscapeIdx= static_cast<int32>(_scene._landscapes.size()) - 1;
}

void MapEditorApp::Remove_Landscape(int32 idx)
{
    if (idx < 0 || idx >= static_cast<int32>(_scene._landscapes.size()))
    {
        return;
    }

    _scene._landscapes.erase(_scene._landscapes.begin() + idx);

    if (_scene._activeLandscapeIdx>= static_cast<int32>(_scene._landscapes.size()))
    {
        _scene._activeLandscapeIdx= static_cast<int32>(_scene._landscapes.size());
    }
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

void MapEditorApp::Do_SculptAt(ImVec2 mousePos, ImVec2 vpPos, ImVec2 vpSize, float timeDelta)
{
    if (_scene._activeLandscapeIdx <= 0 || _scene._activeLandscapeIdx >= static_cast<int32>(_scene._landscapes.size()))
    {
        return;
    }

    auto& actor = _scene._landscapes[_scene._activeLandscapeIdx];
    auto vb = actor->Get_VIBuffer();
    if (!vb)
    {
        return;
    }

    float u = (mousePos.x - vpPos.x) / vpSize.x;
    float v = (mousePos.y - vpPos.y) / vpSize.y;
    float ndcX = u * 2.f - 1.f;
    float ndcY = 1.f - v * 2.f;

    Matrix viewProjInv = (Matrix(_camera->GetView()) * Matrix(_camera->GetProj())).Invert();
    Vec3 nearP = Vec3::Transform(Vec3(ndcX, ndcY, 0.f), viewProjInv);
    Vec3 farP = Vec3::Transform(Vec3(ndcX, ndcY, 1.f), viewProjInv);

    Vec3 rayDir = farP - nearP;
    rayDir.Normalize();

    Vec3 rayOrigin{}, rayDirection{};
    rayOrigin = nearP;
    rayDirection = rayDir;

    // ray & land
    Vec3 landscapePos = actor->Get_Transform()->Get_Position();
    float stepSize = min(vb->Get_SizeX(), vb->Get_SizeZ()) * 0.5f;
    Vec3 hitPos = {};
    bool hit = false;

    for (float i = 0.f; i < 5000.f; i += stepSize)
    {
        Vec3 p = {
          rayOrigin.x + rayDirection.x * i,
          rayOrigin.y + rayDirection.y * i,
          rayOrigin.z + rayDirection.z * i
        };

        float localX = p.x - landscapePos.x;
        float localZ = p.z - landscapePos.z;
        float terrainY = vb->Sample_Height(localX, localZ) + landscapePos.y;

        if (p.y <= terrainY)
        {
            hitPos = p;
            hit = true;
            break;
        }
    }

    if (!hit)
    {
        return;
    }

    const float radius = _sculptState->brushRadius;
    const float strength = _sculptState->brushStrength;
    const ESculptMode mode = _sculptState->mode;
    const float flatH = _sculptState->flattenHeight;

    const uint32 numVX = vb->Get_CountX() + 1;
    const uint32 numVZ = vb->Get_CountZ() + 1;
    auto& heights = vb->Get_Heights();

    float localHitX = hitPos.x - landscapePos.x;
    float localHitZ = hitPos.z - landscapePos.z;

    if (mode == ESculptMode::Smooth)
    {
        vector<float> smoothed = heights;
        for (uint32 z = 0; z < numVZ; ++z)
        {
            for (uint32 x = 0; x < numVX; ++x)
            {
                float vx = x * vb->Get_SizeX();
                float vz = z * vb->Get_SizeZ();
                float dist = sqrtf((vx - localHitX) * (vx - localHitX) + (vz - localHitZ) * (vz - localHitZ));

                if (dist >= radius)
                {
                    continue;
                }

                float sum = 0.f;
                int cnt = 0;
                for (int dz = -1; dz <= 1; ++dz)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        int nx = (int)x + dx;
                        int nz = (int)z + dz;
                        if (nx >= 0 && nx < (int)numVX && nz >= 0 && nz < (int)numVZ)
                        {
                            sum += heights[nz * numVX + nx];
                            ++cnt;
                        }
                    }
                }

                float falloff = 1.f - (dist / radius);
                float avg = sum / cnt;
                uint32 idx = z * numVX + x;
                smoothed[idx] = heights[idx] + (avg - heights[idx]) * falloff * strength * timeDelta * 10.f;
            }
        }
        heights = smoothed;
    }
    else
    {
        for (uint32 z = 0; z < numVZ; ++z)
        {
            for (uint32 x = 0; x < numVX; ++x)
            {
                float vx = x * vb->Get_SizeX();
                float vz = z * vb->Get_SizeZ();
                float dist = sqrtf((vx - localHitX) * (vx - localHitX) + (vz - localHitZ) * (vz - localHitZ));
                if (dist >= radius)
                {
                    continue;
                }

                float falloff = 1.f - (dist / radius);
                float delta = strength * falloff * timeDelta * 10.f;
                uint32  idx = z * numVX + x;

                switch (mode)
                {
                case ESculptMode::Raise:
                    heights[idx] += delta;
                    break;
                case ESculptMode::Lower:
                    heights[idx] -= delta;
                    break;
                case ESculptMode::Flatten:
                    heights[idx] += (flatH - heights[idx]) * delta;
                    break;
                default:
                    break;
                }
            }
        }
    }
    vb->Flush();
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
