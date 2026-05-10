#include "Level_GamePlay.h"
#include "GameInstance.h"
#include "ClientInstance.h"
//#include "FreeCamera.h"
//#include "TargetCamera.h"
#include "PointParticlePreview.h"
#include "Spawn_Prefab.h"


//#include "MapLoader.h"
#include "TargetCamera.h"
#include "MapLoader.h"
#include "Spawn_Prefab.h"
#include "UI_Base.h"
#include "UI_Button.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Transform.h"

namespace
{
    constexpr int32 TEST_HP_STEP = 50;
    constexpr int32 TEST_EXP_STEP = 25;

    wstring Make_TestStatText(int32 value, int32 maxValue)
    {
        return to_wstring(value) + L"    /    " + to_wstring(maxValue);
    }

    Shared<UI_Base> Find_NamedBaseUI(const unordered_map<wstring, Shared<UI_Base>>& uiByName, const wstring& name)
    {
        const auto iter = uiByName.find(name);
        if (iter == uiByName.end() || nullptr == iter->second)
        {
            LOG_WARN("Test Missing Object: {}", String::ToString(name));
            return nullptr;
        }

        return iter->second;
    }

    template<typename T>
    Shared<T> Find_NamedUI(const unordered_map<wstring, Shared<UI_Base>>& uiByName, const wstring& name)
    {
        Shared<UI_Base> ui = Find_NamedBaseUI(uiByName, name);
        if (nullptr == ui)
            return nullptr;

        Shared<T> typedUI = dynamic_pointer_cast<T>(ui);
        if (nullptr == typedUI)
            LOG_WARN("Test Type Mismatch: {}", String::ToString(name));

        return typedUI;
    }
}

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
    Update_Input_TestUI();

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
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    return Bind_TestUI();
}

HRESULT Level_GamePlay::Bind_TestUI()
{
    _hpText = Find_NamedUI<UI_Text>(_uiByName, L"Lune_HP_Text");
    _expText = Find_NamedUI<UI_Text>(_uiByName, L"Lune_EXP_Text");
    _hpGauge = Find_NamedBaseUI(_uiByName, L"Lune_HP_Gauge");
    _expGauge = Find_NamedBaseUI(_uiByName, L"Lune_EXP_Gauge");
    _addHpButton = Find_NamedUI<UI_Button>(_uiByName, L"Add_HP");
    _removeHpButton = Find_NamedUI<UI_Button>(_uiByName, L"Remove_HP");
    _addExpButton = Find_NamedUI<UI_Button>(_uiByName, L"Add_EXP");
    _removeExpButton = Find_NamedUI<UI_Button>(_uiByName, L"Remove_EXP");

    if (nullptr != _hpGauge && nullptr != _hpGauge->Get_Transform())
        _hpGaugeOriginalSize = _hpGauge->Get_Transform()->Get_Size();
    else
        LOG_WARN("Failed HP Gauge");

    if (nullptr != _expGauge && nullptr != _expGauge->Get_Transform())
        _expGaugeOriginalSize = _expGauge->Get_Transform()->Get_Size();
    else
        LOG_WARN("Failed EXP Gauge");

    Update_TestUI();

    return S_OK;
}

void Level_GamePlay::Update_Input_TestUI()
{
    bool needsRefresh = false;

    if (nullptr != _addHpButton && _addHpButton->Was_Clicked())
    {
        _testCurrentHp = std::clamp(_testCurrentHp + TEST_HP_STEP, 0, _testMaxHp);
        needsRefresh = true;
    }

    if (nullptr != _removeHpButton && _removeHpButton->Was_Clicked())
    {
        _testCurrentHp = std::clamp(_testCurrentHp - TEST_HP_STEP, 0, _testMaxHp);
        needsRefresh = true;
    }

    if (nullptr != _addExpButton && _addExpButton->Was_Clicked())
    {
        _testCurrentExp = std::clamp(_testCurrentExp + TEST_EXP_STEP, 0, _testMaxExp);
        needsRefresh = true;
    }

    if (nullptr != _removeExpButton && _removeExpButton->Was_Clicked())
    {
        _testCurrentExp = std::clamp(_testCurrentExp - TEST_EXP_STEP, 0, _testMaxExp);
        needsRefresh = true;
    }

    if (needsRefresh)
        Update_TestUI();
}

void Level_GamePlay::Update_TestUI()
{
    if (nullptr != _hpText)
        _hpText->Set_Text(Make_TestStatText(_testCurrentHp, _testMaxHp));

    if (nullptr != _expText)
        _expText->Set_Text(Make_TestStatText(_testCurrentExp, _testMaxExp));

    ReSize_TestGauge(_hpGauge, _hpGaugeOriginalSize, _testCurrentHp, _testMaxHp);
    ReSize_TestGauge(_expGauge, _expGaugeOriginalSize, _testCurrentExp, _testMaxExp);
}

void Level_GamePlay::ReSize_TestGauge(const Shared<UI_Base>& gauge, const Vec2& originalSize, int32 value, int32 maxValue)
{
    if (nullptr == gauge || nullptr == gauge->Get_Transform())
        return;

    const float ratio = maxValue > 0
        ? std::clamp(static_cast<float>(value) / static_cast<float>(maxValue), 0.f, 1.f)
        : 0.f;

    Vec2 gaugeSize = originalSize;
    gaugeSize.x = originalSize.x * ratio;
    gauge->Get_Transform()->Set_Size(gaugeSize);
}

HRESULT Level_GamePlay::Ready_Layer_Camera(const wstring& strLayerTag)
{
    TargetCamera::TARGETCAMERA_DESC		TargetCameraDesc{};

    TargetCameraDesc.speedPerSec = 10.f;
    TargetCameraDesc.degreePerSec = 180.f;
    TargetCameraDesc.eye = XMFLOAT4(0.f, 20.f, -7.f, 1.f);
    TargetCameraDesc.at = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
    TargetCameraDesc.fovy = XMConvertToRadians(60.f);
    TargetCameraDesc.pitch = 0.f;
    TargetCameraDesc.yaw = 0.f;
    TargetCameraDesc.nearPlane = 0.1f;
    TargetCameraDesc.farPlane = 500.f;
    TargetCameraDesc.mouseSensor = 0.06f;
    TargetCameraDesc.followSpeed = 8.f;
    TargetCameraDesc.offset = Vec3 {0.5f, 1.f, 0.f};
    TargetCameraDesc.distance = 1.8f;
    TargetCameraDesc.sideOffset = 0.f;
    TargetCameraDesc.heightOffset = 0.5f;
    TargetCameraDesc.aimOffset = 0.5f;
    TargetCameraDesc.pitchLookScale = 0.5;
    TargetCameraDesc.pitchMin = -70.f;
    TargetCameraDesc.pitchMax = 80.f;
    TargetCameraDesc.lookHeight = 0.8f;
    TargetCameraDesc.pivotHeight =0.3f;

    if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_TargetCamera_GamePlay"),
        ETOI(LevelType::GamePlay), strLayerTag, &TargetCameraDesc)))
        return E_FAIL;

	return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_BackGround(const wstring& strLayerTag)
{
    if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Terrain"),
        ETOI(LevelType::GamePlay), strLayerTag)))
        return E_FAIL;

    if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Sky_GamePlay"),
        ETOI(LevelType::GamePlay), strLayerTag)))
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

    Shared<GameObject> monster = Client::Spawn_Prefab::Spawn(
        "Monster123123",
        ETOI(LevelType::GamePlay),
        strLayerTag);

    CHECK_NULL(monster, E_FAIL);
    return S_OK;
    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Player(const wstring& strLayerTag)
{
    auto player = Spawn_Prefab::Prefab("TestPlayer")
        .AtLevel(ETOI(LevelType::GamePlay))
        .InLayer(strLayerTag)
        .Spawn();

    auto tr = player->Get_Transform();
    if (tr)
        tr->Set_Position(Vec3(10.f, 5.f, 10.f));

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
    _removeExpButton.reset();
    _addExpButton.reset();
    _removeHpButton.reset();
    _addHpButton.reset();
    _expGauge.reset();
    _hpGauge.reset();
    _expText.reset();
    _hpText.reset();
    _uiByName.clear();

    __super::Free();

}

