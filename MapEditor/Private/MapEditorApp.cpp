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

    _imguiManager = MEImGuiManager::Create(g_hWnd, _device, _context);
    CHECK_NULL(_imguiManager, E_FAIL);

    _camera = MECamera::Create(60.f, static_cast<float>(g_iWinSizeX) / g_iWinSizeY, 0.1f, 1000.f);

    _rt = RenderTarget::Create(_device, _context, g_iWinSizeX, g_iWinSizeY, DXGI_FORMAT_R8G8B8A8_UNORM, Vec4(0.1f, 0.1f, 0.1f, 1.f));
    CHECK_NULL(_rt, E_FAIL);

    _meshShader = ShaderCom::Create(_device, _context,
        L"../../../Client/Bin/ShaderFiles/Shader_VtxMesh.hlsl",
        VTXMESH::Elements, VTXMESH::numElements, ShaderType::VtxMesh);
    CHECK_NULL(_meshShader, E_FAIL);

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

    _deferredShader = ShaderCom::Create(_device, _context,
        L"../../../Client/Bin/ShaderFiles/Shader_Deferred.hlsl",
        VTXTEX::Elements, VTXTEX::numElements, ShaderType::VtxTex);
    CHECK_NULL(_deferredShader, E_FAIL);

    _rectBuffer = VIBuffer_Rect::Create(_device, _context);
    CHECK_NULL(_rectBuffer, E_FAIL);

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
    UpdateRenderHandles();
    RebuildOctreeIfDirty();
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

    CHECK_FAILED(GAME->Begin_MRT(L"MRT_GameObjects", true), E_FAIL);
    {
        float farZ = _camera->GetFar();
        _meshShader->Bind_RawValue("g_fFar", &farZ, sizeof(float));

        for (auto& obj : _scene._objects)
        {
            if (obj->_type != EObjectType::StaticMesh || !obj->_renderHandle)
            {
                continue;
            }

            const auto& model = obj->_renderHandle->GetModel();
            Vec3 localMin, localMax;
            if (model->Try_Get_LocalBounds(localMin, localMax))
            {
                BoundingBox localBox, worldBox;
                BoundingBox::CreateFromPoints(localBox, XMLoadFloat3(&localMin), XMLoadFloat3(&localMax));
                localBox.Transform(worldBox, model->Get_PreLocalTransformMatrix() * obj->_worldMatrix);
                if (!GAME->IsVisible(worldBox))
                {
                    continue;
                }
            }
            obj->_renderHandle->Render(obj->_worldMatrix);
        }
        RenderStaticMeshes();
        RenderInstancedMeshes();
    }
    CHECK_FAILED(GAME->End_MRT(), E_FAIL);

    CHECK_FAILED(GAME->Begin_MRT(L"MRT_LightAcc"), E_FAIL);
    {
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_NormalTexture", L"Target_Normal");
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_DepthTexture", L"Target_Depth");
        GAME->Bind_CamPosition(_deferredShader.get(), "g_CamPosition");

        Matrix view = *GAME->Get_Transform(D3DTS::View);
        Matrix proj = *GAME->Get_Transform(D3DTS::Proj);
        Matrix viewInv = view.Invert();
        Matrix projInv = proj.Invert();
        _deferredShader->Bind_Matrix("g_ViewMatrixInverse", &viewInv);
        _deferredShader->Bind_Matrix("g_ProjMatrixInverse", &projInv);

        const float farZ = _camera->GetFar();
        _deferredShader->Bind_RawValue("g_Far", &farZ, sizeof(float));

        Matrix rectWorld = Matrix::CreateScale(2.f, 2.f, 1.f);
        Matrix identity = Matrix::Identity;
        _deferredShader->Bind_Matrix("g_WorldMatrix", &rectWorld);
        _deferredShader->Bind_Matrix("g_ViewMatrix", &identity);
        _deferredShader->Bind_Matrix("g_ProjMatrix", &identity);
        _rectBuffer->Bind_Resources();

        GAME->Render_Lights(_deferredShader, _rectBuffer);
    }
    CHECK_FAILED(GAME->End_MRT(), E_FAIL);

    CHECK_FAILED(_rt->Begin(), E_FAIL);
    {
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_DiffuseTexture", L"Target_Diffuse");
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_ShadeTexture", L"Target_Shade");
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_SpecularTexture", L"Target_Specular");

        Matrix rectWorld = Matrix::CreateScale(2.f, 2.f, 1.f);
        Matrix identity = Matrix::Identity;
        _deferredShader->Bind_Matrix("g_WorldMatrix", &rectWorld);
        _deferredShader->Bind_Matrix("g_ViewMatrix", &identity);
        _deferredShader->Bind_Matrix("g_ProjMatrix", &identity);

        _deferredShader->Begin(4); // CombinedSimple pass
        _rectBuffer->Bind_Resources();
        _rectBuffer->Render();

        _debugRenderer->Draw(_scene);
    }

    GAME->Render_DebugDepth();
    GAME->Render_DebugOverlay();
    GAME->Bind_BackBuffer();
    CHECK_FAILED(GAME->Clear_Buffers(Vec4(0.1f, 0.1f, 0.1f, 1.f)), E_FAIL);
    GAME->Bind_BackBuffer();

    _manager->Render();
    _imguiManager->Render();

    CHECK_FAILED(GAME->Present(), E_FAIL);

    return S_OK;
}

void MapEditorApp::UpdateRenderHandles()
{
    if (_scene._dirtyRenderHandles && _meshShader)
    {
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

            obj->_renderHandle = MEStaticMeshActor::Create(model, _meshShader);
        }

        for (auto& obj : _scene._instancedMeshes)
        {
            if (!obj->_instancedParamsDirty)
            {
                continue;
            }

            obj->_instancedParamsDirty = false;

            if (!obj->_instancedRenderHandle)
            {
                obj->_instancedRenderHandle = MEInstancedMeshActor::Create(_device, _context);
                if (!obj->_instancedRenderHandle)
                {
                    continue;
                }
            }

            if (!obj->_instancedModelPath.empty())
            {
                obj->_instancedRenderHandle->UpdateInstances(
                    obj->_instancedModelPath,
                    obj->_position,
                    obj->_instancedAreaWidth,
                    obj->_instancedAreaDepth,
                    obj->_instancedDensity,
                    obj->_instancedSeed);
            }
        }
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

void MapEditorApp::RenderStaticMeshes()
{
    float farZ = _camera->GetFar();
    _meshShader->Bind_RawValue("g_fFar", &farZ, sizeof(float));

    vector<uint32> visibleIndices;
    GAME->QueryVisible(visibleIndices);

    for (uint32 idx : visibleIndices)
    {
        auto& obj = _scene._staticMeshes[idx];
        if (!obj->_renderHandle)
        {
            continue;
        }

        obj->_renderHandle->Render(obj->_worldMatrix);
    }
}

void MapEditorApp::RenderInstancedMeshes()
{
    float farZ = _camera->GetFar();

    for (auto& obj : _scene._instancedMeshes)
    {
        if (!obj->_instancedRenderHandle)
        {
            continue;
        }
        obj->_instancedRenderHandle->Render(farZ);
    }
}

void MapEditorApp::SyncSceneLights()
{
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
    _scene.Add(obj);
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
    _scene.ClearScene();
    _meshShader.reset();
    _deferredShader.reset();
    _rectBuffer.reset();
    _modelManager.reset();

    __super::Free();

    _viewport.reset();
    _rt.reset();
    _pickingSystem.reset();
    _debugRenderer.reset();
    _camera.reset();
    _manager.reset();
    _imguiManager.reset();
    _context.Reset();
    _device.Reset();

    GameInstance::DestroyInstance();
}
