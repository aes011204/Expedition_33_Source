#include "Level_Stage01.h"
#include "GameInstance.h"
#include "MapLoader.h"

Level_Stage01::Level_Stage01(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Level{ device, context }
{
}

HRESULT Level_Stage01::Initialize()
{
    CHECK_FAILED(Ready_Lights(), E_FAIL);

    GAME->Play_BGM(Constants::SoundBGM::SpringBGM, 1.f, true, 0.f);

    return S_OK;
}

void Level_Stage01::Update(float fTimeDelta)
{
}

HRESULT Level_Stage01::Render()
{
    Update_WindowTitleFps(_windowTitleFps, L"Level_Stage01");

    return S_OK;
}

HRESULT Level_Stage01::Ready_Lights()
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

Shared<Level_Stage01> Level_Stage01::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto pInstance = make_shared<Level_Stage01>(device, context);

    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Created : Level_Stage01");
        return nullptr;
    }
    return pInstance;
}

void Level_Stage01::Free()
{
    __super::Free();
}
