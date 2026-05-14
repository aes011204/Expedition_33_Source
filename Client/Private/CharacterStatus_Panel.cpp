#include "CharacterStatus_Panel.h"
#include "ClientInstance.h"
#include "CombatStatCom.h"
#include "GameInstance.h"
#include "Player.h"
#include "Spawn_Prefab.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "PreviewCamera.h"

HRESULT CharacterStatus_Panel::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();
    
    return S_OK;
}

void CharacterStatus_Panel::Update(float timeDelta)
{
    if (!_isOpen)
        return;

    Refresh_CombatStats();
    Update_StatusPreviewHeroes(timeDelta);
}

void CharacterStatus_Panel::Open()
{
    _isOpen = true;

    if (_root)
        _root->Open();

    Refresh_CombatStats();
    CHECK_FAILED(Spawn_StatusPreviewHeroes());
    CHECK_FAILED(Ready_StatusPreviewCamera());

    Activate_StatusPreviewCamera();

    // 마우스 잠금
    GAME->Unlock_Mouse();
    GAME->Set_GameplayPaused(true);
}

void CharacterStatus_Panel::Close()
{
    _isOpen = false;

    if (_root)
        _root->Close();

    Restore_PreviousCamera();
    Clear_StatusPreviewHeroes();

    GAME->Set_GameplayPaused(false);
    GAME->Lock_Mouse();
}

void CharacterStatus_Panel::Toggle()
{
    if (_isOpen)
        Close();
    else
        Open();
}

HRESULT CharacterStatus_Panel::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_CharacterStatus_Path;
    desc.prototypeTagPrefix = L"CharacterStatus";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_CharacterStatus");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT CharacterStatus_Panel::Bind_UI()
{
    _levelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentLevel");
    _statPointText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatPoint");

    _vitalityText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentVitality");
    _mightText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentMight");
    _agilityText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentAgility");
    _defenseStatText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentDefense");
    _luckText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentLuck");

    _hpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatHP");
    _attackText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatAttack");
    _speedText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatSpeed");
    _defenseValueText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatDefense");
    _criticalRateText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatCriticalPercent");


    return S_OK;
}

void CharacterStatus_Panel::Refresh_CombatStats()
{
    Shared<Player> player = CLIENT->Get_FrontHero();
    CHECK_NULL(player);

    Shared<CombatStatCom> combatStat = player->Get_Component<CombatStatCom>();
    CHECK_NULL(combatStat);

    const CombatStatCom::CombatStatInfo& statInfo = combatStat->Get_StatInfo();

    Set_UIntText(_levelText, statInfo.level);
    Set_UIntText(_statPointText, statInfo.statPoint);

    Set_IntText(_vitalityText, statInfo.vitality);
    Set_IntText(_mightText, statInfo.might);
    Set_IntText(_agilityText, statInfo.agility);
    Set_IntText(_defenseStatText, statInfo.defense);
    Set_IntText(_luckText, statInfo.luck);
    
    Set_HpText(_hpText, statInfo.hp, statInfo.maxHp);
    Set_IntText(_attackText, static_cast<int32>(statInfo.attack));
    Set_IntText(_speedText, static_cast<int32>(statInfo.speed));
    Set_IntText(_defenseValueText, static_cast<int32>(statInfo.defenseValue));
    Set_PercentText(_criticalRateText, statInfo.criticalRate);
}

Shared<UI_Base> CharacterStatus_Panel::Find_UI(const umap<wstring, Shared<UI_Base>>& uiByName, const wstring& name) const
{
    const auto iter = uiByName.find(name);
    if (iter == uiByName.end() || iter->second == nullptr)
        return nullptr;

    return iter->second;
}

void CharacterStatus_Panel::Set_IntText(const Shared<UI_Text>& text, int32 value)
{
    if (text)
        text->Set_Text(to_wstring(value));
}

void CharacterStatus_Panel::Set_UIntText(const Shared<UI_Text>& text, uint32 value)
{
    if (text)
        text->Set_Text(to_wstring(value));
}

void CharacterStatus_Panel::Set_PercentText(const Shared<UI_Text>& text, float value)
{
    if (text)
        text->Set_Text(to_wstring(static_cast<int32>(value)) + L"%");
}

void CharacterStatus_Panel::Set_HpText(const Shared<UI_Text>& text, float hp, float maxHp)
{
    if (text == nullptr)
        return;

    const int32 hpValue = static_cast<int32>(hp);
    const int32 maxHpValue = static_cast<int32>(maxHp);

    text->Set_Text(to_wstring(hpValue) + L" / " + to_wstring(maxHpValue));
}

HRESULT CharacterStatus_Panel::Spawn_StatusPreviewHeroes()
{
    // 기존에 있다면 삭제
    Clear_StatusPreviewHeroes();

    const vector<Shared<Player>> sourceHeroes = CLIENT->Get_HeroesInPartyOrder();
    if (sourceHeroes.empty())
        return S_OK;

    static const string fallbackPrefabs[] =
    {
        "Player_Lune",
        "Player_Maelle",
        "Player_Gustave",
    };

    const uint32 count = min<uint32>(3, static_cast<uint32>(sourceHeroes.size()));

    _previewHeroes.reserve(count);

    for (uint32 i = 0; i < count; ++i)
    {
        string prefabName = sourceHeroes[i] ? sourceHeroes[i]->Get_SourcePrefabName() : string{};

        if (prefabName.empty())
            prefabName = fallbackPrefabs[i];

        auto object = Spawn_Prefab::Prefab(prefabName)
            .AtLevel(_levelIndex)
            .InLayer(_previewLayerTag)
            .Spawn();

        auto previewPlayer = dynamic_pointer_cast<Player>(object);
        CHECK_NULL(previewPlayer, E_FAIL);

        _previewHeroes.push_back(StatusPreviewHero{ previewPlayer });
    }

    Layout_StatusPreviewHeroes();

    return S_OK;
}

void CharacterStatus_Panel::Clear_StatusPreviewHeroes()
{
    for (StatusPreviewHero& entry : _previewHeroes)
    {
        if (entry.player)
        {
            GAME->Remove_GameObject(_levelIndex, entry.player);
        }
    }

    _previewHeroes.clear();
}

void CharacterStatus_Panel::Update_StatusPreviewHeroes(float timeDelta)
{
    for (StatusPreviewHero& entry : _previewHeroes)
    {
        if (entry.player)
            entry.player->Update_StatusPreview(timeDelta);
    }
}

void CharacterStatus_Panel::Layout_StatusPreviewHeroes()
{
    // 프리뷰 플레이어 초기 위치 세팅
    static const Vec3 positions[] =
    {
        Vec3(-1.4f, 0.f, 0.f),
        Vec3(0.0f,  0.f, 0.f),
        Vec3(1.4f,  0.f, 0.f),
    };

    static const Vec3 rotations[] =
    {
        Vec3(0.f, 0.f, 0.f),
        Vec3(0.f, 0.f, 0.f),
        Vec3(0.f, 0.f, 0.f),
    };

    const uint32 count = min<uint32>(
        static_cast<uint32>(_previewHeroes.size()), 3);

	LOG_DEBUG("[CharacterStatus] Layout preview count: {}", count);

    for (uint32 i = 0; i < count; ++i)
    {
        Shared<Player> player = _previewHeroes[i].player;
        if (player == nullptr || player->Get_Transform() == nullptr)
            continue;

        player->Get_Transform()->Set_WorldPosition(positions[i]);
        player->Get_Transform()->Set_WorldRotationEuler(rotations[i]);

		const Vec3 appliedPos = player->Get_Transform()->Get_WorldPosition();
        const Vec3 appliedRot = player->Get_Transform()->Get_WorldRotationEuler();

        LOG_DEBUG(
            "[CharacterStatus] Preview[{}] name={} prefab={} pos=({}, {}, {}) rot=({}, {}, {})",
            i,
            String::ToString(player->Get_Name()),
            player->Get_SourcePrefabName(),
            appliedPos.x,
            appliedPos.y,
            appliedPos.z,
            appliedRot.x,
            appliedRot.y,
            appliedRot.z);
    }
}

HRESULT CharacterStatus_Panel::Ready_StatusPreviewCamera()
{
    if (_statusPreviewCamera)
        return S_OK;

    PreviewCamera::Preview_CameraDesc desc{};
    desc.eye = Vec4(0.f, 1.45f, 6.f, 1.f);
    desc.at = Vec4(0.f, 1.05f, 0.f, 1.f);
    desc.fovy = XMConvertToRadians(30.f);
    desc.nearPlane = 0.1f;
    desc.farPlane = 100.f;
    desc.mouseSensor = 0.f;
    desc.mouseSensitivity = 0.f;

    Shared<GameObject> cameraObject = nullptr;

    CHECK_FAILED(
        GAME->Add_GameObject(
            ETOI(LevelType::Static),
            TEXT("GameObject_PreviewCamera"),
            _levelIndex,
            _previewLayerTag,
            cameraObject,
            &desc),
        E_FAIL);

    _statusPreviewCamera = dynamic_pointer_cast<PreviewCamera>(cameraObject);
    CHECK_NULL(_statusPreviewCamera, E_FAIL);

    _statusPreviewCamera->Try_BeginPlay();

    return S_OK;
}

void CharacterStatus_Panel::Activate_StatusPreviewCamera()
{
    if (_statusPreviewCamera == nullptr)
        return;

    _previousCamera = GAME->Get_ActiveCamera();

    GAME->Change_ActiveCamera(_statusPreviewCamera);
    _statusPreviewCamera->Apply_View();
}

void CharacterStatus_Panel::Restore_PreviousCamera()
{
    // 원래 카메라로 돌아가기 -> Lerp 값이있다면, 여기서는 수치조절해서 바로 되게 해야할듯
    if (_previousCamera)
        GAME->Change_ActiveCamera(_previousCamera);

    _previousCamera.reset();
}

Unique<CharacterStatus_Panel> CharacterStatus_Panel::Create(uint32 levelIndex)
{
    auto instance = make_unique<CharacterStatus_Panel>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : CharacterStatus_Panel");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void CharacterStatus_Panel::Free()
{
    Restore_PreviousCamera();
    Clear_StatusPreviewHeroes();

    if (_root)
        _root->Close();

    _root.reset();

    _levelText.reset();
    _statPointText.reset();

    _vitalityText.reset();
    _mightText.reset();
    _agilityText.reset();
    _defenseStatText.reset();
    _luckText.reset();

    _hpText.reset();
    _attackText.reset();
    _speedText.reset();
    _defenseValueText.reset();
    _criticalRateText.reset();

    _uiByName.clear();
    _isOpen = false;

    GAME->Set_GameplayPaused(false);

    Base::Free();
}

// --- GamePlay에 있던 UI 버튼 클릭 처리
//
//namespace
//{
//constexpr int32 TEST_HP_STEP = 50;
//constexpr int32 TEST_EXP_STEP = 25;
//
//wstring Make_TestStatText(int32 value, int32 maxValue)
//{
//    return to_wstring(value) + L"    /    " + to_wstring(maxValue);
//}
//
//Shared<UI_Base> Find_NamedBaseUI(const unordered_map<wstring, Shared<UI_Base>>& uiByName, const wstring& name)
//{
//    const auto iter = uiByName.find(name);
//    if (iter == uiByName.end() || nullptr == iter->second)
//    {
//        LOG_WARN("Test Missing Object: {}", String::ToString(name));
//        return nullptr;
//    }
//
//    return iter->second;
//}
//
//template <typename T>
//Shared<T> Find_NamedUI(const unordered_map<wstring, Shared<UI_Base>>& uiByName, const wstring& name)
//{
//    const Shared<UI_Base> ui = Find_NamedBaseUI(uiByName, name);
//    if (nullptr == ui)
//        return nullptr;
//
//    Shared<T> typedUI = dynamic_pointer_cast<T>(ui);
//    if (nullptr == typedUI)
//        LOG_WARN("Test Type Mismatch: {}", String::ToString(name));
//
//    return typedUI;
//}
//}
//
//HRESULT Level_GamePlay::Bind_TestUI()
//{
//    _hpText = Find_NamedUI<UI_Text>(_uiByName, L"Lune_HP_Text");
//    _expText = Find_NamedUI<UI_Text>(_uiByName, L"Lune_EXP_Text");
//    _hpGauge = Find_NamedBaseUI(_uiByName, L"Lune_HP_Gauge");
//    _expGauge = Find_NamedBaseUI(_uiByName, L"Lune_EXP_Gauge");
//    _addHpButton = Find_NamedUI<UI_Button>(_uiByName, L"Add_HP");
//    _removeHpButton = Find_NamedUI<UI_Button>(_uiByName, L"Remove_HP");
//    _addExpButton = Find_NamedUI<UI_Button>(_uiByName, L"Add_EXP");
//    _removeExpButton = Find_NamedUI<UI_Button>(_uiByName, L"Remove_EXP");
//
//    if (nullptr != _hpGauge && nullptr != _hpGauge->Get_Transform())
//        _hpGaugeOriginalSize = _hpGauge->Get_Transform()->Get_Size();
//    else
//        LOG_WARN("Failed HP Gauge");
//
//    if (nullptr != _expGauge && nullptr != _expGauge->Get_Transform())
//        _expGaugeOriginalSize = _expGauge->Get_Transform()->Get_Size();
//    else
//        LOG_WARN("Failed EXP Gauge");
//
//    Update_TestUI();
//
//    return S_OK;
//}
//
//void Level_GamePlay::Update_Input_TestUI()
//{
//    bool needsRefresh = false;
//
//    if (nullptr != _addHpButton && _addHpButton->Was_Clicked())
//    {
//        _testCurrentHp = std::clamp(_testCurrentHp + TEST_HP_STEP, 0, _testMaxHp);
//        needsRefresh = true;
//    }
//
//    if (nullptr != _removeHpButton && _removeHpButton->Was_Clicked())
//    {
//        _testCurrentHp = std::clamp(_testCurrentHp - TEST_HP_STEP, 0, _testMaxHp);
//        needsRefresh = true;
//    }
//
//    if (nullptr != _addExpButton && _addExpButton->Was_Clicked())
//    {
//        _testCurrentExp = std::clamp(_testCurrentExp + TEST_EXP_STEP, 0, _testMaxExp);
//        needsRefresh = true;
//    }
//
//    if (nullptr != _removeExpButton && _removeExpButton->Was_Clicked())
//    {
//        _testCurrentExp = std::clamp(_testCurrentExp - TEST_EXP_STEP, 0, _testMaxExp);
//        needsRefresh = true;
//    }
//
//    if (needsRefresh)
//        Update_TestUI();
//}
//
//void Level_GamePlay::Update_TestUI()
//{
//    if (nullptr != _hpText)
//        _hpText->Set_Text(Make_TestStatText(_testCurrentHp, _testMaxHp));
//
//    if (nullptr != _expText)
//        _expText->Set_Text(Make_TestStatText(_testCurrentExp, _testMaxExp));
//
//    ReSize_TestGauge(_hpGauge, _hpGaugeOriginalSize, _testCurrentHp, _testMaxHp);
//    ReSize_TestGauge(_expGauge, _expGaugeOriginalSize, _testCurrentExp, _testMaxExp);
//}
