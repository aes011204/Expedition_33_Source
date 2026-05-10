#include "MEPickingSystem.h"
#include "SceneContext.h"
#include "MECamera.h"
#include "RenderTarget.h"
#include "GameInstance.h"
#include "MEStaticMeshActor.h"

HRESULT MEPickingSystem::Initialize(ComPtr<Device> device, ComPtr<Context> context, uint32 w, uint32 h)
{
    _device = device;
    _context = context;

    _idRT = RenderTarget::Create(_device, _context, w, h, DXGI_FORMAT_R8G8B8A8_UNORM, Vec4(0.f, 0.f, 0.f, 1.f), true);
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

uint32 MEPickingSystem::SampleID(const vector<Shared<EditorObject>>& objects, const ImVec2& mousePos, const ImVec2& vpPos, const ImVec2& vpSize, const RenderTarget& rt)
{
    EnsureSize(rt.Get_Width(), rt.Get_Height());

    CHECK_FAILED(_idRT->Begin(), 0);

    for (uint32 i = 0; i < static_cast<uint32>(objects.size()); ++i)
    {
        auto& obj = objects[i];
        if (obj->_type != EObjectType::StaticMesh || !obj->_renderHandle)
        {
            continue;
        }
        obj->_renderHandle->RenderID(obj->_worldMatrix, i + 1);
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
        id = p[0] | (p[1] << 8 | p[2] << 16);
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

void MEPickingSystem::DoPickAt(SceneContext& scene, const ImVec2& mousePos, const ImVec2& vpPos, const ImVec2& vpSize, const RenderTarget& rt)
{
    if (!_idRT || !_idStagingTex || vpSize.x <= 0.f || vpSize.y <= 0.f)
    {
        return;
    }

    uint32 id = SampleID(scene._objects, mousePos, vpPos, vpSize, rt);

    if (id == 0)
    {
        scene._selected = nullptr;
    }
    else if (id - 1 < static_cast<uint32>(scene._objects.size()))
    {
        scene._selected = scene._objects[id - 1];
    }
}

void MEPickingSystem::SetCursorAt(SceneContext& scene, const MECamera& camera, const ImVec2& mousePos, const ImVec2& vpPos, const ImVec2& vpSize, const RenderTarget& rt)
{
    if (!_idRT || !_idStagingTex || vpSize.x <= 0.f || vpSize.y <= 0.f)
    {
        return;
    }

    uint32 id = SampleID(scene._objects, mousePos, vpPos, vpSize, rt);

    const float ndcX = ((mousePos.x - vpPos.x) / vpSize.x) * 2.f - 1.f;
    const float ndcY = -((mousePos.y - vpPos.y) / vpSize.y) * 2.f + 1.f;
    const Matrix& proj = camera.GetProj();
    const Matrix invView = camera.GetView().Invert();
    Vec3 rayOrigin(invView._41, invView._42, invView._43);
    Vec3 rayDir = Vec3::TransformNormal(Vec3(ndcX / proj._11, ndcY / proj._22, 1.f), invView);
    rayDir.Normalize();

    if (id > 0 && id - 1 < static_cast<uint32>(scene._objects.size()))
    {
        auto& obj = scene._objects[static_cast<vector<Shared<EditorObject>, allocator<Shared<EditorObject>>>::size_type>(id) - 1];
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
        scene._cursorPos = rayOrigin + rayDir * 10.f;
    }

    scene._cursorVisible = true;
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
