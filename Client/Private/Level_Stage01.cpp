#include "Level_Stage01.h"
#include "GameInstance.h"
#include "MapLoader.h"
#include "ClientInstance.h"
#include "RuntimeRuleStateService.h"
#include "TargetCamera.h"
#include "SoulLikeController.h"

namespace
{
    constexpr wchar_t kEffectName[] = L"Effects/Assets/Map/FX_TornadoPaint_V2.effect.json";

    constexpr wchar_t kParticelEffectName[] = L"Effects/Assets/Map/FX_FallingLight.effect.json";
}

Level_Stage01::Level_Stage01(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Level{ device, context }
{
}

HRESULT Level_Stage01::Initialize()
{
    CHECK_FAILED(Ready_Lights(), E_FAIL);
    CHECK_FAILED(Ready_Layer_Player(TEXT("Layer_Player")), E_FAIL);
    CHECK_FAILED(Ready_Layer_Camera(TEXT("Layer_Camera")), E_FAIL);
    CHECK_FAILED(Ready_Layer_UI(TEXT("Layer_UI")), E_FAIL);

    RuntimeRuleStateService::Apply_ToCurrentRuntime(
        RuntimeRuleStateService::Build_SimonStartUnlocks(), false);

    GAME->Play_BGM(Constants::SoundBGM::SpringBGM, 1.f, true, 0.f);

    return S_OK;
}

void Level_Stage01::BeginPlay()
{
    PlayEffectDesc desc{};
    desc.effectName = kEffectName;
    desc.worldPosition = Vec3(3.8f, 14.69f, -5.41f);
    desc.scale = Vec3(30.f, 30.f, 30.f);

    CLIENT->Play_Effect(desc);

    // Particle
    PlayEffectDesc ParticlDesc{};
    desc.effectName = kParticelEffectName;
    desc.worldPosition = Vec3(219.52f, -4023.47f, 52.33f);
    desc.scale = Vec3(1.f, 1.f, 1.f);

    CLIENT->Play_Effect(ParticlDesc);

    CLIENT->Capture_InitialWorldSnapshot();
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
    LightDesc.diffuse = XMFLOAT4(1.f, 1.f, 1.0f, 1.f);
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

HRESULT Level_Stage01::Ready_Layer_Player(const wstring& strLayerTag)
{
    Vec3 startPos = Vec3(60.93f, 3.63f, -0.93f);
    Vec3 startRot = Vec3(0.f, -90.f, 0.f);

    CHECK_FAILED(CLIENT->Initialize_Hero(ETOI(LevelType::Stage01), strLayerTag, startPos, startRot), E_FAIL);

    return S_OK;
}

HRESULT Level_Stage01::Ready_Layer_Camera(const wstring& strLayerTag)
{
    TargetCamera::TARGETCAMERA_DESC TargetCameraDesc{};
    TargetCameraDesc.speedPerSec = 10.f;
    TargetCameraDesc.degreePerSec = 180.f;
    TargetCameraDesc.eye = XMFLOAT4(0.f, 20.f, -7.f, 1.f);
    TargetCameraDesc.at = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
    TargetCameraDesc.fovy = XMConvertToRadians(60.f);
    TargetCameraDesc.nearPlane = 0.1f;
    TargetCameraDesc.farPlane = 500.f;
    TargetCameraDesc.mouseSensor = 0.06f;
    TargetCameraDesc.followSpeed = 4.f;
    TargetCameraDesc.targetFollowSpeed = 4.f;
    TargetCameraDesc.distance = 1.8f;
    TargetCameraDesc.sideOffset = 0.f;
    TargetCameraDesc.heightOffset = 0.5f;
    TargetCameraDesc.aimOffset = 0.5f;
    TargetCameraDesc.pitchLookScale = 0.5f;
    TargetCameraDesc.pitchMin = -70.f;
    TargetCameraDesc.pitchMax = 80.f;
    TargetCameraDesc.lookHeight = 0.8f;
    TargetCameraDesc.pivotHeight = 0.3f;
    TargetCameraDesc.mouseSensitivity = 0.5f;
    TargetCameraDesc.yawOffset = 0.f;
    TargetCameraDesc.pitchOffset = 0.f;

    Shared<GameObject> cameraObject = nullptr;

    CHECK_FAILED(
        GAME->Add_GameObject(
            ETOI(LevelType::Stage01),
            TEXT("GameObject_TargetCamera_Stage01"),
            ETOI(LevelType::Stage01),
            strLayerTag,
            cameraObject,
            &TargetCameraDesc),
        E_FAIL
    );

    const auto targetCamera = dynamic_pointer_cast<TargetCamera>(cameraObject);
    if (targetCamera != nullptr)
        targetCamera->Set_OrbitAngles(-90.f, 1.1f);

    CLIENT->Set_TargetCamera(targetCamera);

    return S_OK;
}

HRESULT Level_Stage01::Ready_Layer_UI(const wstring& strLayerTag)
{
    CHECK_FAILED(CLIENT->Initialize_GameplayUI(ETOI(LevelType::Stage01)), E_FAIL);

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
    CLIENT->Clear_GameplayUI();

    __super::Free();
}
