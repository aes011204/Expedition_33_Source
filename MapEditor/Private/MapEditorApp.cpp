#include "pch.h"
#include "MapEditorApp.h"
#include "GameInstance.h"
#include "RenderTarget.h"

#include "MEImGuiManager.h"
#include "MEManager.h"
#include "MECamera.h"
#include "PanelViewport.h"
#include "MEStaticMeshActor.h"
#include "MEModelManager.h"


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
        L"../ShaderFiles/Shader_VtxMesh.hlsl",
        VTXMESH::Elements, VTXMESH::numElements);
    CHECK_NULL(_meshShader, E_FAIL);

    _modelManager = MEModelManager::Create();
    CHECK_NULL(_modelManager, E_FAIL);

    _viewport = PanelViewport::Create(_camera, _rt, &_scene);
    CHECK_NULL(_viewport, E_FAIL);

    _manager = MEManager::Create(_viewport, &_scene, _camera.get());
    CHECK_NULL(_manager, E_FAIL);

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
    _manager->Update(timeDelta);
    UpdateRenderHandles();
}

void MapEditorApp::Late_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

HRESULT MapEditor::MapEditorApp::Render()
{
    _camera->Apply();

    CHECK_FAILED(_rt->Begin(), E_FAIL);
    DrawSceneDebug();

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

void MapEditorApp::DrawSceneDebug()
{
    for (auto& obj : _scene._objects)
    {
        const bool isSelected = (obj == _scene._selected);
        const Color color = isSelected ? Color(1.f, 1.f, 0.f, 1.f) : Color(0.f, 1.f, 0.f, 1.f);

        switch (obj->_type)
        {
        case EObjectType::EncounterZone:
        case EObjectType::TriggerVolume:
        case EObjectType::CameraZone:
        case EObjectType::PostProcessVolume:
        {
            if (obj->_zoneShape == EZoneShape::Box)
            {
                DebugBoxDesc desc;
                desc.center = obj->_position;
                desc.extents = obj->_zoneExtent;
                desc.rotation = obj->_rotation;
                desc.style.color = color;
                desc.style.depthEnabled = false;
                GAME->Draw_DebugBox(desc);
            }
            else
            {
                DebugSphereDesc desc;
                desc.center = obj->_position;
                desc.radius = obj->_zoneRadius;
                desc.style.color = color;
                desc.style.depthEnabled = false;
                GAME->Draw_DebugSphere(desc);
            }
            break;
        }

        case EObjectType::SpawnPoint:
        {
            DebugSphereDesc sphere;
            sphere.center = obj->_position;
            sphere.radius = 0.3f;
            sphere.style.color = isSelected ? Color(1.f, 1.f, 0.f, 1.f) : Color(0.f, 0.5f, 1.f, 1.f);
            sphere.style.depthEnabled = false;
            GAME->Draw_DebugSphere(sphere);

            DebugLineDesc line;
            line.start = obj->_position;
            line.end = obj->_position + Vec3(0.f, 1.f, 0.f);
            line.style.color = sphere.style.color;
            line.style.depthEnabled = false;
            GAME->Draw_DebugLine(line);
            break;
        }

        case EObjectType::Light:
        {
            DebugSphereDesc sphere;
            sphere.center = obj->_position;
            sphere.radius = 0.2f;
            sphere.style.color = isSelected ? Color(1.f, 1.f, 0.f, 1.f) : Color(1.f, 0.9f, 0.3f, 1.f);
            sphere.style.depthEnabled = false;
            GAME->Draw_DebugSphere(sphere);

            if (obj->_lightType == LightType::Directional)
            {
                DebugLineDesc line;
                line.start = obj->_position;
                line.end = obj->_position + Vec3(obj->_lightDirection.x, obj->_lightDirection.y, obj->_lightDirection.z) * 2.f;
                line.style.color = sphere.style.color;
                line.style.depthEnabled = false;
                GAME->Draw_DebugLine(line);
            }
            else
            {
                DebugSphereDesc range;
                range.center = obj->_position;
                range.radius = obj->_lightRange;
                range.style.color = Color(1.f, 0.9f, 0.3f, 0.3f);
                range.style.depthEnabled = false;
                GAME->Draw_DebugSphere(range);
            }
            break;
        }

        case EObjectType::StaticMesh:
            if (obj->_renderHandle)
            {
                obj->_renderHandle->Render(obj->_worldMatrix);
            }
            break;

        default:
            break;
        }
    }
}

void MapEditorApp::UpdateRenderHandles()
{
    if (!_scene._dirtyRenderHandles || !_meshShader)
    {
        return;
    }
    _scene._dirtyRenderHandles = false;

    for (auto& obj : _scene._objects)
    {
        if (obj->_type != EObjectType::StaticMesh)
        {
            continue;
        }

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

        obj->_renderHandle = MEStaticMeshActor::Create(_device, _context, obj->_modelPath, _meshShader);
    }
}

Unique<MapEditorApp> MapEditorApp::Create()
{
    Unique<MapEditorApp> instance{ new MapEditorApp() };

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
    _scene._selected.reset();
    _scene._objects.clear();
    _meshShader.reset();
    _modelManager.reset();

    __super::Free();

    _viewport.reset();
    _rt.reset();
    _camera.reset();
    _manager.reset();
    _imguiManager.reset();
    _context.Reset();
    _device.Reset();

    GameInstance::DestroyInstance();
}
