#include "Level_MapTest.h"

#include <Camera.h>

#include "GameObject.h"

#include "GameInstance.h"
#include "ClientInstance.h"

Level_MapTest::Level_MapTest(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Level{ device, context }
{
}

HRESULT Level_MapTest::Initialize()
{
    CHECK_FAILED(Ready_Lights(), E_FAIL);

    auto player = CLIENT->Find_Player(ETOI(LevelType::Static));
    if (player)
    {
        Vec3 pos = Vec3(205.f, 4.5f, -1.24f);

        player->Get_Transform()->Set_WorldPosition(pos);
        GAME->Get_ActiveCamera()->Get_Transform()->Set_WorldPosition(Vec3(pos.x, pos.y + 1, pos.z - 2.f));
    }

    return S_OK;
}

void Level_MapTest::BeginPlay()
{
    Level::BeginPlay();


}

void Level_MapTest::Update(float fTimeDelta)
{
}

HRESULT Level_MapTest::Render()
{
#ifdef _DEBUG
    SetWindowText(g_hWnd, L"Level_MapTest");
#endif

    return S_OK;
}

HRESULT Level_MapTest::Ready_Lights()
{
    LIGHT_DESC  LightDesc{};

    LightDesc.type = LightType::Directional;
    LightDesc.direction = XMFLOAT4(1.f, -1.f, 1.f, 0.f);
    LightDesc.diffuse = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
    LightDesc.ambient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.f);
    LightDesc.specular = XMFLOAT4(1.f, 1.f, 1.f, 1.f);

    if (FAILED(GAME->Add_Light(LightDesc)))
    {
        return E_FAIL;
    }
    return S_OK;
}

Shared<Level_MapTest> Level_MapTest::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto pInstance = make_shared<Level_MapTest>(device, context);

    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Created : Level_MapTest");
        return nullptr;
    }
    return pInstance;
}

void Level_MapTest::Free()
{
    __super::Free();
}
