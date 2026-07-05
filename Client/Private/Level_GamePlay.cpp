#include "Level_GamePlay.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "Client_Constants.h"
#include "MapAmbientVisibility.h"
#include "MovementCom.h"
#include "PlayableParts.h"
#include "Player_WorldBase.h"
#include "PuzzleController.h"
#include "SkyBoxFlow.h"
#include "Spawn_Prefab.h"
#include "TargetCamera.h"
#include "Trigger_Cinematic.h"
#include "RuntimeRuleStateService.h"
#include "MapLandscape.h"

namespace
{
    // 보스 시네마틱 연출 트리거 위치
    constexpr Vec3 kCinematicTriggerSpawnPosition = Vec3(78.61f, 22.06f, -14.58f);

    // 튜토리얼 창병 시네마틱 연출 트리거 위치
    constexpr Vec3 kTutoalCinematicTriggerSpawnPosition = Vec3(-64.56f, 19.4f, -109.29f);

    // 창병 전투 시작 위치
    constexpr Vec3 kLancelierBattlePosition = Vec3(-39.12f, 19.1f, -93.1f);
    constexpr Vec3 kLancelierBattleRotation = Vec3(0.f, 33.6f, 0.f);

    // 방패병 전투 시작 위치
    constexpr Vec3 kPortierBattlePosition = Vec3(-35.03f, 19.42f, -33.65f);
    constexpr Vec3 kPortierBattleRotation = Vec3(0.f, 0.f, 0.f);

    // 보스 전투 시작 위치
    constexpr Vec3 kBossBattlePosition = Vec3(138.35f, 21.8f, -9.67f);
    constexpr Vec3 kBossBattleRotation = Vec3(0.f, 0.f, 0.f);

    VolumetricLightShaftZoneCB Build_GamePlayLightShaftZones()
    {
        VolumetricLightShaftZoneCB lightShaftZoneCB{};
        lightShaftZoneCB.zoneCount = 1;

        Matrix lightShaftWorld =
            Matrix::CreateFromYawPitchRoll(
                DirectX::XMConvertToRadians(1.661f),
                DirectX::XMConvertToRadians(-5.5f),
                DirectX::XMConvertToRadians(0.0f)) *
            Matrix::CreateTranslation(Vec3(-0.956f, 22.472f, -21.720f));

        VolumetricLightShaftZoneEntry& zone = lightShaftZoneCB.zones[0];
        zone.worldToLocal = lightShaftWorld.Invert();
        zone.halfExtents = Vec3(10.000f, 5.201f, 9.400f);
        zone.density = 1.f;
        zone.lightColor = Vec4(1.f, 242.f / 255.f, 204.f / 255.f, 1.f);
        zone.edgeFade = 1.f;
        zone.intensity = 0.3f;

        return lightShaftZoneCB;
    }
}

Level_GamePlay::Level_GamePlay(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : StreamingLevel{ device, context }
{
}

HRESULT Level_GamePlay::Initialize()
{
    //if (FAILED(Ready_ChunkMap()))
    //    return E_FAIL;

    if (FAILED(Ready_Lights()))
        return E_FAIL;

    if (FAILED(Ready_Layer_Player(TEXT("Layer_Player"))))
        return E_FAIL;

    RuntimeRuleStateService::Apply_ToCurrentRuntime(
        RuntimeRuleStateService::Build_GameplayStartUnlocks(), false);

    //if (FAILED(Ready_Layer_Effect(TEXT("Layer_Effect"))))
    //    return E_FAIL;

    if (FAILED(Ready_Layer_UI(TEXT("Layer_UI"))))
        return E_FAIL;

    if (FAILED(Ready_Layer_BackGround(TEXT("Layer_BackGround"))))
        return E_FAIL;

    if (FAILED(Ready_Layer_Camera(TEXT("Layer_Camera"))))
        return E_FAIL;

    if (FAILED(Ready_Layer_Monster(TEXT("Layer_Monster"))))
        return E_FAIL;

    if (FAILED(Ready_Layer_Puzzle(TEXT("Layer_Puzzle"))))
        return E_FAIL;

    if (FAILED(Ready_Layer_Trigger(TEXT("Layer_Trigger"))))
        return E_FAIL;

    if (FAILED(Ready_Layer_BattleStartSpawner(TEXT("Layer_BattleStartSpawner"))))
        return E_FAIL;

    if (FAILED(Ready_Layer_AmbientVisibility(TEXT("Layer_BattleStartSpawner"))))
        return E_FAIL;

    Disable_LandscapeLOD();

    // 안개, 빛 설정
    GAME->Set_RenderEnvironment(true);
    if (FAILED(GAME->Change_IBL(
        L"../../../Client/Bin/Resources/Textures/Shader/IBL/irradiance_eye.dds",
        L"../../../Client/Bin/Resources/Textures/Shader/IBL/perfilter_eye.dds")))
        return E_FAIL;

    GAME->Play_BGM(Constants::SoundBGM::SpringBGM, 0.7f, true, 0.f);

    return S_OK;
}

void Level_GamePlay::BeginPlay()
{
    StreamingLevel::BeginPlay();

    GAME->Set_VolumetricLightShaftZoneCullingDistance(40.f);
    GAME->Set_VolumetricLightShaftZones(Build_GamePlayLightShaftZones());
    Apply_GamePlayRenderParams();

    CLIENT->Capture_InitialWorldSnapshot();
    CLIENT->Queue_LevelAnnounce(LevelType::GamePlay);
}

void Level_GamePlay::Update(float fTimeDelta)
{
    //Update_MapStreaming(fTimeDelta);
    if (GAME->KeyPress(KEY_TYPE::F7))
        Respawn_CinematicTrigger();

    // 주교 앞으로 순간이동
    if (GAME->KeyDown(KEY_TYPE::KEY_9))
    {
        auto active = CLIENT->Get_ActiveHero();
        CHECK_NULL(active);

        active->Get_Transform()->Set_WorldPosition(Vec3(75.44f, 21.99f, -14.77f));

        if (auto movement = active->Get_Component<MovementCom>())
            movement->Sync_RuntimePositionToTransform();
    }

}

HRESULT Level_GamePlay::Render()
{
    Update_WindowTitleFps(_windowTitleFps, L"Level GamePlay");

    return S_OK;
}

HRESULT Level_GamePlay::Ready_ChunkMap()
{
    MapStreamingDesc streamingDesc{};
    streamingDesc.chunkRoot = Constants::Map::SpringChunk_Path;
    streamingDesc.layerTag = L"Layer_MapStaticMesh";
    streamingDesc.levelIndex = ETOI(LevelType::GamePlay);
    streamingDesc.gridWidth = 6;
    streamingDesc.gridHeight = 6;
    streamingDesc.initialChunkX = 3; // chunkX = 0, chunkZ = 2면 02_00 기준으로 4개 스폰
    streamingDesc.initialChunkZ = 0;
    streamingDesc.activeChunkRadius = 1;
    streamingDesc.maxMeshesPerFrame = 2;
    streamingDesc.maxTimePerFrame = chrono::milliseconds(2);

    CHECK_FAILED(Initialize_MapStreaming(streamingDesc), E_FAIL);

    return S_OK;
}

HRESULT Level_GamePlay::Ready_Lights()
{
    GAME->Reset_DynamicShadowCascadeTemporalUpdateIntervals();
    GAME->Clear_Lights();

    LIGHT_DESC LightDesc{};

    LightDesc.type = LightType::Directional;
    LightDesc.direction = Vec4(-0.8f, -0.6f, -1.f, 0.f);
    LightDesc.diffuse = Vec4(1.f, 1.f, 1.f, 1.f);
    LightDesc.ambient = Vec4(0.4f, 0.4f, 0.4f, 1.f);
    LightDesc.specular = Vec4(1.f, 1.f, 1.f, 1.f);
    LightDesc.castShadow = true;

    if (FAILED(GAME->Add_Light(LightDesc)))
        return E_FAIL;

    {
        ShadowLightDesc ShadowDesc{};

        ShadowDesc.eye = Vec3(0.f, 15.f, -7.f);
        ShadowDesc.at = Vec3(0.f, 0.f, 0.f);

        // Camera 따라다니게
        ShadowDesc.followCameraFrustum = true;
        ShadowDesc.focusNear = 0.1f;
        ShadowDesc.focusFar = 110.f;
        ShadowDesc.lightDistancePadding = 30.f;
        // 그림자용 ortho projection 범위 여유 값
        ShadowDesc.xyPadding = 2.f;
        ShadowDesc.zPadding = 40.f;

        // Shadow map 해상도. 4096 * 4096 * 4 * 4byte = 256mb
        ShadowDesc.shadowMapWidth = 4096;
        ShadowDesc.shadowMapHeight = 4096;

        ShadowDesc.cascadeSplitLambda = 0.8f;
        ShadowDesc.bias = 0.0002f;
        ShadowDesc.intensity = 0.6f;

        if (FAILED(GAME->Update_ShadowLight(ShadowDesc)))
            return E_FAIL;

        // 카메라 close/normal/far 모드에 따라서 그림자 구간을 동적 조절하기 위한 프리셋 설정
        ShadowCascadePresetSet cascadePresets{};
        cascadePresets.close.focusNear = 0.1f;
        cascadePresets.close.focusFar = 28.f;
        cascadePresets.close.cascadeSplitLambda = 0.95f;
        cascadePresets.close.cascadeBlendDistance = 0.8f;

        cascadePresets.normal.focusNear = 0.1f;
        cascadePresets.normal.focusFar = 110.f;
        cascadePresets.normal.cascadeSplitLambda = 0.8f;
        cascadePresets.normal.cascadeBlendDistance = 1.7f;

        cascadePresets.farRange.focusNear = 0.1f;
        cascadePresets.farRange.focusFar = 180.f;
        cascadePresets.farRange.cascadeSplitLambda = 0.55f;
        cascadePresets.farRange.cascadeBlendDistance = 3.f;

        if (FAILED(GAME->Set_ShadowCascadePresets(cascadePresets)))
            return E_FAIL;
        if (FAILED(GAME->Set_ShadowCascadeLevel(CascadeLevel::Normal)))
            return E_FAIL;
    }

    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Puzzle(const wstring& strLayerTag)
{
    // Eyes
    {
        const Vec3 kEyePos1 = Vec3(-11.87f, 21.69f, -51.99f);
        const Vec3 kEyePos2 = Vec3(-0.02f, 34.96f, -16.36f);
        const Vec3 kEyePos3 = Vec3(4.69f, 24.92f, -17.56f);

        auto eyeTarget1 = Spawn_Prefab::Prefab(Constants::Prefab::Eye_Target)
            .AtLevel(ETOI(LevelType::GamePlay))
            .InLayer(L"Layer_EyeTarget")
            .Position(kEyePos1)
            .Spawn();

        auto eyeTarget2 = Spawn_Prefab::Prefab(Constants::Prefab::Eye_Target)
            .AtLevel(ETOI(LevelType::GamePlay))
            .InLayer(L"Layer_EyeTarget")
            .Position(kEyePos2)
            .Spawn();

        auto eyeTarget3 = Spawn_Prefab::Prefab(Constants::Prefab::Eye_Target)
            .AtLevel(ETOI(LevelType::GamePlay))
            .InLayer(L"Layer_EyeTarget")
            .Position(kEyePos3)
            .Spawn();
    }

    //퍼즐 컨트롤러
    Shared<GameObject> puzzleController;
    CHECK_FAILED(
        GAME->Add_GameObject(
            ETOI(LevelType::Static),
            L"GameObject_PuzzleController",
            ETOI(LevelType::GamePlay),
            L"Layer_PuzzleController",
            puzzleController),
        E_FAIL);

    CHECK_NULL(puzzleController, E_FAIL);

    Shared<PuzzleController> controller = dynamic_pointer_cast<PuzzleController>(puzzleController);
    LOG_DEBUG("controller cast = {}", controller != nullptr);

    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Trigger(const wstring& strLayerTag)
{
    // 주교 시네마틱 트리거
    {
        Shared<GameObject> spawned = Spawn_Prefab::Prefab(Constants::Prefab::Trigger_Cinematic)
            .AtLevel(ETOI(LevelType::GamePlay))
            .InLayer(L"Layer_GameObject")
            .Position(kCinematicTriggerSpawnPosition)
            .Spawn();

        const Shared<Trigger_Cinematic> trigger = dynamic_pointer_cast<Trigger_Cinematic>(spawned);
        if (trigger != nullptr)
        {
            trigger->Set_SequenceFileId(Constants::Cinematic::BishopCinematicFileId);
            trigger->Set_StartBattleOnComplete(true, "Boss");
            trigger->Set_OnTriggered([this]() { Apply_BishopCinematicLightDirection(); });
        }

    }

    {
        Shared<GameObject> tutorialSpawned = Spawn_Prefab::Prefab(Constants::Prefab::Trigger_Cinematic)
            .AtLevel(ETOI(LevelType::GamePlay))
            .InLayer(L"Layer_GameObject")
            .Position(kTutoalCinematicTriggerSpawnPosition)
            .Spawn();

        const Shared<Trigger_Cinematic> tutorialTrigger = dynamic_pointer_cast<Trigger_Cinematic>(tutorialSpawned);
        if (tutorialTrigger != nullptr)
        {
            tutorialTrigger->Set_SequenceFileId(Constants::Cinematic::TutorialCinematicFileId);
            tutorialTrigger->Set_StartBattleOnComplete(true, "Lancelier");
        }
    }



    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_BattleStartSpawner(const wstring& strLayerTag)
{
    // 튜토리얼 렌서러
    auto tutorialStart = Spawn_Prefab::Prefab(Constants::Prefab::BSS_Gameplay01)
        .AtLevel(ETOI(LevelType::GamePlay))
        .InLayer(strLayerTag)
        .Position(kLancelierBattlePosition)
        .Rotation(kLancelierBattleRotation)
        .Spawn();

    // 방패병
    auto portierStart = Spawn_Prefab::Prefab(Constants::Prefab::BSS_Gameplay02)
        .AtLevel(ETOI(LevelType::GamePlay))
        .InLayer(strLayerTag)
        .Position(kPortierBattlePosition)
        .Rotation(kPortierBattleRotation)
        .Spawn();

    // 보스 전투 시작 스포너
    auto bishopStart = Spawn_Prefab::Prefab(Constants::Prefab::BattleStartSpawner_Boss)
        .AtLevel(ETOI(LevelType::GamePlay))
        .InLayer(strLayerTag)
        .Position(kBossBattlePosition)
        .Rotation(kBossBattleRotation)
        .Spawn();


    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_AmbientVisibility(const wstring& strLayerTag)
{
    MapAmbientVisibility::MAP_AMBIENT_VISIBILITY_DESC bridgeDesc{};
    bridgeDesc.name = L"AV_Cylinder_Test";
    bridgeDesc.modelPath = L"../../../Client/Bin/Resources/Map/SmallLevel_MF_Zone_01/Cube_Nanite.model";
    bridgeDesc.position = Vec3(-0.17f, 21.52f, -19.4f);
    bridgeDesc.rotation = Quat::Identity;
    bridgeDesc.scale = Vec3(170.f, 80.f, 200.f);
    bridgeDesc.ambientVisibilityAmount = 0.f;
    bridgeDesc.visible = true;
    bridgeDesc.cullEnabled = true;

    CHECK_FAILED(GAME->Add_GameObject(
        ETOI(LevelType::Static),
        TEXT("GameObject_MapAmbientVisibility"),
        ETOI(LevelType::GamePlay),
        TEXT("Layer_AmbientVisibility"),
        &bridgeDesc),
        E_FAIL);

    return S_OK;
}

void Level_GamePlay::Respawn_CinematicTrigger()
{
    vector<Shared<GameObject>> triggersToRemove{};

    GAME->Visit_LevelObjects(
        ETOI(LevelType::GamePlay),
        [&](const wstring&, const Shared<GameObject>& gameObject)
        {
            if (dynamic_pointer_cast<Trigger_Cinematic>(gameObject) == nullptr)
                return;

            triggersToRemove.push_back(gameObject);
        });

    for (const Shared<GameObject>& triggerObject : triggersToRemove)
        GAME->Remove_GameObject(ETOI(LevelType::GamePlay), triggerObject);

    GAME->Flush_RemovedGameObjects();

    Shared<GameObject> spawned = Spawn_Prefab::Prefab(Constants::Prefab::Trigger_Cinematic)
        .AtLevel(ETOI(LevelType::GamePlay))
        .InLayer(L"Layer_GameObject")
        .Position(kCinematicTriggerSpawnPosition)
        .Spawn();

    const Shared<Trigger_Cinematic> trigger = dynamic_pointer_cast<Trigger_Cinematic>(spawned);
    if (trigger != nullptr)
    {
        trigger->Set_SequenceFileId(Constants::Cinematic::BishopCinematicFileId);
        trigger->Set_StartBattleOnComplete(true, "Boss");
        trigger->Set_OnTriggered([this]() { Apply_BishopCinematicLightDirection(); });
    }

    Shared<GameObject> tutorialSpawned = Spawn_Prefab::Prefab(Constants::Prefab::Trigger_Cinematic)
        .AtLevel(ETOI(LevelType::GamePlay))
        .InLayer(L"Layer_GameObject")
        .Position(kTutoalCinematicTriggerSpawnPosition)
        .Spawn();

    const Shared<Trigger_Cinematic> tutorialTrigger = dynamic_pointer_cast<Trigger_Cinematic>(tutorialSpawned);
    if (tutorialTrigger != nullptr)
    {
        tutorialTrigger->Set_SequenceFileId(Constants::Cinematic::TutorialCinematicFileId);
        tutorialTrigger->Set_StartBattleOnComplete(true, "Lancelier");
    }

}

void Level_GamePlay::Apply_BishopCinematicLightDirection()
{
    const LIGHT_DESC* currentLightDesc = GAME->Get_PrimaryShadowLightDesc();
    if (currentLightDesc == nullptr || currentLightDesc->type != LightType::Directional)
        return;

    Vec3 lightDirection(-2.240f, -2.970f, -1.000f);
    if (lightDirection.LengthSquared() <= 0.0001f)
        return;

    lightDirection.Normalize();

    LIGHT_DESC lightDesc = *currentLightDesc;
    lightDesc.direction = Vec4(lightDirection.x, lightDirection.y, lightDirection.z, 0.f);

    if (FAILED(GAME->Update_PrimaryShadowLightDesc(lightDesc)))
        LOG_WARN("[Level_GamePlay] Failed to apply Bishop cinematic light direction.");
}

void Level_GamePlay::Apply_GamePlayRenderParams()
{
    Shared<LightingPipeline> lightingPipeline = GAME->Get_LightingPipeline();
    Shared<PostProcessPipeline> postProcessPipeline = GAME->Get_PostProcessPipeline();
    if (_hasSavedGamePlayRenderParams)
        return;

    GAME->Get_LightDescs(_savedLightDescs);
    if (lightingPipeline != nullptr)
    {
        _savedSSAOCB = lightingPipeline->Get_Params();
        _savedDeferredLightingCB = lightingPipeline->Get_DeferredLightingParams();
    }
    if (postProcessPipeline != nullptr)
        _savedPostProcessFeatures = postProcessPipeline->Get_PostProcessFeatures();

    _hasSavedGamePlayRenderParams = true;

    Vec3 lightDirection(-7.300f, -5.030f, -1.000f);
    if (lightDirection.LengthSquared() > 0.0001f)
    {
        lightDirection.Normalize();

        vector<LIGHT_DESC> lightDescs = _savedLightDescs;
        for (LIGHT_DESC& lightDesc : lightDescs)
        {
            if (lightDesc.type != LightType::Directional)
                continue;

            lightDesc.direction = Vec4(lightDirection.x, lightDirection.y, lightDirection.z, 0.f);
            break;
        }
        if (!lightDescs.empty())
            GAME->Set_Lights(lightDescs);
    }

    if (lightingPipeline != nullptr)
    {
        SSAOCB ssao = _savedSSAOCB;
        ssao.SSAOEnabled = 1u;
        ssao.SSAORadius = 0.100f;
        ssao.SSAOBias = 0.0420f;
        ssao.SSAOIntensity = 1.000f;
        ssao.SSAOPower = 1.200f;
        ssao.SSAODepthRange = 1.000f;
        ssao.SSAOSampleCount = 16u;
        lightingPipeline->Set_Params(ssao);

        DeferredLightingCB lighting = _savedDeferredLightingCB;
        lighting.stylizedLightContrastEnabled = 1u;
        lighting.stylizedLightContrastStrength = 0.11f;
        lighting.stylizedLightThreshold = 0.28f;
        lighting.stylizedLightSoftness = 0.283f;
        lighting.stylizedLitBoost = 4.00f;
        lighting.stylizedUnlitAmbientScale = 0.00f;
        lighting.stylizedDeepUnlitThreshold = 0.00f;
        lighting.stylizedDeepUnlitSoftness = 0.001f;
        lighting.stylizedDeepUnlitAmbientScale = 0.00f;
        lightingPipeline->Set_DeferredLightingParams(lighting);
    }
}

void Level_GamePlay::Restore_GamePlayRenderParams()
{
    if (!_hasSavedGamePlayRenderParams)
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
    _hasSavedGamePlayRenderParams = false;
}

HRESULT Level_GamePlay::Ready_Layer_UI(const wstring& strLayerTag)
{
    CHECK_FAILED(CLIENT->Initialize_GameplayUI(ETOI(LevelType::GamePlay)), E_FAIL);

    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Camera(const wstring& strLayerTag)
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
            ETOI(LevelType::GamePlay),
            TEXT("GameObject_TargetCamera_GamePlay"),
            ETOI(LevelType::GamePlay),
            strLayerTag,
            cameraObject,
            &TargetCameraDesc),
        E_FAIL
    );

    const auto targetCamera = dynamic_pointer_cast<TargetCamera>(cameraObject);
    CLIENT->Set_TargetCamera(targetCamera);

    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_BackGround(const wstring& strLayerTag)
{
    SkyBoxFlow::SKYBOXFLOW_DESC skyBoxDesc{};
    skyBoxDesc.modelTag = TEXT("Model_SM_Skybox_Mesh");
    skyBoxDesc.shaderTag = TEXT("Shader_SkyBoxFlow");
    // =======================================================================================
    // =======================================================================================
    // =======================================================================================
    // =======================================================================================
    // =======================================================================================
    // =======================================================================================
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
            ETOI(LevelType::GamePlay),
            TEXT("GameObject_SkyBoxFlow_GamePlay"),
            ETOI(LevelType::GamePlay),
            strLayerTag,
            &skyBoxDesc),
        E_FAIL);

    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Monster(const wstring& strLayerTag)
{
    //Vec3 startPos = Vec3(-77.f, 19.1f, -117.94f);
    //
    //const Shared<GameObject> monster = Client::Spawn_Prefab::Spawn(
    //    Constants::Prefab::MW_Lancelier,
    //    ETOI(LevelType::GamePlay),
    //    strLayerTag);
    //
    //CHECK_NULL(monster, E_FAIL);
    //monster->Get_Transform()->Set_Position(startPos);

    // 이제 여기서 스폰안하고, 에디터에서 스폰&저장 진행.

    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Player(const wstring& strLayerTag)
{
    // 플레이어 시작 위치만 지정
    Vec3 startPos = Vec3(-87.f, 19.47f, -132.94f);
    CHECK_FAILED(CLIENT->Initialize_Hero(ETOI(LevelType::GamePlay), strLayerTag, startPos), E_FAIL);

    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Effect(const wstring& strLayerTag)
{
    return S_OK;
}

void Level_GamePlay::Disable_LandscapeLOD()
{
    GAME->Visit_LevelObjects(
        ETOI(LevelType::GamePlay),
        [](const wstring&, const Shared<GameObject>& gameObject)
        {
            const Shared<MapLandscape> landscape = dynamic_pointer_cast<MapLandscape>(gameObject);
            if (landscape != nullptr)
                landscape->Set_UseLOD(false);
        });
}

Shared<Level_GamePlay> Level_GamePlay::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto pInstance = make_shared<Level_GamePlay>(device, context);

    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Created : Level_GamePlay");
        return nullptr;
    }
    return pInstance;
}

void Level_GamePlay::Free()
{
    Restore_GamePlayRenderParams();

    GAME->Set_VolumetricLightShaftZones(VolumetricLightShaftZoneCB{});
    Free_MapStreaming();

    CLIENT->Clear_GameplayUI();

    __super::Free();
}
