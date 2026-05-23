#include "Level_GamePlay.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "Client_Constants.h"
#include "EffectAssetRuntimeLoader.h"
#include "EffectInstance.h"
#include "PlayableParts.h"
#include "Player_WorldBase.h"
#include "Spawn_Prefab.h"
#include "TargetCamera.h"
#include "MapLoader.h"

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
    //Sync_LuneRightHandEffect();
    //Update_MapStreaming(fTimeDelta);
    Update_WindowTitleFps();

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
         battleMonster.push_back(Constants::Prefab::MB_Lancelier);
         battleMonster.push_back(Constants::Prefab::MB_Lancelier);
         battleMonster.push_back(Constants::Prefab::MB_Lancelier);

         CLIENT->OnEnterBattleField(1, true, heroes, battleMonster);
     }

}

HRESULT Level_GamePlay::Render()
{
    return S_OK;
}

void Level_GamePlay::Update_WindowTitleFps()
{
    const auto now = chrono::steady_clock::now();
    if (_fpsSampleBegin.time_since_epoch().count() == 0)
    {
        _fpsSampleBegin = now;
        _fpsFrameCount = 0;
    }

    ++_fpsFrameCount;

    const auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - _fpsSampleBegin);
    if (elapsed < chrono::milliseconds(500))
    {
        return;
    }

    _displayFps = static_cast<int32>(
        (_fpsFrameCount * 1000ull) / static_cast<uint64>(elapsed.count()));

    wchar_t title[128] = {};
    swprintf_s(title, L"StreamingLevel GamePlay | FPS : %d", _displayFps);
    SetWindowText(g_hWnd, title);

    _fpsSampleBegin = now;
    _fpsFrameCount = 0;
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
    LightDesc.direction = Vec4(1.f, -1.f, -1.f, 0.f);
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
        ShadowDesc.focusFar = 40.f;
        ShadowDesc.lightDistancePadding = 30.f;
        ShadowDesc.xyPadding = 2.f;
        ShadowDesc.zPadding = 40.f;

        ShadowDesc.shadowMapWidth = 4096;
        ShadowDesc.shadowMapHeight = 4096;

        ShadowDesc.bias = 0.0002f;
        ShadowDesc.intensity = 0.45f;

        if (FAILED(GAME->Update_ShadowLight(ShadowDesc)))
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
    //if (FAILED(
    //    GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Monster"),
    //        ETOI(LevelType::GamePlay), strLayerTag)
    //))
    //    return E_FAIL;


    Vec3 startPos = Vec3(180.f, -4020.47f, -1.94f);

    //Lancelier
    const Shared<GameObject> monster = Client::Spawn_Prefab::Spawn(
        Constants::Prefab::MW_Lancelier, // 월드 몬스터 prefab 이름은 MW_* 상수로 통일한다.
        ETOI(LevelType::GamePlay),
        strLayerTag);

    CHECK_NULL(monster, E_FAIL);
    monster->Get_Transform()->Set_Position(startPos);

    //Portier
    const Shared<GameObject> monsterPortier = Client::Spawn_Prefab::Spawn(
        Constants::Prefab::MW_Portier, // 월드 몬스터 prefab 이름은 MW_* 상수로 통일한다.
        ETOI(LevelType::GamePlay),
        strLayerTag);

    CHECK_NULL(monster, E_FAIL);
    monster->Get_Transform()->Set_Position(startPos + Vec3(0.f,0.f, 10.f));

    //Volester
    const Shared<GameObject> monsterVolester = Client::Spawn_Prefab::Spawn(
        Constants::Prefab::MW_Volester, // 월드 몬스터 prefab 이름은 MW_* 상수로 통일한다.
        ETOI(LevelType::GamePlay),
        strLayerTag);

    CHECK_NULL(monster, E_FAIL);
    monster->Get_Transform()->Set_Position(startPos + Vec3(0.f, 0.f, 20.f));

    //
    const Shared<GameObject> monsterBishop = Client::Spawn_Prefab::Spawn(
        Constants::Prefab::MW_Bishop, // 월드 몬스터 prefab 이름은 MW_* 상수로 통일한다.
        ETOI(LevelType::GamePlay),
        strLayerTag);

    CHECK_NULL(monster, E_FAIL);
    monster->Get_Transform()->Set_Position(startPos + Vec3(0.f, 0.f, 30.f));
    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Player(const wstring& strLayerTag)
{
    //Vec3 startPos = Vec3(-465.f, 50.f, 65.f);
    Vec3 startPos = Vec3(-85.f, 51.f, -74.f);
    CHECK_FAILED(CLIENT->Initialize_Hero(ETOI(LevelType::GamePlay), strLayerTag, startPos), E_FAIL);

    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Effect(const wstring& strLayerTag)
{
#pragma region Effect Test
    Shared<const EffectDefinition> effectDefinition =
        EffectAssetRuntimeLoader::Load_DefinitionByName(L"Lune_Thunder_Fall_Charge");
    CHECK_NULL(effectDefinition, E_FAIL);

    const Shared<GameObject> player = CLIENT->Find_Player(ETOI(LevelType::GamePlay));
    CHECK_NULL(player, E_FAIL);

    const Vec3 playerPosition = player->Get_Transform()->Get_WorldPosition();
    const Vec3 playerForward = player->Get_Transform()->Get_WorldForward();
    constexpr float effectForwardOffset = 5.f;

    EffectInstanceDesc effectDesc{};
    effectDesc.definition = effectDefinition;
    effectDesc.position = playerPosition + playerForward * effectForwardOffset;

    CHECK_FAILED(
        GAME->Add_GameObject(
            ETOI(LevelType::GamePlay),
            L"GameObject_EffectInstance",
            ETOI(LevelType::GamePlay),
            strLayerTag,
            &effectDesc),
        E_FAIL
    );

    effectDefinition = EffectAssetRuntimeLoader::Load_DefinitionByName(L"Lune_Thunder_Fall_Charge_Blue");
    CHECK_NULL(effectDefinition, E_FAIL);

    EffectInstanceDesc blueEffectDesc{};
    blueEffectDesc.definition = effectDefinition;
    blueEffectDesc.position = playerPosition;

    CHECK_FAILED(
        GAME->Add_GameObject(
            ETOI(LevelType::GamePlay),
            L"GameObject_EffectInstance",
            ETOI(LevelType::GamePlay),
            strLayerTag,
            _luneRightHandEffect,
            &blueEffectDesc),
        E_FAIL
    );
#pragma endregion

    return S_OK;
}

void Level_GamePlay::Sync_LuneRightHandEffect()
{
    CHECK_NULL(_luneRightHandEffect);

    const Shared<Player_WorldBase> lune = CLIENT->Find_Hero(HeroId::Lune);
    CHECK_NULL_ONCE(lune);

    PartObject* bodyPart = lune->Get_PartObject(L"Part_Body");
    CHECK_NULL_ONCE(bodyPart);

    const auto playableBody = dynamic_cast<PlayableParts*>(bodyPart);
    CHECK_NULL_ONCE(playableBody);

    static constexpr char rightHandSocketName[] = "RightHandFrontSocket";
    const Matrix* socketMatrix = playableBody->Get_SocketBoneMatrixPtr(rightHandSocketName);
    if (nullptr == socketMatrix)
    {
        LOG_WARN_ONCE("Lune right-hand socket '{}' not found.", rightHandSocketName);
        return;
    }

    const Shared<TransformCom> effectTransform = _luneRightHandEffect->Get_Transform();
    const Shared<TransformCom> bodyTransform = playableBody->Get_Transform();
    const Shared<TransformCom> luneTransform = lune->Get_Transform();
    if (nullptr == effectTransform || nullptr == bodyTransform || nullptr == luneTransform)
    {
        LOG_WARN_ONCE("Lune right-hand blue effect follow failed.");
        return;
    }

    const Matrix socketWorldMatrix =
        *socketMatrix * bodyTransform->Get_WorldMatrix() * luneTransform->Get_WorldMatrix();
    effectTransform->Set_WorldPosition(Vec3(socketWorldMatrix._41, socketWorldMatrix._42, socketWorldMatrix._43));
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
