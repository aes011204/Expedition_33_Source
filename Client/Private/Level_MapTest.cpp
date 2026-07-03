#include "Level_MapTest.h"
#include "Camera.h"
#include "TargetCamera.h"
#include "GameObject.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "Client_Constants.h"
#include "MapLoader.h"
#include "SkyBoxFlow.h"
#include "SkyCloudCard.h"
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

    GAME->Set_RenderEnvironment(true);

    return S_OK;
}

void Level_MapTest::BeginPlay()
{
    Level::BeginPlay();

    Play_MapBGM();

    _editorPlayRuntimeHandle =
        GAME->Get_DelegateHub().OnEditorPlayRuntime.Add(this, &Level_MapTest::Play_MapBGM);
}

void Level_MapTest::Update(float fTimeDelta)
{
}

HRESULT Level_MapTest::Render()
{
    Update_WindowTitleFps(_windowTitleFps, L"Level_MapTest");

    return S_OK;
}

void Level_MapTest::Play_MapBGM()
{
    GAME->Play_BGM(Constants::SoundBGM::SpringBGM, 0.7f, true, 0.f);
}

HRESULT Level_MapTest::Ready_Lights()
{
    GAME->Clear_Lights();

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

    ShadowLightDesc ShadowDesc{};
    ShadowDesc.enabled = false;
    if (FAILED(GAME->Update_ShadowLight(ShadowDesc)))
        return E_FAIL;

    return S_OK;
}

HRESULT Level_MapTest::Ready_Layer_BackGround(const wstring& strLayerTag)
{
    // 스카이박스
    SkyBoxFlow::SKYBOXFLOW_DESC skyBoxDesc{};
    skyBoxDesc.modelTag = TEXT("Model_SM_Skybox_Mesh");
    skyBoxDesc.shaderTag = TEXT("Shader_SkyBoxFlow");
    skyBoxDesc.skyTextureTag = TEXT("Texture_SkyBox_CleasTower");
    skyBoxDesc.flowTextureTag = TEXT("Texture_SkyBox_Flow03");

    skyBoxDesc.tint = Vec4(1.604023f, 0.887117f, 0.631814f, 1.f);
    skyBoxDesc.brightness = 500.f;
    skyBoxDesc.contrast = 1.524392f;
    skyBoxDesc.noiseStrength = 0.25f;
    skyBoxDesc.speed = 0.001f;
    skyBoxDesc.scale = 1.f;

    CHECK_FAILED(
        GAME->Add_GameObject(
            ETOI(LevelType::MapTest),
            TEXT("GameObject_SkyBoxFlow_MapTest"),
            ETOI(LevelType::MapTest),
            strLayerTag,
            &skyBoxDesc),
        E_FAIL);

    // 구름
    auto addSkyCloudCard =
        [&](const wchar_t* textureTag, const Vec3& position, const Vec3& rotation, const Vec3& scale)
        {
            SkyCloudCard::SKYCLOUDCARD_DESC desc{};
            desc.modelTag = TEXT("Model_Cloud_Mesh");
            desc.shaderTag = TEXT("Shader_SkyCloudCard");
            desc.cloudTextureTag = textureTag;
            desc.position = position;
            desc.rotation = rotation;
            desc.scale = scale;

            return GAME->Add_GameObject(
                ETOI(LevelType::MapTest),
                TEXT("GameObject_SkyCloudCard_MapTest"),
                ETOI(LevelType::MapTest),
                strLayerTag,
                &desc);
        };

    CHECK_FAILED(addSkyCloudCard(TEXT("Texture_T_Cloud_13_H"),
        Vec3(229.f, 274.341f, 106.534f),
        Vec3(-4.763f, -119.267f, -349.155f),
        Vec3(1.5f, 0.9f, 0.9f)), E_FAIL);

    //CHECK_FAILED(addSkyCloudCard(TEXT("Texture_T_Cloud_14_H"),
    //    Vec3(2288.613f, -2467.369f, 1935.340f),
    //    Vec3(50.303f, 136.239f, -260.292f),
    //    Vec3(90.293f, 90.293f, 90.106f)), E_FAIL);
    //
    //CHECK_FAILED(addSkyCloudCard(TEXT("Texture_T_Cloud_15_H"),
    //    Vec3(20396.386f, 6946.951f, 1026.531f),
    //    Vec3(3.002f, 469.630f, -358.078f),
    //    Vec3(622.793f, 396.934f, 402.847f)), E_FAIL);

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
    cameraDesc.targetFollowSpeed = 4.f;
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
    UNREFERENCED_PARAMETER(strLayerTag);

    // 전투 시작 위치
    const Shared<GameObject> spawner1 = Spawn_Prefab::Prefab("BSS_GamePlay_Boss")
        .AtLevel(ETOI(LevelType::MapTest))
        .InLayer(TEXT("Layer_BattleStartSpawner"))
        .Position(Vec3(17.f, 0.46f, -3.2f))
        .Spawn();
    CHECK_NULL(spawner1, E_FAIL);

    // 전투 시작 위치
    const Shared<GameObject> spawner2 = Spawn_Prefab::Prefab("BSS_MapTest")
        .AtLevel(ETOI(LevelType::MapTest))
        .InLayer(TEXT("Layer_BattleStartSpawner"))
        .Position(Vec3(17.83f, 0.46f, -20.27f))
        .Spawn();
    CHECK_NULL(spawner2, E_FAIL);

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
    if (_editorPlayRuntimeHandle.IsValid())
    {
        GAME->Get_DelegateHub().OnEditorPlayRuntime.Remove(_editorPlayRuntimeHandle);
        _editorPlayRuntimeHandle.Reset();
    }

    CLIENT->Clear_GameplayUI();

    __super::Free();
}
