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

    _manager = MEManager::Create(_viewport, &_scene, _camera.get());
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
        Apply_PassOverrideToModel(*model, modelFilename);

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

void MapEditorApp::Apply_PassOverrideToModel(ModelCom& model, const string& modelFilename)
{
    auto it = _scene._modelOverrides.find(modelFilename);
    int32 passIndex = (it != _scene._modelOverrides.end()) ? it->second : -1;

    for (uint32 i = 0; i < static_cast<uint32>(model.Get_NumMaterials()); ++i)
    {
        auto mat = model.Get_Material(i);
        if (mat)
        {
            mat->Set_ForcedPassIndex(passIndex);
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
        Apply_PassOverrideToModel(*model, modelFilename);
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
