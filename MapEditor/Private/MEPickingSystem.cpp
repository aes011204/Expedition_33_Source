#include "MEPickingSystem.h"
#include "SceneContext.h"
#include "MECamera.h"
#include "RenderTarget.h"
#include "GameInstance.h"
#include "MEStaticMeshActor.h"
#include "MEInstancedMeshActor.h"
#include "Renderer_Define.h"

HRESULT MEPickingSystem::Initialize(ComPtr<Device> device, ComPtr<Context> context, uint32 w, uint32 h)
{
    _device = device;
    _context = context;

    _idRT = RenderTarget::Create(L"RenderTarget", _device, _context, w, h, DXGI_FORMAT_R8G8B8A8_UNORM, Vec4(0.f, 0.f, 0.f, 1.f), true);
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

    CHECK_FAILED(EnsureDepthReadbackSize(w, h), E_FAIL);

    return S_OK;
}

uint32 MEPickingSystem::SampleID(const SceneContext& scene, const ImVec2& mousePos, const ImVec2& vpPos, const ImVec2& vpSize, const RenderTarget& rt)
{
    EnsureSize(rt.Get_Width(), rt.Get_Height());

    CHECK_FAILED(_idRT->Begin(), 0);

    const uint32 staticCount = static_cast<uint32>(scene._staticMeshes.size());
    for (uint32 i = 0; i < staticCount; ++i)
    {
        auto& obj = scene._staticMeshes[i];
        if (!obj->_renderHandle)
        {
            continue;
        }

        obj->_renderHandle->RenderID(obj->_worldMatrix, i + 1);
    }

    const uint32 instancedCount = static_cast<uint32>(scene._instancedMeshes.size());
    for (uint32 i = 0; i < instancedCount; ++i)
    {
        auto& obj = scene._instancedMeshes[i];
        if (!obj->_instancedRenderHandle)
        {
            continue;
        }

        uint32 baseID = 0x800000u | ((i & 0xFFu) << 15);
        obj->_instancedRenderHandle->RenderPickingID(baseID);
    }

    GAME->Bind_BackBuffer();

    const uint32 w = rt.Get_Width();
    const uint32 h = rt.Get_Height();
    const uint32 px = min(static_cast<uint32>((mousePos.x - vpPos.x) / vpSize.x * w), w - 1);
    const uint32 py = min(static_cast<uint32>((mousePos.y - vpPos.y) / vpSize.y * h), h - 1);

    D3D11_BOX box = { px, py, 0, px + 1, py + 1, 1 };
    _context->CopySubresourceRegion(_idStagingTex.Get(), 0, 0, 0, 0, _idRT->Get_Texture().Get(), 0, &box);

    uint32 id = {};
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(_context->Map(_idStagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
    {
        const uint8* p = static_cast<const uint8*>(mapped.pData);
        id = p[0] | (p[1] << 8) | (p[2] << 16);
        _context->Unmap(_idStagingTex.Get(), 0);
    }

    return id;
}


void MEPickingSystem::EnsureSize(uint32 w, uint32 h)
{
    if (w != _idRT->Get_Width() || h != _idRT->Get_Height())
    {
        _idRT->Resize(w, h, DXGI_FORMAT_R8G8B8A8_UNORM, Vec4(0.f, 0.f, 0.f, 1.f), true);
    }
}

HRESULT MEPickingSystem::EnsureDepthReadbackSize(uint32 w, uint32 h)
{
    if (w == 0 || h == 0)
    {
        return E_FAIL;
    }

    if (_depthStagingTex)
    {
        D3D11_TEXTURE2D_DESC existingDesc{};
        _depthStagingTex->GetDesc(&existingDesc);
        if (existingDesc.Width == w && existingDesc.Height == h &&
            existingDesc.Format == DXGI_FORMAT_R32G32B32A32_FLOAT)
        {
            return S_OK;
        }

        _depthStagingTex.Reset();
    }

    D3D11_TEXTURE2D_DESC depthStagingDesc{};
    depthStagingDesc.Width = w;
    depthStagingDesc.Height = h;
    depthStagingDesc.MipLevels = 1;
    depthStagingDesc.ArraySize = 1;
    depthStagingDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    depthStagingDesc.SampleDesc.Count = 1;
    depthStagingDesc.Usage = D3D11_USAGE_STAGING;
    depthStagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    return _device->CreateTexture2D(&depthStagingDesc, nullptr, _depthStagingTex.GetAddressOf());
}

void MEPickingSystem::DoPickAt(SceneContext& scene, const ImVec2& mousePos, const ImVec2& vpPos, const ImVec2& vpSize, const RenderTarget& rt)
{
    if (!_idRT || !_idStagingTex || vpSize.x <= 0.f || vpSize.y <= 0.f)
    {
        return;
    }

    uint32 id = SampleID(scene, mousePos, vpPos, vpSize, rt);
    const uint32 staticCount = static_cast<uint32>(scene._staticMeshes.size());

    if (id == 0)
    {
        scene._selected = nullptr;
    }
    else if (id & 0x800000u)
    {
        uint32 groupIdx = (id >> 15) & 0xFFu;
        uint32 instanceIdx = id & 0x7FFFu;
        if (groupIdx < static_cast<uint32>(scene._instancedMeshes.size()))
        {
            scene._selected = scene._instancedMeshes[groupIdx];
            scene._selected->_selectedInstanceIdx = static_cast<int>(instanceIdx);
        }
        else
        {
            scene._selected = nullptr;
        }
    }
    else if (id <= staticCount)
    {
        scene._selected = scene._staticMeshes[id - 1];
        if (scene._selected)
        {
            scene._selected->_selectedInstanceIdx = -1;
        }
    }
    else
    {
        scene._selected = nullptr;
    }
}


void MEPickingSystem::SetCursorAt(SceneContext& scene, const MECamera& camera, const ImVec2& mousePos, const ImVec2& vpPos, const ImVec2& vpSize, const RenderTarget& rt)
{
    if (!_idRT || !_idStagingTex || vpSize.x <= 0.f || vpSize.y <= 0.f)
    {
        return;
    }

    uint32 id = SampleID(scene, mousePos, vpPos, vpSize, rt);
    const uint32 staticCount = static_cast<uint32>(scene._staticMeshes.size());

    const float ndcX = ((mousePos.x - vpPos.x) / vpSize.x) * 2.f - 1.f;
    const float ndcY = -((mousePos.y - vpPos.y) / vpSize.y) * 2.f + 1.f;
    const Matrix& proj = camera.GetProj();
    const Matrix invView = camera.GetView().Invert();

    Vec3 rayOrigin(invView._41, invView._42, invView._43);
    Vec3 rayDir = Vec3::TransformNormal(Vec3(ndcX / proj._11, ndcY / proj._22, 1.f), invView);
    rayDir.Normalize();

    if (id > 0 && id <= staticCount)
    {
        auto& obj = scene._staticMeshes[id - 1];
        Vec3 localMin, localMax;
        if (obj->_renderHandle && obj->_renderHandle->GetModel()->Try_Get_LocalBounds(localMin, localMax))
        {
            BoundingBox localBox, worldBox;
            BoundingBox::CreateFromPoints(localBox, XMLoadFloat3(&localMin), XMLoadFloat3(&localMax));
            localBox.Transform(worldBox, obj->_renderHandle->GetModel()->Get_PreLocalTransformMatrix() * obj->_worldMatrix);

            float dist = 0.f;
            if (worldBox.Intersects(XMLoadFloat3(&rayOrigin), XMLoadFloat3(&rayDir), dist))
            {
                scene._cursorPos = rayOrigin + rayDir * dist;
            }
            else
            {
                scene._cursorPos = obj->_position;
            }
        }
        else
        {
            scene._cursorPos = obj->_position;
        }
    }
    else
    {
        Vec3 worldPos{};
        if (SampleWorldPos(camera, mousePos, vpPos, vpSize, worldPos))
        {
            scene._cursorPos = worldPos;
        }
        else
        {
            scene._cursorPos = rayOrigin + rayDir * 10.f;
        }
    }

    scene._cursorVisible = true;
}

bool MEPickingSystem::SampleWorldPos(const MECamera& camera, const ImVec2& mousePos, const ImVec2& vpPos, const ImVec2& vpSize, Vec3& outWorldPos)
{
    if (vpSize.x <= 0.f || vpSize.y <= 0.f)
    {
        return false;
    }

    const uint32 rtW = GAME->Get_ViewportWidth();
    const uint32 rtH = GAME->Get_ViewportHeight();
    if (FAILED(EnsureDepthReadbackSize(rtW, rtH)))
    {
        return false;
    }

    if (FAILED(GAME->Copy_RT_Resource(RT::Depth, _depthStagingTex.Get())))
    {
        return false;
    }

    const uint32 px = min(static_cast<uint32>((mousePos.x - vpPos.x) / vpSize.x * rtW), rtW - 1);
    const uint32 py = min(static_cast<uint32>((mousePos.y - vpPos.y) / vpSize.y * rtH), rtH - 1);

    float ndcDepth = {};
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(_context->Map(_depthStagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
    {
        return false;
    }

    const uint8* srcBytes = static_cast<const uint8*>(mapped.pData);
    const float* pixel = reinterpret_cast<const float*>(srcBytes + static_cast<size_t>(mapped.RowPitch) * py) + px * 4;
    ndcDepth = pixel[0];
    _context->Unmap(_depthStagingTex.Get(), 0);

    if (ndcDepth <= 0.f)
    {
        return false;
    }

    const float ndcX = ((mousePos.x - vpPos.x) / vpSize.x) * 2.f - 1.f;
    const float ndcY = -((mousePos.y - vpPos.y) / vpSize.y) * 2.f + 1.f;

    const Matrix viewProjInv = (camera.GetView() * camera.GetProj()).Invert();
    const Vec4 clip = Vec4(ndcX, ndcY, ndcDepth, 1.f);
    const Vec4 world = Vec4::Transform(clip, viewProjInv);

    if (world.w == 0.f)
    {
        return false;
    }

    outWorldPos = Vec3(world.x / world.w, world.y / world.w, world.z / world.w);
    return true;
}


Unique<MEPickingSystem> MEPickingSystem::Create(ComPtr<Device> device, ComPtr<Context> context, uint32 w, uint32 h)
{
    auto instance = make_unique<MEPickingSystem>();
    if (FAILED(instance->Initialize(device, context, w, h)))
    {
        return nullptr;
    }

    return instance;
}
