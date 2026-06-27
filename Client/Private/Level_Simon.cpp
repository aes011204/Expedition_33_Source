#include "Level_Simon.h"
#include "GameInstance.h"
#include "MapLoader.h"

Level_Simon::Level_Simon(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Level{ device, context }
{
}

HRESULT Level_Simon::Initialize()
{
    CHECK_FAILED(Ready_Lights(), E_FAIL);

    GAME->Play_BGM(Constants::SoundBGM::SpringBGM, 1.f, true, 0.f);

    return S_OK;
}

void Level_Simon::Update(float fTimeDelta)
{
}

HRESULT Level_Simon::Render()
{
    Update_WindowTitleFps(_windowTitleFps, L"Level_Stage01");

    return S_OK;
}

HRESULT Level_Simon::Ready_Lights()
{
    GAME->Clear_Lights();

    LIGHT_DESC LightDesc{};
    LightDesc.type = LightType::Directional;
    LightDesc.direction = XMFLOAT4(1.f, -1.f, 1.f, 0.f);
    LightDesc.diffuse = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
    LightDesc.ambient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.f);
    LightDesc.specular = XMFLOAT4(1.f, 1.f, 1.f, 1.f);

    if (FAILED(GAME->Add_Light(LightDesc)))
    {
        return E_FAIL;
    }

    ShadowLightDesc ShadowDesc{};
    ShadowDesc.enabled = false;
    if (FAILED(GAME->Update_ShadowLight(ShadowDesc)))
        return E_FAIL;

    return S_OK;
}

Shared<Level_Simon> Level_Simon::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto pInstance = make_shared<Level_Simon>(device, context);

    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Created : Level_Stage01");
        return nullptr;
    }
    return pInstance;
}

void Level_Simon::Free()
{
    __super::Free();
}
