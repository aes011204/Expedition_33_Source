#include "EditorPickingSystem.h"
#include "RenderTarget.h"
#include "GameInstance.h"
#include "EditorInstance.h"
#include "GameObject.h"

EditorPickingSystem::~EditorPickingSystem()
{

}

HRESULT EditorPickingSystem::Initialize(ComPtr<Device> device, ComPtr<Context> context, uint32 w, uint32 h)
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

void EditorPickingSystem::EnsureSize(uint32 w, uint32 h)
{
    if (w != _idRT->Get_Width() || h != _idRT->Get_Height())
    {
        _idRT->Resize(w, h, DXGI_FORMAT_R8G8B8A8_UNORM, Vec4(0.f, 0.f, 0.f, 1.f), true);
    }
}

uint32 EditorPickingSystem::SampleID(const ImVec2& mousePos, const ImVec2& vpMin, const ImVec2& vpMax)
{
    const uint32 w = _idRT->Get_Width();
    const uint32 h = _idRT->Get_Height();

    const float vpW = vpMax.x - vpMin.x;
    const float vpH = vpMax.y - vpMin.y;

    const uint32 px = min(static_cast<uint32>((mousePos.x - vpMin.x) / vpW * w), w - 1);
    const uint32 py = min(static_cast<uint32>((mousePos.y - vpMin.y) / vpH * h), h - 1);

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

void EditorPickingSystem::DoPickAt(const ImVec2& mousePos, const ImVec2& vpMin, const ImVec2& vpMax)
{
    if (!_idRT || !_idStagingTex)
    {
        return;
    }

    const float vpW = vpMax.x - vpMin.x;
    const float vpH = vpMax.y - vpMin.y;
    if (vpW <= 0.f || vpH <= 0.f)
    {
        return;
    }

    const uint32 rtW = GAME->Get_ViewportWidth();
    const uint32 rtH = GAME->Get_ViewportHeight();
    EnsureSize(rtW, rtH);

    vector<Shared<GameObject>> objects;
    GAME->Visit_CurrentLevelObjects(
        [&](const wstring&, const Shared<GameObject>& obj)
        {
            if (obj)
            {
                objects.push_back(obj);
            }
        });

    CHECK_FAILED(_idRT->Begin(), );

    for (uint32 i = 0; i < static_cast<uint32>(objects.size()); ++i)
    {
        objects[i]->RenderPickingID(i + 1);
    }

    GAME->Bind_BackBuffer();

    uint32 id = SampleID(mousePos, vpMin, vpMax);

    if (id == 0 || id - 1 >= static_cast<uint32>(objects.size()))
    {
        EDITOR->Clear_SelectedObject();
        return;
    }

    EDITOR->Select_Object(objects[id - 1]);
}


Unique<EditorPickingSystem> EditorPickingSystem::Create(ComPtr<Device> device, ComPtr<Context> context, uint32 w, uint32 h)
{
    auto instance = make_unique<EditorPickingSystem>();
    if (FAILED(instance->Initialize(device, context, w, h)))
    {
        return nullptr;
    }

    return instance;
}
