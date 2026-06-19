#include "Level_GestralVillage.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "SkyBoxFlow.h"
#include "TargetCamera.h"

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
    LIGHT_DESC LightDesc{};
    LightDesc.type = LightType::Directional;
    LightDesc.direction = Vec4(1.f, -1.f, 1.f, 0.f);
    LightDesc.diffuse = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
    LightDesc.ambient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.f);
    LightDesc.specular = XMFLOAT4(1.f, 1.f, 1.f, 1.f);

    if (FAILED(GAME->Add_Light(LightDesc)))
    {
        return E_FAIL;
    }

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
