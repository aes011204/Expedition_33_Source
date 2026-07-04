#include "Level_Stage01.h"
#include "GameInstance.h"
#include "MapLoader.h"
#include "ClientInstance.h"
#include "RuntimeRuleStateService.h"
#include "TargetCamera.h"
#include "SoulLikeController.h"
#include "SkyBoxFlow.h"
#include "Trigger_FallTornadoZone.h"
#include "SimonCriticalGlowController.h"
#include "MovementCom.h"
#include "Player_WorldBase.h"

namespace
{
    constexpr wchar_t kEffectName[] = L"Effects/Assets/Map/FX_TornadoPaint_EX.effect.json";

    constexpr wchar_t kParticelEffectName[] = L"Effects/Assets/Map/FX_FallingLight.effect.json";

    constexpr Vec3 kTeleportPos = Vec3(197.57f, -4023.57f, 50.95f);
    constexpr Vec3 kTeleportRot = Vec3(0.f, 27.6f, 0.f);

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
    CHECK_FAILED(Ready_Layer_BackGround(TEXT("Layer_BackGround")), E_FAIL);

    RuntimeRuleStateService::Apply_ToCurrentRuntime(
        RuntimeRuleStateService::Build_SimonStartUnlocks(), false);

    //GAME->Play_BGM(Constants::SoundBGM::SpringBGM, 1.f, true, 0.f);

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
    ParticlDesc.effectName = kParticelEffectName;
    ParticlDesc.worldPosition = Vec3(219.52f, -4023.47f, 52.33f);
    ParticlDesc.scale = Vec3(1.f, 1.f, 1.f);

    CLIENT->Play_Effect(ParticlDesc);

    ParticlDesc.worldPosition = Vec3(200.29f, -4023.47f, 106.4f);
    CLIENT->Play_Effect(ParticlDesc);

    //

    GameObject::GAMEOBJECT_DESC fallTornadoDesc{};
    fallTornadoDesc.position = Vec3(220.62f, -82.56f, -0.98f);
    fallTornadoDesc.scale = Vec3(8.f, 1.f, 8.f);

    Shared<GameObject> fallTornadoZone = nullptr;
    GAME->Add_GameObject(
        ETOI(LevelType::Stage01),
        TEXT("GameObject_Trigger_FallTornadoZone_Stage01"),
        ETOI(LevelType::Stage01),
        TEXT("Layer_ETC"),
        fallTornadoZone,
        &fallTornadoDesc);

    CLIENT->Capture_InitialWorldSnapshot();
    CLIENT->Queue_LevelAnnounce(LevelType::Stage01);

    Apply_Stage01RenderParams();

}

void Level_Stage01::Update(float fTimeDelta)
{
    if (SimonCriticalGlowController* glowController = CLIENT->Get_SimonCriticalGlowController())
    {
        glowController->Update(fTimeDelta);
    }
    // 시몽 앞으로 순간이동
    if (GAME->KeyDown(KEY_TYPE::KEY_9))
    {
        auto active = CLIENT->Get_ActiveHero();
        CHECK_NULL(active);

        active->Get_Transform()->Set_WorldPosition(kTeleportPos);
        active->Get_Transform()->Set_WorldRotationEuler(kTeleportRot);

        if (auto movement = active->Get_Component<MovementCom>())
            movement->Sync_RuntimePositionToTransform();
    }
}

HRESULT Level_Stage01::Render()
{
    Update_WindowTitleFps(_windowTitleFps, L"Level_Stage01");

    return S_OK;
}

HRESULT Level_Stage01::Ready_Lights()
{
    GAME->Reset_DynamicShadowCascadeTemporalUpdateIntervals();
    GAME->Clear_Lights();

    LIGHT_DESC LightDesc{};
    LightDesc.type = LightType::Directional;
    LightDesc.direction = XMFLOAT4(-0.560f, -0.750f, 0.620f, 0.f);
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
    Vec3 startPos = Vec3(78.57f, 2.46f, -1.5f);
    Vec3 startRot = Vec3(0.f, 85.8f, 0.f);

    CHECK_FAILED(CLIENT->Initialize_Hero(ETOI(LevelType::Stage01), strLayerTag, startPos), E_FAIL);

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

HRESULT Level_Stage01::Ready_Layer_BackGround(const wstring& strLayerTag)
{
    SkyBoxFlow::SKYBOXFLOW_DESC skyBoxDesc{};
    skyBoxDesc.modelTag = TEXT("Model_SM_Skybox_Mesh");
    skyBoxDesc.shaderTag = TEXT("Shader_SkyBoxFlow");
    skyBoxDesc.skyTextureTag = TEXT("Texture_SkyBox_Simon");
    skyBoxDesc.flowTextureTag = TEXT("Texture_SkyBox_Flow03");

    skyBoxDesc.tint = Vec4(1.604023f, 0.887117f, 0.631814f, 1.f);
    skyBoxDesc.brightness = 500.f;
    skyBoxDesc.contrast = 1.524392f;
    skyBoxDesc.noiseStrength = 0.25f;
    skyBoxDesc.speed = 0.001f;
    skyBoxDesc.scale = 1.f;

    CHECK_FAILED(
        GAME->Add_GameObject(
            ETOI(LevelType::Stage01),
            TEXT("GameObject_SkyBoxFlow_Stage01"),
            ETOI(LevelType::Stage01),
            strLayerTag,
            &skyBoxDesc),
        E_FAIL);

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

void Level_Stage01::Apply_Stage01RenderParams()
{
    Shared<LightingPipeline> lightingPipeline = GAME->Get_LightingPipeline();
    Shared<PostProcessPipeline> postProcessPipeline = GAME->Get_PostProcessPipeline();
    if (_hasSavedStage01RenderParams)
        return;

    GAME->Get_LightDescs(_savedLightDescs);
    if (lightingPipeline != nullptr)
    {
        _savedSSAOCB = lightingPipeline->Get_Params();
        _savedDeferredLightingCB = lightingPipeline->Get_DeferredLightingParams();
    }
    if (postProcessPipeline != nullptr)
        _savedPostProcessFeatures = postProcessPipeline->Get_PostProcessFeatures();

    _hasSavedStage01RenderParams = true;

    vector<LIGHT_DESC> lightDescs = _savedLightDescs;
    for (LIGHT_DESC& lightDesc : lightDescs)
    {
        if (lightDesc.type != LightType::Directional)
            continue;

        lightDesc.direction = Vec4(-0.560f, -0.750f, 0.620f, 0.f);
        break;
    }
    if (!lightDescs.empty())
        GAME->Set_Lights(lightDescs);

    if (lightingPipeline != nullptr)
    {
        SSAOCB ssao = _savedSSAOCB;
        ssao.SSAOEnabled = 1u;
        ssao.SSAORadius = 0.100f;
        ssao.SSAOBias = 0.0600f;
        ssao.SSAOIntensity = 1.700f;
        ssao.SSAOPower = 1.500f;
        ssao.SSAODepthRange = 1.000f;
        ssao.SSAOSampleCount = 16u;
        lightingPipeline->Set_Params(ssao);

        DeferredLightingCB lighting = _savedDeferredLightingCB;
        lighting.stylizedLightContrastEnabled = 1u;
        lighting.stylizedLightContrastStrength = 0.51f;
        lighting.stylizedLightThreshold = 0.16f;
        lighting.stylizedLightSoftness = 0.500f;
        lighting.stylizedLitBoost = 1.13f;
        lighting.stylizedUnlitAmbientScale = 0.00f;
        lighting.stylizedDeepUnlitThreshold = 0.52f;
        lighting.stylizedDeepUnlitSoftness = 0.500f;
        lighting.stylizedDeepUnlitAmbientScale = 0.29f;
        lightingPipeline->Set_DeferredLightingParams(lighting);
    }

    if (postProcessPipeline != nullptr)
    {
        postProcessPipeline->Set_PostProcessFeatures(
            PostProcessFeature::HDR |
            PostProcessFeature::ToneMapping |
            PostProcessFeature::Exposure |
            PostProcessFeature::Bloom |
            PostProcessFeature::Gamma |
            PostProcessFeature::LightShaft);
    }
}

void Level_Stage01::Restore_Stage01RenderParams()
{
    if (!_hasSavedStage01RenderParams)
        return;

    Shared<LightingPipeline> lightingPipeline = GAME->Get_LightingPipeline();
    if (lightingPipeline != nullptr)
    {
        lightingPipeline->Set_Params(_savedSSAOCB);
        lightingPipeline->Set_DeferredLightingParams(_savedDeferredLightingCB);
    }

    Shared<PostProcessPipeline> postProcessPipeline = GAME->Get_PostProcessPipeline();
    if (postProcessPipeline != nullptr)
        postProcessPipeline->Set_PostProcessFeatures(_savedPostProcessFeatures);

    if (!_savedLightDescs.empty())
        GAME->Set_Lights(_savedLightDescs);

    _savedDeferredLightingCB = DeferredLightingCB{};
    _savedSSAOCB = SSAOCB{};
    _savedPostProcessFeatures = PostProcessFeature::None;
    _savedLightDescs.clear();
    _hasSavedStage01RenderParams = false;
}

void Level_Stage01::Free()
{
    Restore_Stage01RenderParams();

    CLIENT->Clear_GameplayUI();

    __super::Free();
}
