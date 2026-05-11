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
        VTXMESH::Elements, VTXMESH::numElements, ShaderType::VtxMesh);
    CHECK_NULL(_meshShader, E_FAIL);

    _modelManager = MEModelManager::Create();
    CHECK_NULL(_modelManager, E_FAIL);

    _viewport = PanelViewport::Create(_camera, _rt, &_scene);
    CHECK_NULL(_viewport, E_FAIL);

    _manager = MEManager::Create(_viewport, &_scene, _camera.get());
    CHECK_NULL(_manager, E_FAIL);

    CHECK_FAILED(Ready_PickingRT(), E_FAIL);

    _deferredShader = ShaderCom::Create(_device, _context,
        L"../ShaderFiles/Shader_Deferred.hlsl",
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
    _manager->Update(timeDelta);
    UpdateRenderHandles();
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
        DoPickAt(mousePos, vpPos, vpSize);
    }

    if (_viewport->ConsumeCursorSet(mousePos, vpPos, vpSize))
    {
        SetCursorAt(mousePos, vpPos, vpSize);
    }

    string spawnPath{};
    ImVec2 spawnMouse{}, spawnVpPos{}, spawnVpSize{};
    if (_viewport->ConsumeSpawn(spawnPath, spawnMouse, spawnVpPos, spawnVpSize))
    {
        DoSpawnAt(spawnPath);
    }

    SyncSceneLights();

    CHECK_FAILED(GAME->Begin_MRT(L"MRT_GameObjects"), E_FAIL);
    {
        float farZ = _camera->GetFar();
        _meshShader->Bind_RawValue("g_fCameraFar", &farZ, sizeof(float));

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
    }
    CHECK_FAILED(GAME->End_MRT(), E_FAIL);

    CHECK_FAILED(GAME->Begin_MRT(L"MRT_LightAcc"), E_FAIL);
    {
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_NormalTexture", L"Target_Normal");
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_DepthTexture", L"Target_Depth");
        GAME->Bind_CamPosition(_deferredShader.get(), "g_vCamPosition");

        Matrix view = *GAME->Get_Transform(D3DTS::View);
        Matrix proj = *GAME->Get_Transform(D3DTS::Proj);
        Matrix viewInv = view.Invert();
        Matrix projInv = proj.Invert();
        _deferredShader->Bind_Matrix("g_ViewMatrixInverse", &viewInv);
        _deferredShader->Bind_Matrix("g_ProjMatrixInverse", &projInv);

        const float farZ = _camera->GetFar();
        _deferredShader->Bind_RawValue("g_fFar", &farZ, sizeof(float));

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


        DrawSceneDebug();
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
            break;
        default:
            break;
        }
    }

    if (_scene._cursorVisible)
    {
        auto line = [&](Vec3 a, Vec3 b)
            {
                DebugLineDesc d;
                d.start = a;
                d.end = b;
                d.style.color = _debugLineCol;
                d.style.depthEnabled = false;
                GAME->Draw_DebugLine(d);
            };

        line(_scene._cursorPos - Vec3(_debugLinePos, 0.f, 0.f), _scene._cursorPos + Vec3(_debugLinePos, 0.f, 0.f));
        line(_scene._cursorPos - Vec3(0.f, _debugLinePos, 0.f), _scene._cursorPos + Vec3(0.f, _debugLinePos, 0.f));
        line(_scene._cursorPos - Vec3(0.f, 0.f, _debugLinePos), _scene._cursorPos + Vec3(0.f, 0.f, _debugLinePos));
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

        obj->_renderHandle = MEStaticMeshActor::Create(model, _meshShader);
    }
}

void MapEditorApp::DoPickAt(const ImVec2& mousePos, const ImVec2& vpPos, const ImVec2& vpSize)
{
    if (!_idRT || !_idStagingTex || vpSize.x <= 0.f || vpSize.y <= 0.f)
    {
        return;
    }

    const uint32 w = _rt->Get_Width();
    const uint32 h = _rt->Get_Height();

    if (w != _idRT->Get_Width() || h != _idRT->Get_Height())
    {
        _idRT->Resize(w, h, DXGI_FORMAT_R8G8B8A8_UNORM, Vec4(0.f, 0.f, 0.f, 1.f), true);
    }

    _idRT->Begin();
    for (uint32 i = 0; i < static_cast<uint32>(_scene._objects.size()); ++i)
    {
        auto& obj = _scene._objects[i];
        if (obj->_type != EObjectType::StaticMesh || !obj->_renderHandle)
        {
            continue;
        }
        obj->_renderHandle->RenderID(obj->_worldMatrix, i + 1);
    }

    GAME->Bind_BackBuffer();

    const float u = (mousePos.x - vpPos.x) / vpSize.x;
    const float v = (mousePos.y - vpPos.y) / vpSize.y;
    const uint32 px = min(static_cast<uint32>(u * w), w - 1);
    const uint32 py = min(static_cast<uint32>(v * h), h - 1);

    D3D11_BOX box = { px, py, 0, px + 1, py + 1, 1 };
    _context->CopySubresourceRegion(_idStagingTex.Get(), 0, 0, 0, 0, _idRT->Get_Texture().Get(), 0, &box);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(_context->Map(_idStagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
    {
        const uint8* pPixel = static_cast<const uint8*>(mapped.pData);
        const uint32 id = pPixel[0] | (pPixel[1] << 8) | (pPixel[2] << 16);
        _context->Unmap(_idStagingTex.Get(), 0);

        if (id == 0)
        {
            _scene._selected = nullptr;
        }
        else if (id - 1 < static_cast<uint32>(_scene._objects.size()))
        {
            _scene._selected = _scene._objects[id - 1];
        }
    }
}

void MapEditorApp::SetCursorAt(const ImVec2& mousePos, const ImVec2& vpPos, const ImVec2& vpSize)
{
    if (!_idRT || !_idStagingTex || vpSize.x <= 0.f || vpSize.y <= 0.f)
    {
        return;
    }

    const uint32 w = _rt->Get_Width();
    const uint32 h = _rt->Get_Height();

    if (w != _idRT->Get_Width() || h != _idRT->Get_Height())
    {
        _idRT->Resize(w, h, DXGI_FORMAT_R8G8B8A8_UNORM, Vec4(0.f, 0.f, 0.f, 1.f), true);
    }

    CHECK_FAILED(_idRT->Begin());

    for (uint32 i = 0; i < static_cast<uint32>(_scene._objects.size()); ++i)
    {
        auto& obj = _scene._objects[i];
        if (obj->_type != EObjectType::StaticMesh || !obj->_renderHandle)
        {
            continue;
        }

        obj->_renderHandle->RenderID(obj->_worldMatrix, i + 1);
    }

    GAME->Bind_BackBuffer();

    const float u = (mousePos.x - vpPos.x) / vpSize.x;
    const float v = (mousePos.y - vpPos.y) / vpSize.y;
    const uint32 px = min(static_cast<uint32>(u * w), w - 1);
    const uint32 py = min(static_cast<uint32>(v * h), h - 1);

    D3D11_BOX box = { px, py, 0, px + 1, py + 1, 1 };
    _context->CopySubresourceRegion(_idStagingTex.Get(), 0, 0, 0, 0, _idRT->Get_Texture().Get(), 0, &box);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(_context->Map(_idStagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
    {
        const uint8* p = static_cast<const uint8*>(mapped.pData);
        const uint32 id = p[0] | (p[1] << 8) | (p[2] << 16);
        _context->Unmap(_idStagingTex.Get(), 0);

        const float ndcX = ((mousePos.x - vpPos.x) / vpSize.x) * 2.f - 1.f;
        const float ndcY = -((mousePos.y - vpPos.y) / vpSize.y) * 2.f + 1.f;
        const Matrix& proj = _camera->GetProj();
        const Matrix invView = _camera->GetView().Invert();
        Vec3 rayOrigin(invView._41, invView._42, invView._43);
        Vec3 rayDir = Vec3::TransformNormal(Vec3(ndcX / proj._11, ndcY / proj._22, 1.f), invView);
        rayDir.Normalize();

        if (id > 0 && id - 1 < static_cast<uint32>(_scene._objects.size()))
        {
            auto& obj = _scene._objects[id - 1];

            Vec3 localMin, localMax;
            if (obj->_renderHandle &&
                obj->_renderHandle->GetModel()->Try_Get_LocalBounds(localMin, localMax))
            {
                BoundingBox localBox;
                BoundingBox::CreateFromPoints(localBox, XMLoadFloat3(&localMin), XMLoadFloat3(&localMax));

                BoundingBox worldBox;
                localBox.Transform(worldBox,
                    obj->_renderHandle->GetModel()->Get_PreLocalTransformMatrix() * obj->_worldMatrix);

                float dist = 0.f;
                if (worldBox.Intersects(XMLoadFloat3(&rayOrigin), XMLoadFloat3(&rayDir), dist))
                {
                    _scene._cursorPos = rayOrigin + rayDir * dist;
                }
                else
                {
                    _scene._cursorPos = obj->_position;
                }
            }
            else
            {
                _scene._cursorPos = obj->_position;
            }
        }
        else
        {
            _scene._cursorPos = rayOrigin + rayDir * 10.f;
        }

        _scene._cursorVisible = true;
    }
}

void MapEditorApp::DoSpawnAt(const string& modelPath)
{
    if (!_scene._cursorVisible)
    {
        return;
    }

    wstring wPath = String::ToWString(modelPath);
    _scene.SpawnObject(wPath, _scene._cursorPos);
}

HRESULT MapEditorApp::Ready_PickingRT()
{
    _idRT = RenderTarget::Create(_device, _context, g_iWinSizeX, g_iWinSizeY, DXGI_FORMAT_R8G8B8A8_UNORM, Vec4(0.f, 0.f, 0.f, 1.f), true);
    CHECK_NULL(_idRT, E_FAIL);

    D3D11_TEXTURE2D_DESC stagingDesc{};
    stagingDesc.Width = 1;
    stagingDesc.Height = 1;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    CHECK_FAILED(_device->CreateTexture2D(&stagingDesc, nullptr, _idStagingTex.GetAddressOf()), E_FAIL);

    return S_OK;
}

void MapEditorApp::SyncSceneLights()
{
    GAME->Clear_Lights();

    for (const auto& obj : _scene._objects)
    {
        if (obj->_type != EObjectType::Light)
        {
            continue;
        }

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
    _scene._objects.push_back(obj);
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
    _scene._selected.reset();
    _scene._objects.clear();
    _meshShader.reset();
    _modelManager.reset();

    __super::Free();

    _viewport.reset();
    _rt.reset();
    _idRT.reset();
    _idStagingTex.Reset();
    _camera.reset();
    _manager.reset();
    _imguiManager.reset();
    _context.Reset();
    _device.Reset();

    GameInstance::DestroyInstance();
}
