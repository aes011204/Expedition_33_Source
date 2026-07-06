#include "UIPreviewManager.h"
#include "PreviewCamera.h"
#include "PreviewPlayer.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "Camera.h"
#include "Spawn_Prefab.h"

static const char* Get_HeroPrefabName(HeroId heroId)
{
    switch (heroId)
    {
    case HeroId::Gustave:
        return Constants::Prefab::PP_Gustave;
    case HeroId::Lune:
        return Constants::Prefab::PP_Lune;
    case HeroId::Maelle:
        return Constants::Prefab::PP_Maelle;
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
    _pendingPreviewReveal = false;

    return S_OK;
}

void UIPreviewManager::Update(float timeDelta)
{
    if (!_isActive)
        return;

    if (_previewCamera)
        _previewCamera->Update(timeDelta);

    for (PreviewHeroEntry& entry : _previewHeroes)
    {
        if (entry.player == nullptr)
            continue;

        entry.player->Update_Preview(timeDelta, _shownHeroId != HeroId::END);
    }
}

void UIPreviewManager::Begin_Preview(PreviewShotPanel panel)
{
    _activePanel = panel;

    if (_isActive)
        return;

    if (FAILED(Ensure_PreviewCamera()) || FAILED(Ensure_PreviewCharacters()))
        return;

    for (PreviewHeroEntry& entry : _previewHeroes)
    {
        if (entry.player)
            entry.player->Set_Visible(true);
    }

    _previewCamera->Reset_ToStartPos();
    CLIENT->Request_Preview();

    _isActive = true;
    _pendingPreviewReveal = false;
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

    entry.player->Sync_FromRuntimeData();

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
        if (entry.player == nullptr)
            continue;

        entry.player->Update_GameMenuFacialPreview(0.f, false);
        entry.player->Set_Visible(false); // PreviewPlayer는 월드 입력/이동 컴포넌트가 없어서 숨겨서 재사용해도 안전하다.
    }

    CLIENT->Restore_PreviousCamera();

    if (_previewCamera)
        _previewCamera->Set_TargetTransform(nullptr);

    _isActive = false;
    _pendingPreviewReveal = false;
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

HRESULT UIPreviewManager::Prepare_PreviewResources()
{
    CHECK_FAILED(Ensure_PreviewCamera(), E_FAIL);
    CHECK_FAILED(Ensure_PreviewCharacters(), E_FAIL);

    for (PreviewHeroEntry& entry : _previewHeroes)
    {
        if (entry.player == nullptr)
            continue;

        CHECK_FAILED(entry.player->Sync_FromRuntimeData(), E_FAIL);
        CHECK_FAILED(entry.player->Prewarm_CostumeVariants(), E_FAIL);
        entry.player->Set_Visible(false);
    }

    _pendingPreviewReveal = true;
    return S_OK;
}

void UIPreviewManager::Sync_PreviewHeroRuntimeData(HeroId heroId)
{
    const int32 heroIndex = static_cast<int32>(heroId);
    if (heroIndex < 0 || heroIndex >= static_cast<int32>(_previewHeroes.size()))
        return;

    PreviewHeroEntry& entry = _previewHeroes[heroIndex];
    if (entry.player == nullptr)
        return;

    entry.player->Sync_FromRuntimeData();
}

void UIPreviewManager::Play_PreviewAnimation(HeroId heroId, PreviewAnimState introAnim, PreviewAnimState idleAnim, float blendDuration)
{
    const int32 heroIndex = static_cast<int32>(heroId);
    if (heroIndex < 0 || heroIndex >= static_cast<int32>(_previewHeroes.size()))
        return;

    PreviewHeroEntry& entry = _previewHeroes[heroIndex];
    if (entry.player == nullptr)
        return;

    entry.player->Play_PreviewAnimation(introAnim, idleAnim, blendDuration);
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

    const uint32 count = min<uint32>(
        static_cast<uint32>(HeroId::END),
        static_cast<uint32>(partyEntries.size()));

    for (uint32 i = 0; i < count; ++i)
    {
        const HeroPartyEntry& partyEntry = partyEntries[i];
        const char* prefabName = Get_HeroPrefabName(partyEntry.heroId);

        if (prefabName == nullptr || prefabName[0] == '\0')
            continue;

        auto object = Spawn_Prefab::Prefab(prefabName)
            .AtLevel(ETOI(LevelType::Static))
            .InLayer(_previewLayerTag)
            .Spawn();

        auto previewPlayer = dynamic_pointer_cast<PreviewPlayer>(object);
        CHECK_NULL(previewPlayer, E_FAIL);

        previewPlayer->Set_Visible(false);
        CHECK_FAILED(previewPlayer->Sync_FromRuntimeData(), E_FAIL);

        PreviewHeroEntry previewEntry{};
        previewEntry.heroId = partyEntry.heroId;
        previewEntry.player = previewPlayer;

        _previewHeroes[static_cast<int32>(partyEntry.heroId)] = move(previewEntry);
    }

    return S_OK;
}

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

    Base::Free();
}
