#include "Level_GamePlay.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "Client_Constants.h"
#include "EffectInstance.h"
#include "Spawn_Prefab.h"
#include "TargetCamera.h"
#include "MapLoader.h"
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

    if (FAILED(Ready_Layer_Effect(TEXT("Layer_Effect"))))
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
#ifdef _DEBUG
    const int32 fps = (fTimeDelta > 0.f)
            ? static_cast<int32>(1.f / fTimeDelta) : 0.f;

     GAME->Print(L"현재 FPS : {}", fps, duration(0.f), color(Vec4(1.f, 1.f, 0.f, 1.f)));
#else
    static float s_titleUpdateAccum = 0.f;   // 제목은 너무 자주 바꾸지 않도록 누적 시간 사용
    static float s_frameAccum = 0.f;         // 평균 FPS 계산용 프레임 시간 누적
    static int32 s_frameCount = 0;           // 평균 FPS 계산용 프레임 수

    s_titleUpdateAccum += fTimeDelta;
    s_frameAccum += fTimeDelta;
    ++s_frameCount;

    if (s_titleUpdateAccum >= 0.5f)          // 0.5초마다 제목 갱신
    {
        const float avgDelta = (s_frameCount > 0) ? (s_frameAccum / s_frameCount) : 0.f;
        const int32 fps = (avgDelta > 0.f) ? static_cast<int32>(1.f / avgDelta) : 0;

        wchar_t title[128] = {};
        swprintf_s(title, L"Level GamePlay | FPS : %d", fps); // Release에서 창 제목에 FPS 표시
        SetWindowText(g_hWnd, title);

        s_titleUpdateAccum = 0.f;
        s_frameAccum = 0.f;
        s_frameCount = 0;
    }
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

         CLIENT->OnEnterBattleField(1, false, heroes, battleMonster);
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
    Vec3 startPos = Vec3(213.f, -4020.47f, -1.94f);
    CHECK_FAILED(CLIENT->Initialize_Hero(ETOI(LevelType::GamePlay), strLayerTag, startPos), E_FAIL);

    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Effect(const wstring& strLayerTag)
{
#pragma region Effect Test
    auto effectDefinition = make_shared<EffectDefinition>();
    effectDefinition->name = "Thunder_Fall_Lightning_Test";

    ComputeSpriteEmitterDesc emitterDesc{};
    emitterDesc.required.material.textureId = "Texture_Effect_LightningBook2x2";
    emitterDesc.required.material.tint = Vec4(0.50f, 0.50f, 1.00f, 1.00f); // 파란색
    emitterDesc.required.material.intensity = 2.f;
    emitterDesc.required.material.opacityPower = 0.f;
    emitterDesc.required.material.noiseStrength = 0.2f;
    emitterDesc.required.material.blendMode = "Additive";
    emitterDesc.required.material.opacitySource = "Red";
    emitterDesc.required.material.subUVRows = 2u;
    emitterDesc.required.material.subUVCols = 2u;
    emitterDesc.required.spriteRender.screenAlignment = EffectSpriteScreenAlignment::Velocity;
    emitterDesc.required.spriteRender.directionalAlignmentMode = EffectSpriteDirectionalAlignmentMode::TextureAxis;
    emitterDesc.required.spriteRender.spriteTextureAxis = EffectSpriteTextureAxis::Y;
    emitterDesc.required.playback.duration = 0.45f;
    emitterDesc.required.playback.loopCount = 0u;
    emitterDesc.required.drawLimit.maxDrawCount = 24u;
    emitterDesc.spawn.instanceCount = 24u;

    emitterDesc.spawn.particleSpawn.processSpawnRate = false;
    emitterDesc.spawn.particleSpawn.processBurstList = true;
    emitterDesc.spawn.particleSpawn.bursts = {
        PointParticleBurstDesc{ 0.00f, 6u },
        PointParticleBurstDesc{ 0.14f, 5u },
        PointParticleBurstDesc{ 0.28f, 4u }
    };
    emitterDesc.spawn.particleSpawn.maxActiveCount = 24u;

    emitterDesc.lifetime.lifeTime = Vec2(0.40f, 1.10f);
    emitterDesc.initialSize.sizeMin = Vec2(0.12f, 0.14f);
    emitterDesc.initialSize.sizeMax = Vec2(0.22f, 0.42f);

    emitterDesc.motion.enabled = true;
    emitterDesc.motion.initialRadialVelocityEnabled = true;
    emitterDesc.motion.initialRadialVelocityInWorldSpace = false;
    emitterDesc.motion.radialPivot = Vec3(0.f, 0.f, 0.f);
    emitterDesc.motion.radialSpeed = Vec2(-0.39f, -0.18f);
    emitterDesc.motion.velocityScaleByLife = Vec2(1.0f, 0.25f);

    emitterDesc.sphereLocation.enabled = true;
    emitterDesc.sphereLocation.offset = Vec3(0.f, 0.f, 0.f);
    emitterDesc.sphereLocation.radius = 0.37f;
    emitterDesc.sphereLocation.mode = PointParticleSphereLocationMode::Surface;

    emitterDesc.sizeByLife.enabled = true;
    emitterDesc.sizeByLife.multiplyXStart = 1.2f;
    emitterDesc.sizeByLife.multiplyXEnd = 1.0f;
    emitterDesc.sizeByLife.multiplyYStart = 1.2f;
    emitterDesc.sizeByLife.multiplyYEnd = 1.0f;

    emitterDesc.subUVFrameOverLife.enabled = true;
    emitterDesc.subUVFrameOverLife.playbackMode = SubUVFramePlaybackMode::FramesPerSecond;
    emitterDesc.subUVFrameOverLife.startFrame = 0u;
    emitterDesc.subUVFrameOverLife.endFrame = 3u;
    emitterDesc.subUVFrameOverLife.loop = true;
    emitterDesc.subUVFrameOverLife.framesPerSecond = 20.f;

    emitterDesc.initialColor.startColorMin = Vec4(1.f, 1.f, 1.f, 0.85f);
    emitterDesc.initialColor.startColorMax = Vec4(1.f, 1.f, 1.f, 1.00f);
    emitterDesc.colorOverLife.endColorMin = Vec4(0.60f, 0.80f, 0.95f, 0.08f);
    emitterDesc.colorOverLife.endColorMax = Vec4(0.85f, 0.90f, 1.00f, 0.08f);

    EffectEmitterDefinition emitterDefinition{};
    emitterDefinition.id = 1u;
    emitterDefinition.name = "lightning_0";
    emitterDefinition.kind = EffectEmitterKind::ComputeSprite;
    emitterDefinition.enabled = true;
    emitterDefinition.useLocalSpace = true;
    emitterDefinition.concreteDesc = emitterDesc;
    effectDefinition->emitters.push_back(emitterDefinition);

    EffectInstanceDesc effectDesc{};
    effectDesc.definition = effectDefinition;
    effectDesc.position = Vec3(83.5f, 5.14f, -0.24f);

    const Shared<EffectInstance> effectPrototype = EffectInstance::Create(_device, _context);
    CHECK_NULL(effectPrototype, E_FAIL);

    const Shared<GameObject> effectObject = effectPrototype->Clone(&effectDesc);
    CHECK_NULL(effectObject, E_FAIL);

    CHECK_FAILED(
        GAME->Add_GameObject(
            ETOI(LevelType::GamePlay),
            strLayerTag,
            effectObject),
        E_FAIL
    );
#pragma endregion

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
