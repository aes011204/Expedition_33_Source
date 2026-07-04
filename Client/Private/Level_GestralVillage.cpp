#include "Level_GestralVillage.h"
#include "Client_Constants.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "GameObject.h"
#include "SkyBoxFlow.h"
#include "Spawn_Prefab.h"
#include "TargetCamera.h"
#include "SoulLikeController.h"
#include "MapAmbientVisibility.h"
#include "BattleStartSpawner.h"
#include "RuntimeRuleStateService.h"
#include "ColliderCom.h"
#include "NPC_Base.h"
#include "Player_WorldBase.h"
#include "MovementCom.h"

namespace
{
    VolumetricLightShaftZoneCB Build_GestralVillageLightShaftZones()
    {
        VolumetricLightShaftZoneCB lightShaftZoneCB{};
        lightShaftZoneCB.zoneCount = 1;

        Matrix lightShaftWorld =
            Matrix::CreateFromYawPitchRoll(
                DirectX::XMConvertToRadians(20.959f),
                DirectX::XMConvertToRadians(-0.000f),
                DirectX::XMConvertToRadians(-0.000f)) *
            Matrix::CreateTranslation(Vec3(11.399f, 10.280f, 92.334f));

        VolumetricLightShaftZoneEntry& zone = lightShaftZoneCB.zones[0];
        zone.worldToLocal = lightShaftWorld.Invert();
        zone.halfExtents = Vec3(12.410f, 12.600f, 6.600f);
        zone.density = 1.f;
        zone.lightColor = Vec4(1.f, 242.f / 255.f, 204.f / 255.f, 1.f);
        zone.edgeFade = 1.f;
        zone.intensity = 0.410f;

        return lightShaftZoneCB;
    }

    constexpr wchar_t kFlameEffectName[] = L"Effects/Assets/Map/FX_Brazier_Fire.effect.json";
    constexpr wchar_t kLeafEffectName[] = L"Effects/Assets/Map/FX_FallingLeaf.effect.json";

    constexpr Vec3 kCasinoLightPositions[] = {
        Vec3(118.12f, -3.23f, 19.45f),
        Vec3(123.58f, -3.23f, 17.01f),
        Vec3(130.79f, -3.23f, 13.69f),
        Vec3(138.57f, -3.23f, 10.15f),
        Vec3(146.17f, -3.23f, 6.77f),
        Vec3(153.71f, -3.23f, 3.37f),
        Vec3(162.12f, -3.23f, -0.44f),
        Vec3(169.83f, -3.23f, -4.09f),
        Vec3(161.36f, -3.23f, -22.58f),
        Vec3(153.53f, -3.23f, -18.87f),
        Vec3(144.43f, -3.23f, -15.01f),
        Vec3(137.51f, -3.23f, -11.88f),
        Vec3(129.94f, -3.23f, -8.20f),
        Vec3(121.95f, -3.23f, -4.96f),
        Vec3(114.97f, -3.23f, -1.60f),
        Vec3(109.85f, -3.23f, 0.82f),
    };

    constexpr Vec3 kSymbolLightPosition[] = {
        Vec3(9.53f, 10.14f, 104.55f),
        Vec3(11.87f, 10.23f, 99.53f),
        Vec3(12.9f, 10.53f, 93.59f),
        Vec3(10.28f, 10.58f, 87.98f),
        Vec3(-5.59f, 8.9f, 86.44f),
        Vec3(-7.37f, 8.9f, 91.5f),
        Vec3(-7.65f, 8.82f, 96.49f),
        Vec3(-6.46f, 8.9f, 101.09f)
    };
}

Level_GestralVillage::Level_GestralVillage(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Level{ device, context }
{
}

HRESULT Level_GestralVillage::Initialize()
{
    if (FAILED(Ready_Lights()))
        return E_FAIL;

    if (FAILED(Ready_Layer_Player(TEXT("Layer_Player"))))
        return E_FAIL;

    UnlockRules_GestralVillage();

    if (FAILED(Ready_Layer_UI(TEXT("Layer_UI"))))
        return E_FAIL;

    if (FAILED(Ready_Layer_BackGround(TEXT("Layer_BackGround"))))
        return E_FAIL;

    if (FAILED(Ready_Layer_Camera(TEXT("Layer_Camera"))))
        return E_FAIL;

    if (FAILED(Ready_Layer_NPC(TEXT("Layer_NPC"))))
        return E_FAIL;

    if (FAILED(Ready_Layer_ETC(TEXT("Layer_ETC"))))
        return E_FAIL;

    // 안개, 빛 설정
    GAME->Set_RenderEnvironment(true);
    if (FAILED(GAME->Change_IBL(
        L"../../../Client/Bin/Resources/Textures/Shader/IBL/irradiance_sunset.dds",
        L"../../../Client/Bin/Resources/Textures/Shader/IBL/perfilter_sunset.dds")))
        return E_FAIL;

    GAME->Set_FogZoneCullingDistance(15.f);


    return S_OK;
}

void Level_GestralVillage::BeginPlay()
{
    Level::BeginPlay();

    GAME->Set_VolumetricLightShaftZoneCullingDistance(30.f);
    GAME->Set_VolumetricLightShaftZones(Build_GestralVillageLightShaftZones());
    Play_MapBGM();

    _editorPlayRuntimeHandle =
        GAME->Get_DelegateHub().OnEditorPlayRuntime.Add(this, &Level_GestralVillage::Play_MapBGM);

    CLIENT->Capture_InitialWorldSnapshot();
    CLIENT->Queue_LevelAnnounce(LevelType::GestralVillage);

    // flame
    PlayEffectDesc desc{};
    desc.effectName = kFlameEffectName;
    desc.scale = Vec3(1.f, 1.f, 1.f);

    for (const auto& pos : kCasinoLightPositions)
    {
        desc.worldPosition = pos;
        CLIENT->Play_Effect(desc);
    }

    for (const auto& pos : kSymbolLightPosition)
    {
        desc.worldPosition = pos;
        CLIENT->Play_Effect(desc);
    }

    // leaf
    PlayEffectDesc leafDesc{};
    leafDesc.effectName = kLeafEffectName;
    leafDesc.worldPosition = Vec3(2.38f, -0.05f, 143.15f);
    leafDesc.scale = Vec3(1.f, 1.f, 1.f);

    CLIENT->Play_Effect(leafDesc);
}

void Level_GestralVillage::Update(float fTimeDelta)
{
    // 감자자루 NPC 앞으로 순간이동
    if (GAME->KeyDown(KEY_TYPE::KEY_9))
    {
        auto active = CLIENT->Get_ActiveHero();
        CHECK_NULL(active);

        active->Get_Transform()->Set_WorldPosition(Vec3(6.33f, -6.73, 2.7f));

        if (auto movement = active->Get_Component<MovementCom>())
            movement->Sync_RuntimePositionToTransform();
    }

    if (GAME->KeyDown(KEY_TYPE::F1))
        ClientInstance::Toggle_PotatoBagArenaMapRenderGateDebug();

    ClientInstance::Draw_PotatoBagArenaMapRenderGateDebug();
}

HRESULT Level_GestralVillage::Render()
{
    Update_WindowTitleFps(_windowTitleFps, L"Level_GestralVillage");

    return S_OK;
}

HRESULT Level_GestralVillage::Ready_Lights()
{
    GAME->Clear_Lights();

    LIGHT_DESC LightDesc{};

    LightDesc.type = LightType::Directional;
    LightDesc.direction = Vec4(-0.8f, -0.53f, -0.05f, 0.f);
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
        ShadowDesc.zPadding = 10.f;

        // Shadow map 해상도. 4096 * 4096 * 4 * 4byte = 256mb
        // fix 4096 -> 2048
        ShadowDesc.shadowMapWidth = 2048;
        ShadowDesc.shadowMapHeight = 2048;

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
        cascadePresets.normal.focusFar = 60.f;
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

        GAME->Set_DynamicShadowCascadeTemporalUpdateIntervals(1u, 1u, 2u, 12u);
    }

    return S_OK;
}

HRESULT Level_GestralVillage::Ready_Environments()
{
    GAME->Set_FogZoneCullingDistance(30.f);
    GAME->Set_VolumetricLightShaftZoneCullingDistance(15.f);

    return S_OK;
}

HRESULT Level_GestralVillage::Ready_Layer_UI(const wstring& strLayerTag)
{
    CHECK_FAILED(CLIENT->Initialize_GameplayUI(ETOI(LevelType::GestralVillage)), E_FAIL);

    return S_OK;
}

HRESULT Level_GestralVillage::Ready_Layer_Camera(const wstring& strLayerTag)
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
            ETOI(LevelType::GestralVillage),
            TEXT("GameObject_TargetCamera_Gestral"),
            ETOI(LevelType::GestralVillage),
            strLayerTag,
            cameraObject,
            &TargetCameraDesc),
        E_FAIL
    );

    const auto targetCamera = dynamic_pointer_cast<TargetCamera>(cameraObject);
    if (targetCamera != nullptr)
        targetCamera->Set_OrbitAngles(-175.7f, 1.1f);

    CLIENT->Set_TargetCamera(targetCamera);

    return S_OK;
}

HRESULT Level_GestralVillage::Ready_Layer_BackGround(const wstring& strLayerTag)
{
    SkyBoxFlow::SKYBOXFLOW_DESC skyBoxDesc{};
    skyBoxDesc.modelTag = TEXT("Model_SM_Skybox_Mesh");
    skyBoxDesc.shaderTag = TEXT("Shader_SkyBoxFlow");
    skyBoxDesc.skyTextureTag = TEXT("Texture_SkyBox_Gestral");
    skyBoxDesc.flowTextureTag = TEXT("Texture_SkyBox_Flow03");

    skyBoxDesc.tint = Vec4(1.604023f, 0.887117f, 0.631814f, 1.f);
    skyBoxDesc.brightness = 500.f;
    skyBoxDesc.contrast = 1.524392f;
    skyBoxDesc.noiseStrength = 0.25f;
    skyBoxDesc.speed = 0.001f;
    skyBoxDesc.scale = 1.f;

    CHECK_FAILED(
        GAME->Add_GameObject(
            ETOI(LevelType::GestralVillage),
            TEXT("GameObject_SkyBoxFlow_GestralVillage"),
            ETOI(LevelType::GestralVillage),
            strLayerTag,
            &skyBoxDesc),
        E_FAIL);

    return S_OK;
}

HRESULT Level_GestralVillage::Ready_Layer_Player(const wstring& strLayerTag)
{
    Vec3 startPos = Vec3(-2.f, -5.46f, 183.54f);
    Vec3 startRot = Vec3(0.f, -173.f, 0.f);

    CHECK_FAILED(CLIENT->Initialize_Hero(ETOI(LevelType::GestralVillage), strLayerTag, startPos, startRot), E_FAIL);

    return S_OK;
}

HRESULT Level_GestralVillage::Ready_Layer_NPC(const wstring& strLayerTag)
{
    const Shared<GameObject> noco = Spawn_Prefab::Prefab("NPC_Noco")
        .AtLevel(ETOI(LevelType::GestralVillage))
        .InLayer(strLayerTag)
        .Position(Vec3(7.29f, 0.42f, 143.f))
        .Rotation(Vec3(0.f, -90.f, 0.f))
        .Scale(Vec3(0.6f, 0.6f, 0.6f))
        .Spawn();

    CHECK_NULL(noco, E_FAIL);

    const Shared<GameObject> golgra = Spawn_Prefab::Prefab("NPC_Golgra")
        .AtLevel(ETOI(LevelType::GestralVillage))
        .InLayer(strLayerTag)
        .Position(Vec3(2.41f, 3.50f, 88.82f))
        .Rotation(Vec3(0.f, 0.f, 0.f))
        .Spawn();

    CHECK_NULL(golgra, E_FAIL);

    const Shared<GameObject> merchant = Spawn_Prefab::Prefab("NPC_Merchant")
        .AtLevel(ETOI(LevelType::GestralVillage))
        .InLayer(strLayerTag)
        .Position(Vec3(-62.30f, 1.03f, 126.23f))
        .Rotation(Vec3(0.f, 168.5f, 0.f))
        .Spawn();

    CHECK_NULL(merchant, E_FAIL);

    const Shared<GameObject> chad = Spawn_Prefab::Prefab("NPC_Chad")
        .AtLevel(ETOI(LevelType::GestralVillage))
        .InLayer(strLayerTag)
        .Position(Vec3(-26.71f, 1.24f, 138.15f))
        .Rotation(Vec3(0.f, 179.1f, 0.f))
        .Spawn();

    CHECK_NULL(chad, E_FAIL);

    const Shared<GameObject> sumo = Spawn_Prefab::Prefab("NPC_Sumo")
        .AtLevel(ETOI(LevelType::GestralVillage))
        .InLayer(strLayerTag)
        .Position(Vec3(4.36f, -6.4f, 11.05f))
        .Rotation(Vec3(0.f, 180.f, 0.f))
        .Spawn();

    CHECK_NULL(chad, E_FAIL);

    const Shared<GameObject> dance = Spawn_Prefab::Prefab("NPC_Dance")
        .AtLevel(ETOI(LevelType::GestralVillage))
        .InLayer(strLayerTag)
        .Position(Vec3(-62.24f, -1.59f, 38.08f))
        .Rotation(Vec3(0.f, 111.6f, 0.f))
        .Scale(Vec3(1.f, 1.f, 1.f))
        .Spawn();

    CHECK_NULL(dance, E_FAIL);

    auto buff = dynamic_pointer_cast<NPC_Base>(dance);
    CHECK_NULL(buff, E_FAIL);

    buff->Set_FightVisible(false);

    return S_OK;
}

HRESULT Level_GestralVillage::Ready_Layer_Monster(const wstring& strLayerTag)
{

    return S_OK;
}

HRESULT Level_GestralVillage::Ready_Layer_Effect(const wstring& strLayerTag)
{
    return S_OK;
}

HRESULT Level_GestralVillage::Ready_Layer_ETC(const wstring& strLayerTag)
{
    // 골그라 브금 전환
    const Shared<GameObject> bgmZone = Spawn_Prefab::Prefab(Constants::Prefab::Trigger_BGMZone)
        .AtLevel(ETOI(LevelType::GestralVillage))
        .InLayer(strLayerTag)
        .Position(Vec3(-0.46f, -1.64f, 95.23f))
        .Rotation(Vec3(0.f, 0.f, 0.f))
        .Scale(Vec3(4.5f, 5.f, 5.f))
        .Spawn();

    CHECK_NULL(bgmZone, E_FAIL);

    {
        MapAmbientVisibility::MAP_AMBIENT_VISIBILITY_DESC symbolDesc{};
        symbolDesc.name = L"AV_Cylinder_Symbol";
        symbolDesc.modelPath = L"../../../Client/Bin/Resources/Map/Level_Main_GestralVillage_V2/Cylinder.model";
        symbolDesc.position = Vec3(3.41f, 17.03f, 92.3f);
        symbolDesc.rotation = Quat::Identity;
        symbolDesc.scale = Vec3(18000.f, 19500.f, 18000.f);
        symbolDesc.visible = true;
        symbolDesc.cullEnabled = true;

        CHECK_FAILED(GAME->Add_GameObject(
            ETOI(LevelType::Static),
            TEXT("GameObject_MapAmbientVisibility"),
            ETOI(LevelType::GestralVillage),
            TEXT("Layer_AmbientVisibility"),
            &symbolDesc),
            E_FAIL);

        MapAmbientVisibility::MAP_AMBIENT_VISIBILITY_DESC caveDesc{};
        caveDesc.name = L"AV_Cylinder_Test";
        caveDesc.modelPath = L"../../../Client/Bin/Resources/Map/Level_Main_GestralVillage_V2/Cylinder.model";
        caveDesc.position = Vec3(38.52f, 4.38f, 124.41f);
        caveDesc.rotation = Quat::CreateFromYawPitchRoll(
            DirectX::XMConvertToRadians(27.f),   // Y 회전, yaw
            DirectX::XMConvertToRadians(0.f),    // X 회전, pitch
            DirectX::XMConvertToRadians(-3.f)    // Z 회전, roll
        );
        caveDesc.scale = Vec3(10000.f, 5000.f, 15000.f);
        caveDesc.visible = true;
        caveDesc.cullEnabled = true;

        CHECK_FAILED(GAME->Add_GameObject(
            ETOI(LevelType::Static),
            TEXT("GameObject_MapAmbientVisibility"),
            ETOI(LevelType::GestralVillage),
            TEXT("Layer_AmbientVisibility"),
            &caveDesc),
            E_FAIL);
    }

    // 감자자루 스폰
    auto potatobag = Spawn_Prefab::Prefab(Constants::Prefab::BSS_Gestral_PotatoBag)
        .AtLevel(ETOI(LevelType::GestralVillage))
        .InLayer(L"Layer_BattleStartSpawner")
        .Position(Vec3(1.1f, 3.68f, 53.87f))
        .Rotation(Vec3(0.f, -14.4f, 0.f))
        .Spawn();

    _bssPotatoBag = dynamic_pointer_cast<BattleStartSpawner>(potatobag);
    CHECK_NULL(_bssPotatoBag, E_FAIL);

    _bssPotatoBag->Set_SpawnerId(Constants::Value::potatoBagBattleStartSpawnerId);
    if (const Shared<TransformCom> spawnerTransform = _bssPotatoBag->Get_Transform())
    {
        Vec3 right = spawnerTransform->Get_WorldRight();
        Vec3 forward = spawnerTransform->Get_WorldForward();

        right.y = 0.f;
        forward.y = 0.f;

        if (right.LengthSquared() > 0.0001f)
            right.Normalize();
        else
            right = Vec3::Right;

        if (forward.LengthSquared() > 0.0001f)
            forward.Normalize();
        else
            forward = Vec3::Forward;

        ClientInstance::Set_PotatoBagArenaMapRenderGateBasis(
            spawnerTransform->Get_WorldPosition(),
            right,
            forward);
    }

    const Shared<GameObject> upDownRope = Spawn_Prefab::Prefab("ETC/WorldUpDownRopeSpot")
        .AtLevel(ETOI(LevelType::GestralVillage))
        .InLayer(strLayerTag)
        .Position(Vec3(-25.5f, 3.f, 90.5f))
        .Spawn();

    CHECK_NULL(upDownRope, E_FAIL);

    const Shared<GameObject> grapplingRope = Spawn_Prefab::Prefab("ETC/WorldGrapplingRopeSpot")
        .AtLevel(ETOI(LevelType::GestralVillage))
        .InLayer(strLayerTag)
        .Position(Vec3(-25.9f, 17.5f, 76.5f))
        .Rotation(Vec3(0.f, -90.f, 0.f))
        .Scale(Vec3(1.f, 1.f, 1.f))
        .Spawn();

    CHECK_NULL(grapplingRope, E_FAIL);

    if (const Shared<ColliderCom> gateCollider = grapplingRope->Get_Component<ColliderCom>(TEXT("Com_RopeGateCollider")))
    {
        ColliderShapeDesc gateShapeDesc = gateCollider->Get_ShapeDesc();
        gateShapeDesc.center = Vec3(0.220f, -1.500f, 9.420f);
        gateShapeDesc.extents = Vec3(2.000f, 1.000f, 0.500f);
        gateCollider->Set_ShapeDesc(gateShapeDesc);
    }

    if (const Shared<ColliderCom> landingCollider = grapplingRope->Get_Component<ColliderCom>(TEXT("Com_GrappleLandingDebugCollider")))
    {
        ColliderShapeDesc landingShapeDesc = landingCollider->Get_ShapeDesc();
        landingShapeDesc.center = Vec3(0.f, 0.480f, 0.f);
        landingShapeDesc.radius = 0.350f;
        landingCollider->Set_ShapeDesc(landingShapeDesc);
    }

    const Shared<GameObject> grapplingRope2 = Spawn_Prefab::Prefab("ETC/WorldGrapplingRopeSpot")
        .AtLevel(ETOI(LevelType::GestralVillage))
        .InLayer(strLayerTag)
        .Position(Vec3(-45.f, 9.f, 77.5f))
        .Rotation(Vec3(0.f, 0.f, 0.f))
        .Scale(Vec3(1.f, 1.f, 1.f))
        .Spawn();

    CHECK_NULL(grapplingRope2, E_FAIL);

    if (const Shared<ColliderCom> gateCollider = grapplingRope2->Get_Component<ColliderCom>(TEXT("Com_RopeGateCollider")))
    {
        ColliderShapeDesc gateShapeDesc = gateCollider->Get_ShapeDesc();
        gateShapeDesc.center = Vec3(12.320f, 9.790f, -3.020f);
        gateShapeDesc.extents = Vec3(1.220f, 1.000f, 2.210f);
        gateCollider->Set_ShapeDesc(gateShapeDesc);
    }

    if (const Shared<ColliderCom> landingCollider = grapplingRope2->Get_Component<ColliderCom>(TEXT("Com_GrappleLandingDebugCollider")))
    {
        ColliderShapeDesc landingShapeDesc = landingCollider->Get_ShapeDesc();
        landingShapeDesc.center = Vec3(0.f, 0.500f, 0.f);
        landingShapeDesc.radius = 0.350f;
        landingCollider->Set_ShapeDesc(landingShapeDesc);
    }

    const Shared<GameObject> grapplingRope3 = Spawn_Prefab::Prefab("ETC/WorldGrapplingRopeSpot")
        .AtLevel(ETOI(LevelType::GestralVillage))
        .InLayer(strLayerTag)
        .Position(Vec3(-66.15f, 14.f, 60.5f))
        .Rotation(Vec3(0.f, -6.5f, 0.f))
        .Scale(Vec3(1.f, 1.f, 1.f))
        .Spawn();

    CHECK_NULL(grapplingRope3, E_FAIL);

    if (const Shared<ColliderCom> gateCollider = grapplingRope3->Get_Component<ColliderCom>(TEXT("Com_RopeGateCollider")))
    {
        ColliderShapeDesc gateShapeDesc = gateCollider->Get_ShapeDesc();
        gateShapeDesc.center = Vec3(11.000f, 0.570f, 2.530f);
        gateShapeDesc.extents = Vec3(1.290f, 1.140f, 2.080f);
        gateCollider->Set_ShapeDesc(gateShapeDesc);
    }

    if (const Shared<ColliderCom> landingCollider = grapplingRope3->Get_Component<ColliderCom>(TEXT("Com_GrappleLandingDebugCollider")))
    {
        ColliderShapeDesc landingShapeDesc = landingCollider->Get_ShapeDesc();
        landingShapeDesc.center = Vec3(-1.370f, 0.800f, 0.060f);
        landingShapeDesc.radius = 0.350f;
        landingCollider->Set_ShapeDesc(landingShapeDesc);
    }

    return S_OK;
}

void Level_GestralVillage::UnlockRules_GestralVillage()
{
    RuntimeRuleStateService::Apply_ToCurrentRuntime(
        RuntimeRuleStateService::Build_GestralVillageStartUnlocks(), false);

    CLIENT->Restore_PartyFormationAfterEditorStop();
}

void Level_GestralVillage::Play_MapBGM()
{
    GAME->Play_BGM(Constants::SoundBGM::GestralVillageMarket, 0.7f, true, 0.f);
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
    ClientInstance::Clear_PotatoBagArenaMapRenderGateBasis();

    GAME->Set_VolumetricLightShaftZones(VolumetricLightShaftZoneCB{});

    if (_editorPlayRuntimeHandle.IsValid())
    {
        GAME->Get_DelegateHub().OnEditorPlayRuntime.Remove(_editorPlayRuntimeHandle);
        _editorPlayRuntimeHandle.Reset();
    }

    CLIENT->Clear_GameplayUI();

    __super::Free();
}
