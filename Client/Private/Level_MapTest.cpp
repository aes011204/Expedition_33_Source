#include "Level_MapTest.h"
#include "Camera.h"
#include "TargetCamera.h"
#include "GameObject.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "MapLoader.h"
#include "Spawn_Prefab.h"

Level_MapTest::Level_MapTest(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Level{ device, context }
{
}

HRESULT Level_MapTest::Initialize()
{
    CHECK_FAILED(Ready_Lights(), E_FAIL);
    CHECK_FAILED(Ready_Layer_BackGround(TEXT("Layer_BackGround")), E_FAIL);

    CHECK_FAILED(Ready_Layer_Player(TEXT("Layer_Player")), E_FAIL);
    CHECK_FAILED(Ready_Layer_UI(TEXT("Layer_UI")), E_FAIL);

    // 플레이어가 생성되고 나서 Target 카메라를 준비해야한다. 순서 조심
    CHECK_FAILED(Ready_Layer_Camera(TEXT("Layer_Camera")), E_FAIL);

    CHECK_FAILED(Ready_Layer_Monster(TEXT("Layer_Monster")), E_FAIL);
    CHECK_FAILED(Ready_Layer_Effect(TEXT("Layer_Effect")), E_FAIL);

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

HRESULT Level_MapTest::Ready_Layer_BackGround(const wstring& strLayerTag)
{
    CHECK_FAILED(GAME->Add_GameObject(
        ETOI(LevelType::MapTest), TEXT("GameObject_Sky_MapTest"),
        ETOI(LevelType::MapTest), strLayerTag), E_FAIL);

    return S_OK;
}

HRESULT Level_MapTest::Ready_Layer_UI(const wstring& strLayerTag)
{
    CHECK_FAILED(CLIENT->Initialize_GameplayUI(ETOI(LevelType::MapTest)), E_FAIL);

    return S_OK;
}

HRESULT Level_MapTest::Ready_Layer_Camera(const wstring& strLayerTag)
{
    TargetCamera::TARGETCAMERA_DESC cameraDesc{};
    cameraDesc.speedPerSec = 10.f;
    cameraDesc.degreePerSec = 180.f;
    cameraDesc.eye = XMFLOAT4(0.f, 20.f, -7.f, 1.f);
    cameraDesc.at = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
    cameraDesc.fovy = XMConvertToRadians(60.f);
    cameraDesc.nearPlane = 0.1f;
    cameraDesc.farPlane = 500.f;
    cameraDesc.mouseSensor = 0.06f;
    cameraDesc.followSpeed = 4.f;
    cameraDesc.distance = 1.8f;
    cameraDesc.sideOffset = 0.f;
    cameraDesc.heightOffset = 0.5f;
    cameraDesc.aimOffset = 0.5f;
    cameraDesc.pitchLookScale = 0.5f;
    cameraDesc.pitchMin = -70.f;
    cameraDesc.pitchMax = 80.f;
    cameraDesc.lookHeight = 0.8f;
    cameraDesc.pivotHeight = 0.3f;
    cameraDesc.mouseSensitivity = 0.5f;
    cameraDesc.yawOffset = 0.f;
    cameraDesc.pitchOffset = 0.f;

    Shared<GameObject> cameraObject = nullptr;

    CHECK_FAILED(
        GAME->Add_GameObject(
            ETOI(LevelType::MapTest),
            TEXT("GameObject_TargetCamera_MapTest"),
            ETOI(LevelType::MapTest),
            strLayerTag,
            cameraObject,
            &cameraDesc),
        E_FAIL);

    const Shared<TargetCamera> targetCamera =
        dynamic_pointer_cast<TargetCamera>(cameraObject);

    CHECK_NULL(targetCamera, E_FAIL);

    CLIENT->Set_TargetCamera(targetCamera);

    return S_OK;
}

HRESULT Level_MapTest::Ready_Layer_Player(const wstring& strLayerTag)
{
    Vec3 startPos = Vec3(5.f, 0.5f, -14.24f);

    CHECK_FAILED(CLIENT->Initialize_Hero(ETOI(LevelType::MapTest), TEXT("Layer_Player"), startPos), E_FAIL);

    return S_OK;
}

HRESULT Level_MapTest::Ready_Layer_Monster(const wstring& strLayerTag)
{
    Vec3 startPos = Vec3(0.f, 1.15f, -7.94f);

    const Shared<GameObject> monster = Client::Spawn_Prefab::Spawn(
        Constants::Prefab::MW_Lancelier,
        ETOI(LevelType::MapTest),
        strLayerTag);

    CHECK_NULL(monster, E_FAIL);
    monster->Get_Transform()->Set_Position(startPos);


    //Portier
    const Shared<GameObject> monsterPortier = Client::Spawn_Prefab::Spawn(
        Constants::Prefab::MW_Portier, // 월드 몬스터 prefab 이름은 MW_* 상수로 통일한다.
        ETOI(LevelType::GamePlay),
        strLayerTag);

    CHECK_NULL(monster, E_FAIL);
    monster->Get_Transform()->Set_Position(startPos + Vec3(0.f, 0.f, 10.f));

    //Volester
    const Shared<GameObject> monsterVolester = Client::Spawn_Prefab::Spawn(
        Constants::Prefab::MW_Volester, // 월드 몬스터 prefab 이름은 MW_* 상수로 통일한다.
        ETOI(LevelType::GamePlay),
        strLayerTag);

    CHECK_NULL(monster, E_FAIL);
    monster->Get_Transform()->Set_Position(startPos + Vec3(0.f, 0.f, 20.f));

    //Bishop
    const Shared<GameObject> monsterBishop = Client::Spawn_Prefab::Spawn(
        Constants::Prefab::MW_Bishop, // 월드 몬스터 prefab 이름은 MW_* 상수로 통일한다.
        ETOI(LevelType::GamePlay),
        strLayerTag);

    CHECK_NULL(monster, E_FAIL);
    monster->Get_Transform()->Set_Position(startPos + Vec3(0.f, 0.f, 30.f));
    return S_OK;

    return S_OK;
}

HRESULT Level_MapTest::Ready_Layer_Effect(const wstring& strLayerTag)
{

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
