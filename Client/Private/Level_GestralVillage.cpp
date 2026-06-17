#include "Level_GestralVillage.h"
#include "GameInstance.h"

Level_GestralVillage::Level_GestralVillage(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Level{ device, context }
{
}

HRESULT Level_GestralVillage::Initialize()
{
    CHECK_FAILED(Ready_Lights(), E_FAIL);

    return S_OK;
}

void Level_GestralVillage::Update(float fTimeDelta)
{

}

HRESULT Level_GestralVillage::Render()
{
    Update_WindowTitleFps(_windowTitleFps, L"Level_GestralVillage");

    return S_OK;
}

HRESULT Level_GestralVillage::Ready_Lights()
{
    LIGHT_DESC LightDesc{};
    LightDesc.type = LightType::Directional;
    LightDesc.direction = Vec4(1.f, -1.f, 1.f, 0.f);
    LightDesc.diffuse = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
    LightDesc.ambient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.f);
    LightDesc.specular = XMFLOAT4(1.f, 1.f, 1.f, 1.f);

    if (FAILED(GAME->Add_Light(LightDesc)))
    {
        return E_FAIL;
    }

    return S_OK;
}

Shared<Level_GestralVillage> Level_GestralVillage::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto pInstance = make_shared<Level_GestralVillage>(device, context);

    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Created : Level_GestralVillage");
        return nullptr;
    }
    return pInstance;
}

void Level_GestralVillage::Free()
{
    __super::Free();
}
