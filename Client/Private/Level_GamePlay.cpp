#include "Level_GamePlay.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "Client_Constants.h"
#include "Spawn_Prefab.h"
#include "TargetCamera.h"
#include "MapLoader.h"
#include "Spawn_Prefab.h"
#include "UI_Base.h"
#include "UI_Button.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Transform.h"

Level_GamePlay::Level_GamePlay(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Level{ device, context }
{
}

HRESULT Level_GamePlay::Initialize()
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

    if (FAILED(Ready_Layer_Monster(TEXT("Layer_Monster"))))
        return E_FAIL;


    return S_OK;
}

void Level_GamePlay::BeginPlay()
{
    Level::BeginPlay();
}

void Level_GamePlay::Update(float fTimeDelta)
{
    //Update_Input_TestUI();

    // 테스트
#ifdef _DEBUG
    const int32 fps = (fTimeDelta > 0.f)
            ? static_cast<int32>(1.f / fTimeDelta) : 0;

     GAME->Print(L"현재 FPS : {}", fps, duration(0.f), color(Vec4(1.f, 1.f, 0.f, 1.f)));
#endif

     //테스트
     if (GAME->KeyDown(KEY_TYPE::KEY_0))
     {
         vector<HeroId> heroes =
         {
             HeroId::Lune,
             HeroId::Maelle,
             HeroId::Gustave,
         };

         vector<string> battleMonster;
         battleMonster.push_back(Constants::Prefab::MB_Lancelier); // 전투 몬스터 prefab 이름은 MB_* 상수로 통일한다.
         battleMonster.push_back(Constants::Prefab::MB_Lancelier); // 테스트 3마리 스폰도 같은 상수를 재사용한다.
         battleMonster.push_back(Constants::Prefab::MB_Lancelier); // 문자열 변경 누락으로 인한 스폰 실패를 막는다.

         CLIENT->OnEnterBattleField(1, true, heroes, battleMonster);
     }

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
    LIGHT_DESC LightDesc{};

    LightDesc.type = LightType::Directional;
    LightDesc.direction = XMFLOAT4(1.f, -1.f, -1.f, 0.f);
    LightDesc.diffuse = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
    LightDesc.ambient = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.f);
    LightDesc.specular = XMFLOAT4(1.f, 1.f, 1.f, 1.f);

    if (FAILED(GAME->Add_Light(LightDesc)))
        return E_FAIL;

    ShadowLightDesc ShadowDesc{};

    ShadowDesc.eye = Vec3(-0.f, 10.f, -7.f);
    ShadowDesc.at = Vec3(0.f, 0.f, 0.f);
    ShadowDesc.fovy = DirectX::XMConvertToRadians(120.f);
    ShadowDesc.aspect = 16.f / 9.f;
    ShadowDesc.zn = 0.1f;
    ShadowDesc.zf = 1000.f;
    ShadowDesc.bias = 0.03f;
    ShadowDesc.intensity = 0.4f;

    if (FAILED(GAME->Update_ShadowLight(ShadowDesc)))
        return E_FAIL;

    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_UI(const wstring& strLayerTag)
{
    /*UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = ETOI(LevelType::GamePlay);
    desc.jsonPath = Constants::UI::UI_Json_Path;
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);*/
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
    if (FAILED(
        GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Sky_GamePlay"),
            ETOI(LevelType::GamePlay), strLayerTag)))
                return E_FAIL;

    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Monster(const wstring& strLayerTag)
{
   /* if (FAILED(
        GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Monster"),
            ETOI(LevelType::GamePlay), strLayerTag)
    ))
        return E_FAIL;*/


    Vec3 startPos = Vec3(225.f, -4020.5f, -1.24f);


     
    const Shared<GameObject> monster = Client::Spawn_Prefab::Spawn(
        Constants::Prefab::MW_Lancelier, // 월드 몬스터 prefab 이름은 MW_* 상수로 통일한다.
        ETOI(LevelType::GamePlay),
        strLayerTag);

    CHECK_NULL(monster, E_FAIL);
    monster->Get_Transform()->Set_Position(startPos);
    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Player(const wstring& strLayerTag)
{
    Vec3 startPos = Vec3(205.f, -4020.5f, -1.24f);
    CHECK_FAILED(CLIENT->Initialize_Hero(ETOI(LevelType::GamePlay), strLayerTag, startPos), E_FAIL);

    return S_OK;
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
    CLIENT->Clear_GameplayUI();

    __super::Free();
}
