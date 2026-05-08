#include "Level_GamePlay.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "FreeCamera.h"
#include "TargetCamera.h"
#include "PointParticlePreview.h"

//#include "MapLoader.h"

#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"

Level_GamePlay::Level_GamePlay(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Level{ device, context }
{
}

HRESULT Level_GamePlay::Initialize()
{
    if (FAILED(Ready_Lights()))
        return E_FAIL;

    if (FAILED(Ready_Layer_UI(TEXT("Layer_UI"))))
		return E_FAIL;

	if (FAILED(Ready_Layer_Camera(TEXT("Layer_Camera"))))
		return E_FAIL;

    if (FAILED(Ready_Layer_BackGround(TEXT("Layer_BackGround"))))
        return E_FAIL;

    if (FAILED(Ready_Layer_Player(TEXT("Layer_Player"))))
        return E_FAIL;

    if (FAILED(Ready_Layer_Monster(TEXT("Layer_Monster"))))
        return E_FAIL;

    //CHECK_FAILED(Ready_Layer_MapStaticMesh(L"Layer_MapStaticMesh"), E_FAIL);

    return S_OK;
}

void Level_GamePlay::BeginPlay()
{
    Level::BeginPlay();

    
}

void Level_GamePlay::Update(float fTimeDelta)
{
    // 테스트
#ifdef _DEBUG
    static float accumulatedTime = 0.f;
    accumulatedTime += fTimeDelta;

    const int fakeHP = 100 - static_cast<int>(accumulatedTime) % 100;
    const wchar_t* stateText = (static_cast<int>(accumulatedTime) % 2 == 0) ? L"Idle" : L"Run";

    GAME->Print(L"프린트 테스트", duration(0.f));
    GAME->Print(L"Player State : {}", stateText, duration(0.f));
    GAME->Print(L"Time : {:.2f}", accumulatedTime, duration(0.f), color(Vec4(1.f, 1.f, 0.f, 1.f)));
#endif
}

HRESULT Level_GamePlay::Render()
{
#ifdef _DEBUG
    SetWindowText(g_hWnd, TEXT("Level GamePlay"));


#endif
    return S_OK;
}

HRESULT Level_GamePlay::Ready_Lights()
{
    LIGHT_DESC			LightDesc{};

	LightDesc.type          = LightType::Directional;
	LightDesc.direction     = XMFLOAT4(1.f, -1.f, 1.f, 0.f);
	LightDesc.diffuse       = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
	LightDesc.ambient       = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.f);
	LightDesc.specular      = XMFLOAT4(1.f, 1.f, 1.f, 1.f);

    if (FAILED(GAME->Add_Light(LightDesc)))
        return E_FAIL;

    ShadowLightDesc     ShadowDesc{};

    ShadowDesc.eye          = Vec3(-0.f, 10.f, -7.f);
    ShadowDesc.at           = Vec3(0.f, 0.f, 0.f);
    ShadowDesc.fovy         = DirectX::XMConvertToRadians(120.f);
    ShadowDesc.aspect       = 16.f / 9.f;
    ShadowDesc.zn           = 0.1f;
    ShadowDesc.zf           = 1000.f;
    ShadowDesc.bias         = 0.03f;
    ShadowDesc.intensity    = 0.4f;

    if (FAILED(GAME->Update_ShadowLight(ShadowDesc)))
        return E_FAIL;

    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_UI(const wstring& strLayerTag)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = ETOI(LevelType::GamePlay);
    desc.jsonPath = Constants::UI::UI_Json_Path;

    return UI_RuntimeLoader::Load_Document(desc);
}

HRESULT Level_GamePlay::Ready_Layer_Camera(const wstring& strLayerTag)
{
	FreeCamera::FREECAMERA_DESC		CameraDesc{};

	CameraDesc.speedPerSec = 10.f;
	CameraDesc.degreePerSec = 180.f;
	CameraDesc.eye = XMFLOAT4(0.f, 10.f, -7.f, 1.f);
	CameraDesc.at = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
	CameraDesc.fovy = XMConvertToRadians(60.f) ;
	CameraDesc.nearPlane = 0.1f;
	CameraDesc.farPlane = 500.f;
	CameraDesc.mouseSensor = 0.05f;

	if (FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), TEXT("GameObject_FreeCamera"),
		ETOI(LevelType::Static), strLayerTag, &CameraDesc)))
		return E_FAIL;

    TargetCamera::TARGETCAMERA_DESC		TargetCameraDesc{};

    TargetCameraDesc.speedPerSec = 10.f;
    TargetCameraDesc.degreePerSec = 180.f;
    TargetCameraDesc.eye = XMFLOAT4(0.f, 20.f, -7.f, 1.f);
    TargetCameraDesc.at = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
    TargetCameraDesc.fovy = XMConvertToRadians(60.f);
  //  TargetCameraDesc.pitch = 0.f;
  //  TargetCameraDesc.yaw = 0.f;
    TargetCameraDesc.nearPlane = 0.1f;
    TargetCameraDesc.farPlane = 500.f;
    TargetCameraDesc.mouseSensor = 0.06f;
    TargetCameraDesc.followSpeed = 8.f;
    TargetCameraDesc.distance = 1.8f;
    TargetCameraDesc.sideOffset = 0.f;
    TargetCameraDesc.heightOffset = 0.5f;
    TargetCameraDesc.aimOffset = 0.5f;
    TargetCameraDesc.pitchLookScale = 0.5;
    TargetCameraDesc.pitchMin = -70.f;
    TargetCameraDesc.pitchMax = 80.f;
    TargetCameraDesc.lookHeight = 0.8f;
    TargetCameraDesc.pivotHeight =0.3f;
    TargetCameraDesc.mouseSensitivity = 0.5f;
    
    if (FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), TEXT("GameObject_TargetCamera"),
        ETOI(LevelType::Static), strLayerTag, &TargetCameraDesc)))
        return E_FAIL;

	return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_BackGround(const wstring& strLayerTag)
{
    if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Terrain"),
        ETOI(LevelType::GamePlay), strLayerTag)))
        return E_FAIL;

    if (FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), TEXT("GameObject_Sky"),
        ETOI(LevelType::Static), strLayerTag)))
        return E_FAIL;

    if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_PointParticlePreview"),
        ETOI(LevelType::GamePlay), strLayerTag)))
        return E_FAIL;



	return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Monster(const wstring& strLayerTag)
{
    if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Monster"),
        ETOI(LevelType::GamePlay), strLayerTag)))
        return E_FAIL;

    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Player(const wstring& strLayerTag)
{
    if (FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), TEXT("GameObject_Player"),
        ETOI(LevelType::Static), strLayerTag)))
        return E_FAIL;

    auto goPlayer = CLIENT->Find_Player(ETOI(LevelType::Static));
    if (goPlayer)
    {
        auto tr = goPlayer->Get_Transform();
        if (tr)
        {
            Vec3 pos(10.f, 5.f, 10.f);
            tr->Set_Position(pos);
        }
    }

    return S_OK;
}

//HRESULT Level_GamePlay::Ready_Layer_MapStaticMesh(const wstring& strLayerTag)
//{
//    CHECK_FAILED(MapLoader::Load(_device, _context, Constants::Map::SmallLevel_SimonArea_Remake_Path,
//        ETOI(LevelType::GamePlay), strLayerTag), E_FAIL);
//
//    return S_OK;
//}

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
    __super::Free();

}

