#include "Level_Stage01.h"
#include "GameInstance.h"
#include "Client_Constants.h"
#include "GameObject.h"
#include "MapLoader.h"
#include "ClientInstance.h"
#include "RuntimeRuleStateService.h"
#include "Spawn_Prefab.h"
#include "TargetCamera.h"
#include "SoulLikeController.h"
#include "SkyBoxFlow.h"
#include "Trigger_TeleportZone.h"
#include "SimonCriticalGlowController.h"
#include "MapAmbientVisibility.h"
#include "MovementCom.h"
#include "Player_WorldBase.h"
#include "Trigger_WorldDialogue.h"
#include "Trigger_Cinematic.h"

namespace
{
    constexpr wchar_t kEffectName[] = L"Effects/Assets/Map/FX_TornadoPaint_V2.effect.json";

    constexpr wchar_t kParticelEffectName[] = L"Effects/Assets/Map/FX_FallingLight.effect.json";

    constexpr Vec3 kTeleportPos = Vec3(197.57f, -4023.57f, 50.95f);
    constexpr Vec3 kTeleportRot = Vec3(0.f, 27.6f, 0.f);

    constexpr Vec3 kSimonWorldSpawnPos = Vec3(209.f, -4023.35f, 75.66f);
    constexpr Vec3 kSimonWorldSpawnRot = Vec3(0.f, 40.3f, 0.f);

    constexpr Vec3 kSimonCinematicTriggerPos = Vec3(205.f, -4023.f, 68.44f);

    bool Has_WorldDialogueTrigger(uint32 levelIndex)
    {
        bool found = false;

        GAME->Visit_LevelObjects(
            levelIndex,
            [&found](const wstring& layerTag, const Shared<GameObject>& gameObject)
            {
                UNREFERENCED_PARAMETER(layerTag);

                if (found || gameObject == nullptr)
                    return;

                found = gameObject->Get_SourcePrefabName() == Constants::Prefab::Trigger_WorldDialogue;
            });

        return found;
    }

}

Level_Stage01::Level_Stage01(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Level{ device, context }
{
}

HRESULT Level_Stage01::Initialize()
{
    CHECK_FAILED(Ready_Lights(), E_FAIL);
    CHECK_FAILED(Ready_Layer_Player(TEXT("Layer_Player")), E_FAIL);
    CHECK_FAILED(Ready_Layer_Monster(TEXT("Layer_Monster")), E_FAIL);
    CHECK_FAILED(Ready_Layer_Camera(TEXT("Layer_Camera")), E_FAIL);
    CHECK_FAILED(Ready_Layer_UI(TEXT("Layer_UI")), E_FAIL);
    CHECK_FAILED(Ready_Layer_AmbientVisibility(TEXT("Layer_AmbientVisibility")), E_FAIL);
    CHECK_FAILED(Ready_Layer_ETC(TEXT("Layer_ETC")), E_FAIL);
    CHECK_FAILED(Ready_Layer_BackGround(TEXT("Layer_BackGround")), E_FAIL);
    CHECK_FAILED(Ready_Layer_CinematicTrigger(TEXT("Layer_Cinematic")), E_FAIL);

    RuntimeRuleStateService::Apply_ToCurrentRuntime(
        RuntimeRuleStateService::Build_SimonStartUnlocks(), false);
    CLIENT->Restore_CurrentHeroesHpToMax();
    Prewarm_SimonBattleSetup();

    // 안개, 빛 설정
    GAME->Set_RenderEnvironment(true);
    if (FAILED(GAME->Change_IBL(
        L"../../../Client/Bin/Resources/Textures/Shader/IBL/irradiance_eye.dds",
        L"../../../Client/Bin/Resources/Textures/Shader/IBL/perfilter_eye.dds")))
        return E_FAIL;

    // 바람 소리?
    CLIENT->Request_BGMAfterLevelFade(Constants::SoundBGM::SimonStartWindBGM, 0.7f, true, 0.f);

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

    GameObject::GAMEOBJECT_DESC teleportZoneDesc{};
    teleportZoneDesc.position = Vec3(216.88f, -12.38f, -2.66f);
    teleportZoneDesc.scale = Vec3(8.f, 2.f, 8.f);

    Shared<GameObject> fallTornadoZone = nullptr;
    GAME->Add_GameObject(
        ETOI(LevelType::Stage01),
        TEXT("GameObject_Trigger_TeleportZone_Stage01"),
        ETOI(LevelType::Stage01),
        TEXT("Layer_ETC"),
        fallTornadoZone,
        &teleportZoneDesc);

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

    if (GAME->KeyPress(KEY_TYPE::F7))
        Respawn_CinematicTrigger();

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

    CHECK_FAILED(CLIENT->Initialize_Hero(ETOI(LevelType::Stage01), strLayerTag, startPos, startRot), E_FAIL);

    return S_OK;
}

HRESULT Level_Stage01::Ready_Layer_Monster(const wstring& strLayerTag)
{
    Shared<GameObject> simonObject = Spawn_Prefab::Prefab(Constants::Prefab::MW_Simon)
        .AtLevel(ETOI(LevelType::Stage01))
        .InLayer(L"Layer_GameObject")
        .Position(kSimonWorldSpawnPos)
        .Rotation(kSimonWorldSpawnRot)
        .Spawn();

    CHECK_NULL(simonObject, E_FAIL);
    simonObject->Set_RuntimeAuthoringLoaded(true);

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
        targetCamera->Set_OrbitAngles(90.f, 1.1f);

    CLIENT->Set_TargetCamera(targetCamera);

    return S_OK;
}

HRESULT Level_Stage01::Ready_Layer_UI(const wstring& strLayerTag)
{
    CHECK_FAILED(CLIENT->Initialize_GameplayUI(ETOI(LevelType::Stage01)), E_FAIL);

    return S_OK;
}

HRESULT Level_Stage01::Ready_Layer_AmbientVisibility(const wstring& strLayerTag)
{
    MapAmbientVisibility::MAP_AMBIENT_VISIBILITY_DESC desc{};
    desc.name = L"AV_Cube_Stage01_Test";
    desc.modelPath = L"../../../Client/Bin/Resources/Map/SmallLevel_SimonArea/Cube.model";
    desc.position = Vec3(33.38f, 4.73f, 0.20f);
    desc.rotation = Quat::Identity;
    desc.scale = Vec3(461.883f, 200.f, 200.f);
    desc.ambientVisibilityAmount = 0.45f;
    desc.visible = true;
    desc.cullEnabled = true;

    CHECK_FAILED(GAME->Add_GameObject(
        ETOI(LevelType::Static),
        TEXT("GameObject_MapAmbientVisibility"),
        ETOI(LevelType::Stage01),
        strLayerTag,
        &desc),
        E_FAIL);

    return S_OK;
}

HRESULT Level_Stage01::Ready_Layer_ETC(const wstring& strLayerTag)
{
    const uint32 levelIndex = ETOI(LevelType::Stage01);
    if (Has_WorldDialogueTrigger(levelIndex))
        return S_OK;

    const Shared<GameObject> dialogueObject = Spawn_Prefab::Prefab(Constants::Prefab::Trigger_WorldDialogue)
        .AtLevel(levelIndex)
        .InLayer(strLayerTag)
        .Position(Vec3(116.2f, 2.8f, -2.7f))
        .Rotation(Vec3(0.f, 0.f, 0.f))
        .Scale(Vec3(1.f, 1.f, 3.f))
        .Spawn();

    CHECK_NULL(dialogueObject, E_FAIL);
    if (const Shared<Trigger_WorldDialogue> dialogueTrigger =
        dynamic_pointer_cast<Trigger_WorldDialogue>(dialogueObject))
    {
        dialogueTrigger->Set_UIRequest(
            Constants::UI::UI_WorldDialogue_Simon,
            L"Panel_WorldDialogue_Simon",
            Constants::UI_Anim::UI_WorldDialogue_SimonAnim_Path,
            10.f);
    }

    dialogueObject->Set_RuntimeAuthoringLoaded(true);

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

HRESULT Level_Stage01::Ready_Layer_CinematicTrigger(const wstring& strLayerTag)
{
    const Shared<GameObject> triggerObject = Spawn_Prefab::Prefab(Constants::Prefab::Trigger_Cinematic)
        .AtLevel(ETOI(LevelType::Stage01))
        .InLayer(strLayerTag)
        .Position(kSimonCinematicTriggerPos)
        .Spawn();

    CHECK_NULL(triggerObject, E_FAIL);

    const Shared<Trigger_Cinematic> simonTrigger =
        dynamic_pointer_cast<Trigger_Cinematic>(triggerObject);
    CHECK_NULL(simonTrigger, E_FAIL);

    simonTrigger->Set_SequenceFileId(Constants::Cinematic::SimonIntroFileId);
    simonTrigger->Set_StartBattleOnComplete(true, "Simon");
    triggerObject->Set_RuntimeAuthoringLoaded(true);

    return S_OK;
}

void Level_Stage01::Prewarm_SimonBattleSetup()
{
    BattleMonsterSpawnDesc simonSpawnDesc{};
    simonSpawnDesc.prefabName = Constants::Prefab::MB_Simon;

    CLIENT->Queue_BattleEffectPrewarmForBattleSetup(
        vector<HeroId>{ HeroId::Gustave, HeroId::Lune, HeroId::Maelle },
        vector<BattleMonsterSpawnDesc>{ simonSpawnDesc },
        ETOI(LevelType::Stage01));
    CLIENT->Flush_EffectPrewarmQueue();
}

void Level_Stage01::Respawn_CinematicTrigger()
{
    vector<Shared<GameObject>> triggersToRemove{};

    GAME->Visit_LevelObjects(
        ETOI(LevelType::Stage01),
        [&](const wstring&, const Shared<GameObject>& gameObject)
        {
            if (dynamic_pointer_cast<Trigger_Cinematic>(gameObject) == nullptr)
                return;

            triggersToRemove.push_back(gameObject);
        });

    for (const Shared<GameObject>& triggerObject : triggersToRemove)
        GAME->Remove_GameObject(ETOI(LevelType::Stage01), triggerObject);

    GAME->Flush_RemovedGameObjects();

    CHECK_FAILED(Ready_Layer_CinematicTrigger(TEXT("Layer_Cinematic")));
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
        postProcessPipeline->Set_PostProcessFeature(PostProcessFeature::DOF, false);
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
    {
        postProcessPipeline->Set_PostProcessFeature(PostProcessFeature::DOF, true);
        postProcessPipeline->Set_PostProcessFeatures(_savedPostProcessFeatures);
    }

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
