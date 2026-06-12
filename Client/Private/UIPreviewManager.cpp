#include "UIPreviewManager.h"
#include "PreviewCamera.h"
#include "Player_WorldBase.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "Camera.h"
#include "Spawn_Prefab.h"

static const char* Get_HeroPrefabName(HeroId heroId)
{
    switch (heroId)
    {
    case HeroId::Gustave:
        return Constants::Prefab::PW_Gustave; // 월드 프리팹 이름은 공용 상수로 맞춘다.
    case HeroId::Lune:
        return Constants::Prefab::PW_Lune; // UI 미리보기 스폰도 실제 prefab 파일명과 같은 값을 사용한다.
    case HeroId::Maelle:
        return Constants::Prefab::PW_Maelle; // 이름 변경 시 호출부 문자열이 흩어지지 않게 한다.
    default:
        return "";
    }
}


UIPreviewManager::UIPreviewManager()
{
}

UIPreviewManager::~UIPreviewManager()
{
}

HRESULT UIPreviewManager::Initialize()
{
    _isActive = false;

    return S_OK;
}

void UIPreviewManager::Update(float timeDelta)
{
    if(!_isActive)
        return;

    if(_previewCamera)
        _previewCamera->Update(timeDelta);

    if(!_previewHeroes.empty())
    {
        for (PreviewHeroEntry& entry : _previewHeroes)
        {
            if (entry.player)
            {
                entry.player->Update_StatusPreview(timeDelta);
                entry.player->Update_GameMenuFacialPreview(timeDelta, _shownHeroId != HeroId::END);
            }
        }
    }
}

void UIPreviewManager::Begin_Preview(PreviewShotPanel panel)
{
    _activePanel = panel;

    if (_isActive)
        return;

    if (FAILED(Ensure_PreviewCamera()) || FAILED(Ensure_PreviewCharacters()))
        return;

 //   _previousCamera = GAME->Get_ActiveCamera();

    for (PreviewHeroEntry& entry : _previewHeroes)
    {
        if (entry.player)
            entry.player->Set_Visible(true);
    }

    _previewCamera->Reset_ToStartPos();
    CLIENT->Request_Preview();

    _isActive = true;
}

void UIPreviewManager::Show_Preview(HeroId heroId, PreviewShotPanel panel)
{
    _activePanel = panel;
    _shownHeroId = heroId;

    if (!_isActive)
        Begin_Preview(panel);

    if (!_isActive || _previewCamera == nullptr)
        return;

    const int32 heroIndex = static_cast<int32>(heroId);
    if (heroIndex < 0 || heroIndex >= static_cast<int32>(_previewHeroes.size()))
        return;

    PreviewHeroEntry& entry = _previewHeroes[heroIndex];
    if (entry.player == nullptr || entry.player->Get_Transform() == nullptr)
        return;

    const auto preset = CLIENT->Get_Preset(heroId, panel);
    _previewCamera->Set_Preset(preset);
    _previewCamera->Set_TargetTransform(entry.player->Get_Transform());

}

void UIPreviewManager::Hide_Preview()
{
    if (!_isActive)
        return;

    for (PreviewHeroEntry& entry : _previewHeroes)
    {
        if (entry.player)
            entry.player->Update_GameMenuFacialPreview(0.f, false);
    }

    CLIENT->Restore_PreviousCamera();

    if (_previewCamera)
        _previewCamera->Set_TargetTransform(nullptr);

    Clear_StatusPreviewHeroes();

    _isActive = false;
    _shownHeroId = HeroId::END;
}

void UIPreviewManager::Apply_Layout(const vector<PreviewSlotDesc>& layout)
{
    if (_previewHeroes.empty())
        return;

    for (const PreviewSlotDesc& slot : layout)
    {
        const int32 heroIndex = static_cast<int32>(slot.heroId);
        if (heroIndex < 0 || heroIndex >= static_cast<int32>(_previewHeroes.size()))
            continue;

        PreviewHeroEntry& entry = _previewHeroes[heroIndex];
        if (entry.player == nullptr || entry.player->Get_Transform() == nullptr)
            continue;

        entry.player->Get_Transform()->Set_WorldPosition(slot.position);
        entry.player->Get_Transform()->Set_WorldRotationEuler(slot.rotation);
    }
}

HRESULT UIPreviewManager::Ensure_PreviewCamera()
{
    if (_previewCamera)
        return S_OK;

    return Spawn_PreviewCamera();
}

HRESULT UIPreviewManager::Ensure_PreviewCharacters()
{
    if (!_previewHeroes.empty())
        return S_OK;

    return Spawn_PreviewCharacter();
}

HRESULT UIPreviewManager::Spawn_PreviewCamera()
{
    if (!GAME->Has_Prototype(ETOI(LevelType::Static), TEXT("GameObject_PreviewCamera")))
        return E_FAIL;

    PreviewCamera::Preview_CameraDesc desc{};
    desc.eye = Vec4(0.f, 1.45f, 16.f, 1.f);
    desc.at = Vec4(0.f, 1.05f, 0.f, 1.f);
    desc.fovy = XMConvertToRadians(30.f);
   desc.nearPlane = 0.1f;
   desc.farPlane = 100.f;
   desc.lookOffset = Vec3(0.f, 2.f, 0.f);
   desc.distance = 5.f;
   desc.followSpeed = 3.f;
   
   // desc.eye = Vec4(0.f, 1.9f, 10.f, 1.f);
   // desc.at = Vec4(0.f, 1.3f, 0.f, 1.f);
   // desc.fovy = XMConvertToRadians(20.f);
   // desc.nearPlane = 0.1f;
   // desc.farPlane = 100.f;
   // desc.mouseSensor = 0.f;
   // desc.mouseSensitivity = 0.f;
   // desc.distance = 2.8f;
   // desc.followSpeed = 2.f;
   // desc.lookOffset = Vec3(0.f, 2.0f, 0.f);

    Shared<GameObject> cameraObject = nullptr;

    CHECK_FAILED(
        GAME->Add_GameObject(
            ETOI(LevelType::Static),
            TEXT("GameObject_PreviewCamera"),
            ETOI(LevelType::Static),
            TEXT("Layer_Camera"),
            cameraObject,
            &desc),
        E_FAIL);

    _previewCamera = dynamic_pointer_cast<PreviewCamera>(cameraObject);
    CHECK_NULL(_previewCamera, E_FAIL);

    _previewCamera->Try_BeginPlay();

    return S_OK;
}

HRESULT UIPreviewManager::Spawn_PreviewCharacter()
{
    const vector<HeroPartyEntry>& partyEntries = CLIENT->Get_HeroEntries();
    if (partyEntries.empty())
        return S_OK;

    _previewHeroes.clear();
    _previewHeroes.resize(static_cast<size_t>(HeroId::END));

    const uint32 count = min<uint32>(static_cast<uint32>(HeroId::END), static_cast<uint32>(partyEntries.size()));

    for (uint32 i = 0; i < count; ++i)
    {
        const HeroPartyEntry& entry = partyEntries[i];

        string prefabName = entry.hero ? entry.hero->Get_SourcePrefabName() : string{};

        if (prefabName.empty())
            prefabName = Get_HeroPrefabName(entry.heroId);

        auto object = Spawn_Prefab::Prefab(prefabName)
            .AtLevel(ETOI(LevelType::Static))
            .InLayer(_previewLayerTag)
            .Spawn();

        auto previewPlayer = dynamic_pointer_cast<Player_WorldBase>(object);
        CHECK_NULL(previewPlayer, E_FAIL);

        previewPlayer->Set_Visible(false);
        PreviewHeroEntry previewEntry{};
        previewEntry.heroId = entry.heroId;
        previewEntry.player = previewPlayer;
        _previewHeroes[static_cast<int32>(entry.heroId)] = move(previewEntry);
    }

    return S_OK;
}

//void UIPreviewManager::Restore_PreviousCamera()
//{
//    if (_previousCamera == nullptr)
//        return;
//
//    GAME->Change_ActiveCamera(_previousCamera);
//    _previousCamera.reset();
//}

void UIPreviewManager::Clear_StatusPreviewHeroes()
{
    for (PreviewHeroEntry& entry : _previewHeroes)
    {
        if (entry.player)
            GAME->Remove_GameObject(ETOI(LevelType::Static), entry.player);
    }

    _previewHeroes.clear();
}

Shared<UIPreviewManager> UIPreviewManager::Create()
{
    auto instance = make_shared<UIPreviewManager>();

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("Failed to Created : UIPreviewManager");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UIPreviewManager::Free()
{
    Hide_Preview();
    Clear_StatusPreviewHeroes();

    if (_previewCamera)
    {
        GAME->Remove_GameObject(ETOI(LevelType::Static), _previewCamera);
        _previewCamera.reset();
    }

  //  _previousCamera.reset();
    Base::Free();
}
