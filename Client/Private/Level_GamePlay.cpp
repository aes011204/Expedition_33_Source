#include "Level_GamePlay.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "Client_Constants.h"
#include "PlayableParts.h"
#include "Player_WorldBase.h"
#include "Spawn_Prefab.h"
#include "TargetCamera.h"

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


    return S_OK;
}

void Level_GamePlay::BeginPlay()
{
    StreamingLevel::BeginPlay();
}

void Level_GamePlay::Update(float fTimeDelta)
{
    //Update_MapStreaming(fTimeDelta);
     //테스트
     if (GAME->KeyDown(KEY_TYPE::KEY_0))
     {
         vector<HeroId> heroes =
         {
             HeroId::Lune,
             HeroId::Maelle,
             HeroId::Gustave,
         };

         vector<BattleMonsterSpawnDesc> battleMonster;
         battleMonster.push_back({ Constants::Prefab::MB_Lancelier });
         battleMonster.push_back({ Constants::Prefab::MB_Lancelier });
         battleMonster.push_back({ Constants::Prefab::MB_Lancelier });

         CLIENT->OnEnterBattleField(1, true, heroes, battleMonster);
     }

}

HRESULT Level_GamePlay::Render()
{
    Update_WindowTitleFps(_windowTitleFps, L"StreamingLevel GamePlay");

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
    LIGHT_DESC LightDesc{};

    LightDesc.type = LightType::Directional;
    LightDesc.direction = Vec4(-0.8, -0.6f, -1.f, 0.f);
    LightDesc.diffuse = Vec4(1.f, 1.f, 1.f, 1.f);
    LightDesc.ambient = Vec4(0.4f, 0.4f, 0.4f, 1.f);
    LightDesc.specular = Vec4(1.f, 1.f, 1.f, 1.f);
    LightDesc.castShadow = true;

    if (FAILED(GAME->Add_Light(LightDesc)))
        return E_FAIL;

    {
        ShadowLightDesc ShadowDesc{};

        ShadowDesc.eye = Vec3(0.f, 10.f, -7.f);
        ShadowDesc.at = Vec3(0.f, 0.f, 0.f);

        // Camera 따라다니게
        ShadowDesc.followCameraFrustum = true;
        ShadowDesc.focusNear = 0.1f;
        ShadowDesc.focusFar = 110.f;
        ShadowDesc.lightDistancePadding = 30.f;
        ShadowDesc.xyPadding = 2.f;
        ShadowDesc.zPadding = 40.f;

        ShadowDesc.shadowMapWidth = 4096;
        ShadowDesc.shadowMapHeight = 4096;

        ShadowDesc.cascadeSplitLambda = 0.8f;
        ShadowDesc.bias = 0.0002f;
        ShadowDesc.intensity = 0.45f;

        if (FAILED(GAME->Update_ShadowLight(ShadowDesc)))
            return E_FAIL;

        ShadowCascadePresetSet cascadePresets{};
        cascadePresets.close.focusNear = 0.1f;
        cascadePresets.close.focusFar = 28.f;
        cascadePresets.close.cascadeSplitLambda = 0.95f;
        cascadePresets.close.cascadeBlendDistance = 2.5f;

        cascadePresets.normal.focusNear = 0.1f;
        cascadePresets.normal.focusFar = 110.f;
        cascadePresets.normal.cascadeSplitLambda = 0.8f;
        cascadePresets.normal.cascadeBlendDistance = 4.f;

        cascadePresets.farRange.focusNear = 0.1f;
        cascadePresets.farRange.focusFar = 180.f;
        cascadePresets.farRange.cascadeSplitLambda = 0.55f;
        cascadePresets.farRange.cascadeBlendDistance = 6.f;

        if (FAILED(GAME->Set_ShadowCascadePresets(cascadePresets)))
            return E_FAIL;
        if (FAILED(GAME->Set_ShadowCascadeLevel(CascadeLevel::Normal)))
            return E_FAIL;
    }

    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_UI(const wstring& strLayerTag)
{
    //UI_RuntimeLoader::UI_LoadDesc desc{};
    //desc.levelIndex = ETOI(LevelType::GamePlay);
    //desc.jsonPath = Constants::UI::UI_Json_Path;
    //desc.outUIByName = &_uiByName;

    //CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);
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
    if (FAILED(
        GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Sky_GamePlay"),
            ETOI(LevelType::GamePlay), strLayerTag)))
                return E_FAIL;

    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Monster(const wstring& strLayerTag)
{
    Vec3 startPos = Vec3(-77.f, 19.1f, -117.94f);

    const Shared<GameObject> monster = Client::Spawn_Prefab::Spawn(
        Constants::Prefab::MW_Lancelier,
        ETOI(LevelType::GamePlay),
        strLayerTag);

    CHECK_NULL(monster, E_FAIL);
    monster->Get_Transform()->Set_Position(startPos);

    // 이제 여기서 스폰안하고, 에디터에서 스폰&저장 진행

    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Player(const wstring& strLayerTag)
{
    //Vec3 startPos = Vec3(-465.f, 50.f, 65.f);
    Vec3 startPos = Vec3(-87.f, 19.47f, -132.94f);
    CHECK_FAILED(CLIENT->Initialize_Hero(ETOI(LevelType::GamePlay), strLayerTag, startPos), E_FAIL);

    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Effect(const wstring& strLayerTag)
{
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
    //Free_MapStreaming();

    CLIENT->Clear_GameplayUI();

    __super::Free();
}
