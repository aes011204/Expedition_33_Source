#include "Level_GestralVillage.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "SkyBoxFlow.h"
#include "Spawn_Prefab.h"
#include "TargetCamera.h"
#include "SoulLikeController.h"

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

    if (FAILED(Ready_Layer_UI(TEXT("Layer_UI"))))
        return E_FAIL;

    if (FAILED(Ready_Layer_BackGround(TEXT("Layer_BackGround"))))
        return E_FAIL;

    if (FAILED(Ready_Layer_Camera(TEXT("Layer_Camera"))))
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
    //GAME->Play_BGM(Constants::SoundBGM::SpringBGM, 1.f, true, 0.f);

    return S_OK;
}

void Level_GestralVillage::Update(float fTimeDelta)
{

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
        ShadowDesc.zPadding = 20.f;

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
        cascadePresets.normal.focusFar = 80.f;
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
    CLIENT->Set_TargetCamera(targetCamera);

    return S_OK;
}

HRESULT Level_GestralVillage::Ready_Layer_BackGround(const wstring& strLayerTag)
{
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
    Vec3 startPos = Vec3(-9.f, -3.47f, 207.94f);
    CHECK_FAILED(CLIENT->Initialize_Hero(ETOI(LevelType::GestralVillage), strLayerTag, startPos), E_FAIL);

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
    {
        const Shared<GameObject> controllerObject = Spawn_Prefab::Prefab(Constants::Prefab::SoulLikeController)
            .AtLevel(ETOI(LevelType::GestralVillage))
            .InLayer(strLayerTag)
            .Spawn();

        CHECK_NULL(controllerObject, E_FAIL);

        const Shared<SoulLikeController> soulLikeController =
            dynamic_pointer_cast<SoulLikeController>(controllerObject);

        CHECK_NULL(soulLikeController, E_FAIL);
        CLIENT->Set_SoulLikeController(soulLikeController);
    }

    return S_OK;
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
    CLIENT->Clear_GameplayUI();

    __super::Free();
}
