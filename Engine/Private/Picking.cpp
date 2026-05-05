#include "Picking.h"
#include "GameInstance.h"
#include "Camera.h"

Picking::Picking(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device(device)
    , _context(context)
{
}

HRESULT Picking::Initialize()
{
    const uint32 width = GAME->Get_ViewportWidth();
    const uint32 height = GAME->Get_ViewportHeight();

    if (0 == width || 0 == height)
        return E_FAIL;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;

    CHECK_FAILED(_device->CreateTexture2D(&desc, nullptr, _depthTexture.GetAddressOf()), E_FAIL);

    _depthPixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
    return S_OK;
}

HRESULT Picking::Update()
{
    if (!_depthTexture)
        return E_FAIL;

    D3D11_MAPPED_SUBRESOURCE subResource{};
    CHECK_FAILED(_context->Map(_depthTexture.Get(), 0, D3D11_MAP_READ, 0, &subResource), E_FAIL);

    const uint32 width = GAME->Get_ViewportWidth();
    const uint32 height = GAME->Get_ViewportHeight();

    memcpy(_depthPixels.data(), subResource.pData, sizeof(Vec4) * width * height);

    _context->Unmap(_depthTexture.Get(), 0);

    return S_OK;
}

bool Picking::Pick_WorldPosition(const POINT& pixelPos, Vec3& outWorldPos)
{
    Vec4 depthPixel{};
    if (!Read_DepthPixel(pixelPos, depthPixel))
        return false;

    return Reconstruct_WorldPosition(pixelPos, depthPixel, outWorldPos);
}

bool Picking::Read_DepthPixel(const POINT& mousePos, Vec4& outDepthPixel)
{
    const uint32 width = GAME->Get_ViewportWidth();
    const uint32 height = GAME->Get_ViewportHeight();

    if (mousePos.x < 0 || mousePos.y < 0)
        return false;

    if (mousePos.x >= static_cast<LONG>(width) || mousePos.y >= static_cast<LONG>(height))
        return false;

     const size_t pixelIndex =
        static_cast<size_t>(mousePos.y) * static_cast<size_t>(width) +
        static_cast<size_t>(mousePos.x);

     outDepthPixel = _depthPixels[pixelIndex];

    if (outDepthPixel.w == 0.f)
        return false;

    if (outDepthPixel.x == 0.f && outDepthPixel.y == 0.f)
        return false;

    return true;
}

bool Picking::Reconstruct_WorldPosition(const POINT& mousePos, const Vec4& depthPixel, Vec3& outWorldPos) const
{
    Shared<Camera> camera = GAME->Get_ActiveCamera();
    CHECK_NULL(camera, false);

    const float farPlane = camera->Get_Far();
    if (farPlane <= 0.f)
        return false;

    const Matrix* viewInverse = GAME->Get_Transform_Inverse(D3DTS::View);
    const Matrix* projInverse = GAME->Get_Transform_Inverse(D3DTS::Proj);

    if (!viewInverse || !projInverse)
        return false;

    const uint32 width = GAME->Get_ViewportWidth();
    const uint32 height = GAME->Get_ViewportHeight();

    const float texcoordX = (static_cast<float>(mousePos.x) + 0.5f) / width;
    const float texcoordY = (static_cast<float>(mousePos.y) + 0.5f) / height;

    // 깊이 복원 far가 500이면, depthPixel은 0.2. viewZ = 100
    const float viewZ = depthPixel.y * farPlane;

    // NDC 좌표로 변환. 0 ~ 1 -> -1 ~ 1
    Vec4 worldPos{};
    worldPos.x = texcoordX * 2.f - 1.f;
    worldPos.y = texcoordY * -2.f + 1.f;
    worldPos.z = depthPixel.x;
    worldPos.w = 1.f;

    // 이 픽셀이 NDC 좌표상 어디에 있는지
    worldPos *= viewZ;
    worldPos = Vec4::Transform(worldPos, *projInverse);
    worldPos = Vec4::Transform(worldPos, *viewInverse);

    outWorldPos = Vec3(worldPos.x, worldPos.y, worldPos.z);

    return true;
}

Unique<Picking> Picking::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_unique<Picking>(device, context);
    instance->Initialize();

    return instance;
}

void Picking::Free()
{
    _depthPixels.clear();
    _depthTexture.Reset();

    __super::Free();
}
