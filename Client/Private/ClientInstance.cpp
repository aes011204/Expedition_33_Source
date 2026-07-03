#include "pch.h"
#include "Client_Constants.h"
#include "ClientInstance.h"
#include "BattleController.h"
#include "BattleEffectPrewarmCollector.h"
#include "BT_Node_Bootstrap.h"
#include "ComputeSpriteEmitter.h"
#include "EffectCom.h"
#include "EffectEmitterRegistration.h"
#include "EffectInstancePool.h"
#include "GameDataTable.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "GameplayUI.h"
#include "Hero_Manager.h"
#include "Level_Loading.h"
#include "LevelTransitionFade.h"
#include "PersistentObjectManager.h"
#include "Player_WorldBase.h"
#include "ResourceLoader.h"
#include "RuntimeRuleStateService.h"
#include "ScreenFadeTransition.h"
#include "PreviewShotTable.h"
#include "UIPreviewManager.h"
#include "CameraDirector.h"
#include "MapLoader.h"
#include "ClientScreenFxManager.h"
#include "CinematicCamera.h"
#include "CinematicSequenceRunner.h"
#include "MovementCom.h"
#include "PlayerInventory.h"
#include "CostumeManager.h"
#include "DialogueDB.h"
#include "PlayerStatCom.h"
#include "SoulLikeController.h"
#include "TargetCamera.h"
#include "BattleTurnCamera.h"
#include "BattleEncounterProfile.h"
#include "CameraPresetIO.h"
#include "Unlock_Manager.h"
#include "DialogueDB.h"
#include "DialogueManager.h"
#include "DialogueCamera.h"
#include "BattleFollowupManager.h"
#include "Client_Constants.h"
#include "Monster_WorldBase.h"
#include "NPC_Base.h"
#include "ContainerObject.h"
#include "ModelCom.h"

IMPLEMENT_SINGLETON(ClientInstance)

namespace
{
    bool g_debugDisableDynamicShadowRenderPass = false;
    bool g_debugDisableMapInstancedMeshRender = false;
    bool g_debugDisableGrassInstancedRender = false;
    bool g_debugDisableMapStaticMeshRender = false;
    uint32 g_debugMapInstancedVisibleCountReadbackRequestId = 0;
    bool g_debugForceGestralPalisadeInstancedCull = false;
    bool g_debugDisableMapInstancedMeshShadow = false;
    bool g_debugUseMapInstancedMeshShadowLOD = false;
    bool g_debugDisableGrassInstancedShadow = false;
    bool g_debugDisableMapStaticMeshShadow = false;
    bool g_potatoBagArenaRenderGateReady = false;
    bool g_potatoBagArenaRenderGateDebugVisible = false;
    bool g_potatoBagArenaInstancedShadowOverrideActive = false;
    bool g_potatoBagArenaSavedUseInstancedShadowCache = false;
    Vec3 g_potatoBagArenaOrigin = Vec3::Zero;
    Vec3 g_potatoBagArenaRight = Vec3::Right;
    Vec3 g_potatoBagArenaForward = Vec3::Forward;
    constexpr Vec3 kPotatoBagArenaGateExtents = Vec3(36.f, 28.f, 58.f);

    Quat Build_PotatoBagArenaGateRotation()
    {
        Vec3 forward = g_potatoBagArenaForward;
        forward.y = 0.f;
        if (forward.LengthSquared() <= 0.0001f)
        {
            Vec3 right = g_potatoBagArenaRight;
            right.y = 0.f;
            if (right.LengthSquared() > 0.0001f)
            {
                right.Normalize();
                forward = Vec3(right.z, 0.f, -right.x);
            }
            else
            {
                forward = Vec3::Forward;
            }
        }
        else
        {
            forward.Normalize();
        }

        return Quat::CreateFromYawPitchRoll(atan2f(forward.x, forward.z), 0.f, 0.f);
    }

    BoundingOrientedBox Build_PotatoBagArenaGateBox()
    {
        return BoundingOrientedBox(
            g_potatoBagArenaOrigin,
            kPotatoBagArenaGateExtents,
            Build_PotatoBagArenaGateRotation());
    }

    wstring Normalize_EffectNameForPool(const wstring& effectName)
    {
        if (effectName.empty())
            return {};

        wstring resolvedName = effectName;
        if (!resolvedName.ends_with(L".effect.json"))
            resolvedName += L".effect.json";

        return resolvedName;
    }

    void Restore_PotatoBagArenaShadowOverrides()
    {
        if (!g_potatoBagArenaInstancedShadowOverrideActive)
            return;

        GAME->Set_UseInstancedShadowCache(g_potatoBagArenaSavedUseInstancedShadowCache);
        g_potatoBagArenaInstancedShadowOverrideActive = false;
    }

    int32 Resolve_InteractionPromptPriority(InteractionPromptType type)
    {
        switch (type)
        {
        case InteractionPromptType::Dialogue:
            return 300;

        case InteractionPromptType::Picking:
            return 200;

        case InteractionPromptType::Cross:
        case InteractionPromptType::Up:
        case InteractionPromptType::Down:
            return 100;

        default:
            return 0;
        }
    }

    bool Should_CaptureReplayWorldObject(const wstring& layerTag, const Shared<GameObject>& gameObject)
    {
        if (gameObject == nullptr)
            return false;

        const ObjectType objectType = gameObject->Get_ObjectType();
        if (objectType == ObjectType::FreeCamera ||
            objectType == ObjectType::TargetCamera ||
            objectType == ObjectType::Terrain ||
            objectType == ObjectType::Landscape ||
            objectType == ObjectType::Sky ||
            objectType == ObjectType::Water ||
            objectType == ObjectType::Foliage)
        {
            return false;
        }

        return layerTag != L"Layer_Player" &&
            layerTag != L"Layer_MapStaticMesh" &&
            layerTag != L"Layer_Camera" &&
            layerTag != L"Layer_UI" &&
            layerTag != L"Layer_Effect";
    }

    void Clear_EquippedPictosMissingFromInventory(
        const vector<HeroPartyEntry>& heroEntries,
        const Shared<PlayerInventory>& inventory)
    {
        if (inventory == nullptr)
            return;

        for (const HeroPartyEntry& entry : heroEntries)
        {
            const Shared<Player_WorldBase> hero = entry.hero;
            if (hero == nullptr)
                continue;

            const Shared<PlayerStatCom> statCom = hero->Get_Component<PlayerStatCom>();
            if (statCom == nullptr)
                continue;

            const auto equippedPictosIds = statCom->Get_EquippedPictosIds();
            for (size_t slotIndex = 0; slotIndex < equippedPictosIds.size(); ++slotIndex)
            {
                const string& pictosId = equippedPictosIds[slotIndex];
                if (pictosId.empty())
                    continue;

                if (!inventory->Is_PictosUnlocked(pictosId))
                    statCom->Set_EquippedPictos(slotIndex, "");
            }
        }
    }

    string Get_WeaponCharacterId(HeroId heroId)
    {
        switch (heroId)
        {
        case HeroId::Gustave:
            return "Gustave";
        case HeroId::Lune:
            return "Lune";
        case HeroId::Maelle:
            return "Maelle";
        default:
            return "";
        }
    }

    string Get_DefaultEquippedWeaponId(HeroId heroId)
    {
        switch (heroId)
        {
        case HeroId::Gustave:
            return "Lanceram";
        case HeroId::Lune:
            return "Colim";
        case HeroId::Maelle:
            return "Medalum";
        default:
            return "";
        }
    }
}

ClientInstance::ClientInstance()
{
}

ClientInstance::~ClientInstance() = default;

HRESULT ClientInstance::Initialize_Client(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    if (_isInitialized)
        return S_OK;

    _device = device;
    _context = context;

    // 프로토타입 등록
    if (FAILED(Ready_ResourceLoader()))
        return E_FAIL;

    // Effect emitter 등록
    Register_ClientEffectEmitters();

    // 게임 플레이 데이터
    if (FAILED(Ready_GameDataTables()))
        return E_FAIL;

    Client::Force_Register_ClientBTNodes();

    _persistentObjectManager = PersistentObjectManager::Create();
    CHECK_NULL(_persistentObjectManager, E_FAIL);

    _heroManager = Hero_Manager::Create();
    CHECK_NULL(_heroManager, E_FAIL);

    _battleController = BattleController::Create();
    CHECK_NULL(_battleController, E_FAIL);

    _previewShotTable = PreviewShotTable::Create();
    CHECK_NULL(_previewShotTable, E_FAIL);

    _previewManager = UIPreviewManager::Create();
    CHECK_NULL(_previewManager, E_FAIL);

    _cameraDirector = CameraDirector::Create();
    CHECK_NULL(_cameraDirector, E_FAIL);
    _effectInstancePool = EffectInstancePool::Create();
    CHECK_NULL(_effectInstancePool, E_FAIL);

    _screenFxManager = ClientScreenFxManager::Create();
    CHECK_NULL(_screenFxManager, E_FAIL);

    _screenFadeTransition = ScreenFadeTransition::Create();
    if (_screenFadeTransition == nullptr)
        LOG_WARN("ScreenFadeTransition is not available. Screen fade requests will run immediately.");

    _levelTransitionFade = LevelTransitionFade::Create(_device, _context);
    if (_levelTransitionFade == nullptr)
        LOG_WARN("LevelTransitionFade is not available. Level transition requests will fall back to immediate changes.");

    _cinematicSequenceRunner = CinematicSequenceRunner::Create();
    CHECK_NULL(_cinematicSequenceRunner, E_FAIL);

    _playerInventory = PlayerInventory::Create();
    CHECK_NULL(_playerInventory, E_FAIL);

    // 일반 픽토스 일단은 전부 다 등록. 나중에 몬스터 잡기? 상점? 등에서 뭐 구매하게 만들 수 있으려나
    //_playerInventory->Ensure_DefaultUnlockedPictos(Get_AllPictosIds());

    // 무기 세팅
    vector<PlayerInventory::WeaponInventoryEntry> defaultUnlockedWeapons{};
    const HeroId defaultWeaponHeroes[] = { HeroId::Gustave, HeroId::Lune, HeroId::Maelle };

    for (HeroId heroId : defaultWeaponHeroes)
    {
        const string characterId = Get_WeaponCharacterId(heroId);
        const string weaponIds[] =
        {
            Get_DefaultEquippedWeaponId(heroId),
            "Baguette",
        };

        for (const string& weaponId : weaponIds)
        {
            if (characterId.empty() || weaponId.empty())
                continue;

            const GameDataTable::WeaponDesc* weapon = Get_WeaponDesc(characterId, weaponId);

            PlayerInventory::WeaponInventoryEntry entry{};
            entry.characterId = weapon ? weapon->characterId : characterId;
            entry.weaponId = weapon ? weapon->weaponId : weaponId;
            entry.enhanceLevel = weapon ? weapon->level : 1;
            entry.enhanceExp = 0;
            defaultUnlockedWeapons.emplace_back(std::move(entry));
        }
    }

    _playerInventory->Ensure_DefaultUnlockedWeapons(defaultUnlockedWeapons);

    _costumeManager = CostumeManager::Create();
    CHECK_NULL(_costumeManager, E_FAIL);

    _unlockManager = Unlock_Manager::Create();
    CHECK_NULL(_unlockManager, E_FAIL);

    _dialogueDB = DialogueDB::Create();
    CHECK_NULL(_dialogueDB, E_FAIL);

    _dialogueDB->LoadFromFile(String::ToString(Client::Constants::Dialogue::Dialogue_Dir));

    _dialogueManager = DialogueManager::Create();
    CHECK_NULL(_dialogueManager, E_FAIL);

    _battleFollowupManager = BattleFollowupManager::Create();
    CHECK_NULL(_battleFollowupManager, E_FAIL);

    _isInitialized = true;

    return S_OK;
}

HRESULT ClientInstance::Initialize_Client_ForEffectEditor(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    if (_isInitialized)
        return S_OK;

    _device = device;
    _context = context;

    CHECK_FAILED(Ready_ResourceLoader(), E_FAIL);

    Register_ClientEffectEmitters();

    _effectInstancePool = EffectInstancePool::Create();
    CHECK_NULL(_effectInstancePool, E_FAIL);

    _screenFadeTransition = ScreenFadeTransition::Create();
    if (_screenFadeTransition == nullptr)
        LOG_WARN("ScreenFadeTransition is not available. Screen fade requests will run immediately.");

    _levelTransitionFade = LevelTransitionFade::Create(_device, _context);
    if (_levelTransitionFade == nullptr)
        LOG_WARN("LevelTransitionFade is not available. Level transition requests will fall back to immediate changes.");

    _isInitialized = true;

    return S_OK;
}

void ClientInstance::Update_Client(float timeDelta)
{
    Update_FadeTransitions(timeDelta);

    if (_heroManager)
        _heroManager->Update_HeroManager(timeDelta);

    Update_EffectPrewarmQueue();

    if (_battleController)
        _battleController->Update(timeDelta);

    Update_PotatoBagArenaMapInstancedShadowOverride();

    if (_gamePlayUI)
        _gamePlayUI->Update(timeDelta);

    if (_previewManager)
        _previewManager->Update(timeDelta);

    if (_effectInstancePool)
        _effectInstancePool->Release_Finished();

    if (_cinematicSequenceRunner)
        _cinematicSequenceRunner->Update(timeDelta, true);

    if (_dialogueManager)
    {
        if (Is_BattleActive() || Is_BattleEntering() || Is_SoulLikeBattleActive())
        {
            if (_dialogueManager->IsActive())
                _dialogueManager->EndDialogue(false);
        }
        else
        {
            _dialogueManager->Update();
        }
    }
}

void ClientInstance::Update_FadeTransitions(float timeDelta)
{
    if (_screenFadeTransition)
        _screenFadeTransition->Update(timeDelta);

    if (_levelTransitionFade)
        _levelTransitionFade->Update(timeDelta);
}

void ClientInstance::Update_UIPreviewOnly(float timeDelta)
{
    if (_previewManager)
        _previewManager->Update(timeDelta);
}

bool ClientInstance::Is_DebugDisableMapInstancedMeshRender()
{
    return g_debugDisableMapInstancedMeshRender;
}

bool ClientInstance::Is_DebugDisableDynamicShadowRenderPass()
{
    return g_debugDisableDynamicShadowRenderPass;
}

void ClientInstance::Set_DebugDisableDynamicShadowRenderPass(bool disabled)
{
    g_debugDisableDynamicShadowRenderPass = disabled;
    GAME->Set_ShadowEnabled(!disabled);
}

void ClientInstance::Set_DebugDisableMapInstancedMeshRender(bool disabled)
{
    g_debugDisableMapInstancedMeshRender = disabled;
}

bool ClientInstance::Is_DebugDisableGrassInstancedRender()
{
    return g_debugDisableGrassInstancedRender;
}

void ClientInstance::Set_DebugDisableGrassInstancedRender(bool disabled)
{
    g_debugDisableGrassInstancedRender = disabled;
}

bool ClientInstance::Is_DebugDisableMapStaticMeshRender()
{
    return g_debugDisableMapStaticMeshRender;
}

void ClientInstance::Set_DebugDisableMapStaticMeshRender(bool disabled)
{
    g_debugDisableMapStaticMeshRender = disabled;
}

void ClientInstance::Request_DebugMapInstancedVisibleCountReadback()
{
    ++g_debugMapInstancedVisibleCountReadbackRequestId;
    if (g_debugMapInstancedVisibleCountReadbackRequestId == 0)
        g_debugMapInstancedVisibleCountReadbackRequestId = 1;
}

uint32 ClientInstance::Get_DebugMapInstancedVisibleCountReadbackRequestId()
{
    return g_debugMapInstancedVisibleCountReadbackRequestId;
}

bool ClientInstance::Is_DebugForceGestralPalisadeInstancedCull()
{
    return g_debugForceGestralPalisadeInstancedCull;
}

void ClientInstance::Set_DebugForceGestralPalisadeInstancedCull(bool enabled)
{
    g_debugForceGestralPalisadeInstancedCull = enabled;
}

bool ClientInstance::Is_DebugDisableMapInstancedMeshShadow()
{
    return g_debugDisableMapInstancedMeshShadow;
}

void ClientInstance::Set_DebugDisableMapInstancedMeshShadow(bool disabled)
{
    g_debugDisableMapInstancedMeshShadow = disabled;
}

bool ClientInstance::Is_DebugUseMapInstancedMeshShadowLOD()
{
    return g_debugUseMapInstancedMeshShadowLOD;
}

void ClientInstance::Set_DebugUseMapInstancedMeshShadowLOD(bool enabled)
{
    g_debugUseMapInstancedMeshShadowLOD = enabled;
}

bool ClientInstance::Is_DebugDisableGrassInstancedShadow()
{
    return g_debugDisableGrassInstancedShadow;
}

void ClientInstance::Set_DebugDisableGrassInstancedShadow(bool disabled)
{
    g_debugDisableGrassInstancedShadow = disabled;
}

bool ClientInstance::Is_DebugDisableMapStaticMeshShadow()
{
    return g_debugDisableMapStaticMeshShadow;
}

void ClientInstance::Set_DebugDisableMapStaticMeshShadow(bool disabled)
{
    g_debugDisableMapStaticMeshShadow = disabled;
}

void ClientInstance::Set_PotatoBagArenaMapRenderGateBasis(const Vec3& origin, const Vec3& right, const Vec3& forward)
{
    g_potatoBagArenaOrigin = origin;
    g_potatoBagArenaRight = right;
    g_potatoBagArenaForward = forward;
    g_potatoBagArenaRenderGateReady = true;
}

void ClientInstance::Clear_PotatoBagArenaMapRenderGateBasis()
{
    Restore_PotatoBagArenaShadowOverrides();

    g_potatoBagArenaRenderGateReady = false;
    g_potatoBagArenaRenderGateDebugVisible = false;
}

bool ClientInstance::Is_PotatoBagArenaMapRenderGateActive()
{
    const Shared<ClientInstance>& instance = ClientInstance::GetInstance();

    return g_potatoBagArenaRenderGateReady &&
        GAME->Current_LevelIndex() == ETOI(LevelType::GestralVillage) &&
        instance != nullptr &&
        instance->Is_PotatoBagBattleActive();
}

bool ClientInstance::Should_RenderMapObjectInPotatoBagArena(const BoundingBox& worldBounds)
{
    if (!Is_PotatoBagArenaMapRenderGateActive())
        return true;

    return Build_PotatoBagArenaGateBox().Intersects(worldBounds);
}

bool ClientInstance::Should_RenderMapInstanceInPotatoBagArena(const BoundingSphere& worldSphere)
{
    if (!Is_PotatoBagArenaMapRenderGateActive())
        return true;

    return Build_PotatoBagArenaGateBox().Intersects(worldSphere);
}

void ClientInstance::Update_PotatoBagArenaMapInstancedShadowOverride()
{
    const bool shouldDisableInstancedShadow = Is_PotatoBagArenaMapRenderGateActive();

    if (shouldDisableInstancedShadow)
    {
        if (!g_potatoBagArenaInstancedShadowOverrideActive)
        {
            g_potatoBagArenaSavedUseInstancedShadowCache = GAME->Is_UseInstancedShadowCache();
            GAME->Set_UseInstancedShadowCache(false);
            g_potatoBagArenaInstancedShadowOverrideActive = true;
        }
        return;
    }

    Restore_PotatoBagArenaShadowOverrides();
}

void ClientInstance::Toggle_PotatoBagArenaMapRenderGateDebug()
{
    g_potatoBagArenaRenderGateDebugVisible = !g_potatoBagArenaRenderGateDebugVisible;
}

void ClientInstance::Draw_PotatoBagArenaMapRenderGateDebug()
{
    if (!g_potatoBagArenaRenderGateDebugVisible || !Is_PotatoBagArenaMapRenderGateActive())
        return;

    DebugBoxDesc desc{};
    desc.center = g_potatoBagArenaOrigin;
    desc.extents = kPotatoBagArenaGateExtents;
    desc.rotation = Build_PotatoBagArenaGateRotation();
    desc.style.color = Color(0.2f, 1.f, 0.35f, 1.f);
    desc.style.depthEnabled = false;

    GAME->Draw_DebugBox(desc);
}

HRESULT ClientInstance::Ready_StaticResources()
{
    CHECK_NULL(_resourceLoader, E_FAIL);

    if (FAILED(_resourceLoader->Load_LevelTables(LevelType::Static)))
        return E_FAIL;

    return S_OK;
}

HRESULT ClientInstance::Ready_LevelResources(LevelType levelType)
{
    CHECK_NULL(_resourceLoader, E_FAIL);

    if (FAILED(_resourceLoader->Load_LevelTables(levelType)))
        return E_FAIL;

    return S_OK;
}

Shared<ResourceLoader> ClientInstance::Get_ResourceLoader() const
{
    return _resourceLoader;
}

uint32 ClientInstance::Get_NextExp(uint32 level) const
{
    if (_gameDataTable == nullptr)
        return 0;

    return _gameDataTable->Get_NextExp(level);
}

GameDataTable::SkillTreeDesc* ClientInstance::Get_SkillTreeDesc(const string& skillName)
{
    return _gameDataTable->Get_SkillTreeDesc(skillName);
}

const umap<string, vector<string>>* ClientInstance::Get_SkillTreeRequirementData()
{
    return _gameDataTable->Get_SkillTreeRequirementData();
}

const GameDataTable::WeaponDesc* ClientInstance::Get_WeaponDesc(const string& characterId, const string& weaponId) const
{
    if (_gameDataTable == nullptr)
        return nullptr;

    return _gameDataTable->Get_WeaponDesc(characterId, weaponId);
}

const vector<GameDataTable::WeaponDesc>* ClientInstance::Get_WeaponDescs(const string& characterId) const
{
    if (_gameDataTable == nullptr)
        return nullptr;

    return _gameDataTable->Get_WeaponDescs(characterId);
}

const GameDataTable::WeaponUpgradeDesc* ClientInstance::Get_WeaponUpgradeDesc(
    const string& characterId,
    const string& weaponId,
    uint32 level) const
{
    if (_gameDataTable == nullptr)
        return nullptr;

    return _gameDataTable->Get_WeaponUpgradeDesc(characterId, weaponId, level);
}

const GameDataTable::WeaponUpgradeDesc* ClientInstance::Get_NextWeaponUpgradeDesc(
    const string& characterId,
    const string& weaponId,
    uint32 currentLevel) const
{
    if (_gameDataTable == nullptr)
        return nullptr;

    return _gameDataTable->Get_NextWeaponUpgradeDesc(characterId, weaponId, currentLevel);
}

uint32 ClientInstance::Resolve_WeaponEnhanceLevel(const string& characterId, const string& weaponId) const
{
    if (_playerInventory == nullptr)
        return 1;

    return _playerInventory->Get_WeaponEnhanceLevel(characterId, weaponId);
}

uint32 ClientInstance::Resolve_WeaponPower(const string& characterId, const string& weaponId) const
{
    const uint32 enhanceLevel = Resolve_WeaponEnhanceLevel(characterId, weaponId);
    if (const GameDataTable::WeaponUpgradeDesc* upgradeDesc = Get_WeaponUpgradeDesc(characterId, weaponId, enhanceLevel))
        return upgradeDesc->power;

    const GameDataTable::WeaponDesc* weaponDesc = Get_WeaponDesc(characterId, weaponId);
    return weaponDesc ? weaponDesc->power : 0;
}

const Skill_Data* ClientInstance::Get_Skill(const string& skillId)
{
    return _gameDataTable->Get_Skill(skillId);
}

const vector<string> ClientInstance::Get_AllSkillIds()
{
    return _gameDataTable->Get_AllSkillIds();
}

HRESULT ClientInstance::Reload_GameDataTables()
{
    if (_gameDataTable == nullptr)
        return Ready_GameDataTables();

    return _gameDataTable->Load_All();
}

HRESULT ClientInstance::Reload_CameraPresetData()
{
    if (auto targetCamera = dynamic_pointer_cast<TargetCamera>(GAME->Get_CameraByType(ObjectType::TargetCamera)))
    {
        TargetCameraPreset preset{};

        if (CameraPresetIO::LoadTargetPresetFromFile(TargetCameraMode::NORMAL, preset))
            targetCamera->Set_Preset(TargetCameraMode::NORMAL, preset);

        if (CameraPresetIO::LoadTargetPresetFromFile(TargetCameraMode::AIM, preset))
            targetCamera->Set_Preset(TargetCameraMode::AIM, preset);

        if (CameraPresetIO::LoadTargetPresetFromFile(TargetCameraMode::BATTLE_ENTER, preset))
            targetCamera->Set_Preset(TargetCameraMode::BATTLE_ENTER, preset);

        if (CameraPresetIO::LoadTargetPresetFromFile(TargetCameraMode::SOULLIKE, preset))
            targetCamera->Set_Preset(TargetCameraMode::SOULLIKE, preset);

        targetCamera->Change_Mode(targetCamera->Get_CurrentMode());
    }

    if (auto battleTurnCamera = dynamic_pointer_cast<BattleTurnCamera>(GAME->Get_CameraByType(ObjectType::BattleTurnCamera)))
        battleTurnCamera->Reload_BattleCameraAuthoringData();

    if (auto dialogueCamera = dynamic_pointer_cast<DialogueCamera>(GAME->Get_CameraByType(ObjectType::DialogueCamera)))
        dialogueCamera->Set_CameraPresetId(dialogueCamera->Get_CameraPresetId());

    return S_OK;
}

const Item_Data* ClientInstance::Get_Item(const string& itemId)
{
    if (!_gameDataTable)
        return nullptr;

    return _gameDataTable->Get_Item(itemId);
}

const Item_Data* ClientInstance::Get_Item(BattleItemType itemType)
{
    if (!_gameDataTable)
        return nullptr;

    return _gameDataTable->Get_Item(itemType);
}

bool ClientInstance::Consume_Item(BattleItemType itemType)
{
    if (_gameDataTable == nullptr)
        return false;

    return _gameDataTable->Consume_Item(itemType);
}

const GameDataTable::PictosDesc* ClientInstance::Get_PictosDesc(const string& pictosId) const
{
    if (_gameDataTable == nullptr)
        return nullptr;

    return _gameDataTable->Get_PictosDesc(pictosId);
}

const vector<GameDataTable::PictosDesc>& ClientInstance::Get_PictosDescs() const
{
    static const vector<GameDataTable::PictosDesc> empty{};
    if (_gameDataTable == nullptr)
        return empty;

    return _gameDataTable->Get_PictosDescs();
}

const vector<string> ClientInstance::Get_AllPictosIds() const
{
    if (_gameDataTable == nullptr)
        return {};

    return _gameDataTable->Get_AllPictosIds();
}

void ClientInstance::Apply_PreviewLayout(const vector<UIPreviewManager::PreviewSlotDesc>& layout)
{
    return _previewManager->Apply_Layout(layout);
}

void ClientInstance::Begin_Preview(PreviewShotPanel panel)
{
    return _previewManager->Begin_Preview(panel);
}

void ClientInstance::Show_Preview(HeroId heroId, PreviewShotPanel panel)
{
    return _previewManager->Show_Preview(heroId, panel);
}

void ClientInstance::Hide_Preview()
{
    if (_previewManager == nullptr)
    {
        return;
    }

    return _previewManager->Hide_Preview();
}

PreviewShotPreset ClientInstance::Get_Preset(HeroId hero, PreviewShotPanel panel)
{
    return _previewShotTable->Get(hero, panel);
}

HRESULT ClientInstance::Initialize_Hero(uint32 levelIndex, const wstring& layerTag, const Vec3& startPos, const Vec3& startRot)
{
    return _heroManager->Initialize_Hero(levelIndex, layerTag, startPos, startRot);
}

void ClientInstance::Update_HeroManager(float timeDelta)
{
    return _heroManager->Update_HeroManager(timeDelta);
}

Shared<Player_WorldBase> ClientInstance::Get_ActiveHero() const
{
    return _heroManager->Get_ActiveHero();
}

const vector<HeroPartyEntry>& ClientInstance::Get_HeroEntries() const
{
    return _heroManager->Get_HeroEntries();
}

Shared<Player_WorldBase> ClientInstance::Get_Hero(HeroPartySlot slot) const
{
    return _heroManager->Get_Hero(slot);
}

HeroId ClientInstance::Get_HeroId(HeroPartySlot slot) const
{
    return _heroManager->Get_HeroId(slot);
}

HeroPartySlot ClientInstance::Get_ActiveHeroSlot() const
{
    return _heroManager->Get_ActiveHeroSlot();
}

void ClientInstance::Set_ActiveHeroSlotForCheckpoint(HeroPartySlot slot)
{
    if (_heroManager == nullptr)
        return;

    _heroManager->Set_ActiveHeroSlotForCheckpoint(slot);
}

void ClientInstance::Set_DialogueFocusActive(bool active)
{
    if (_heroManager == nullptr)
        return;

    _heroManager->Set_DialogueFocusActive(active);
}

void ClientInstance::Set_TargetCamera(const Shared<TargetCamera>& camera)
{
    return _heroManager->Set_TargetCamera(camera);
}

void ClientInstance::Clear_Party()
{
    return _heroManager->Clear_Party();
}

Shared<Player_WorldBase> ClientInstance::Get_FrontHero() const
{
    return _heroManager->Get_FrontHero();
}

Shared<Player_WorldBase> ClientInstance::Find_Hero(HeroId heroId) const
{
    return _heroManager->Find_Hero(heroId);
}

const HeroSpawnDesc* ClientInstance::Find_HeroSpawnDesc(HeroId heroId)
{
    return _heroManager->Find_HeroSpawnDesc(heroId);
}

bool ClientInstance::Ensure_DefaultEquippedWeapon(HeroId heroId)
{
    const Shared<Player_WorldBase> hero = Find_Hero(heroId);
    const Shared<PlayerStatCom> statCom = hero ? hero->Get_Component<PlayerStatCom>() : nullptr;
    if (statCom == nullptr)
        return false;

    if (!statCom->Get_PlayerRuntimeStat().CurEquippedWeaponId.empty())
        return false;

    const string characterId = Get_WeaponCharacterId(heroId);
    const string weaponId = Get_DefaultEquippedWeaponId(heroId);
    if (characterId.empty() || weaponId.empty())
        return false;

    const auto inventory = Get_PlayerInventory();
    if (inventory && !inventory->Is_WeaponUnlocked(characterId, weaponId))
        return false;

    const GameDataTable::WeaponDesc* weaponDesc = Get_WeaponDesc(characterId, weaponId);
    if (weaponDesc == nullptr)
        return false;

    statCom->Set_EquippedWeapon(weaponDesc->weaponId, Resolve_WeaponPower(characterId, weaponDesc->weaponId));
    return true;
}

void ClientInstance::Reset_AllHeroesToIdle()
{
    return _heroManager->Reset_AllHeroesToIdle();
}

void ClientInstance::Restore_PartyFormationAfterEditorStop()
{
    return _heroManager->Restore_PartyFormationAfterEditorStop();
}

bool ClientInstance::Is_ActiveHero(const Shared<GameObject>& target) const
{
    return _heroManager->Is_ActiveHero(target);
}

void ClientInstance::Warp_PartyToFormation(const Vec3& anchorPos, const Vec3& anchorRot)
{
    return _heroManager->Warp_PartyToFormation(anchorPos, anchorRot);
}

HRESULT ClientInstance::Initialize_GameplayUI(uint32 levelIndex)
{
    Clear_GameplayUI();

    _gamePlayUI = GameplayUI::Create(levelIndex);
    CHECK_NULL(_gamePlayUI, E_FAIL);

    return S_OK;
}

void ClientInstance::Clear_GameplayUI()
{
    Clear_InteractionPrompt(nullptr);

    if (_gamePlayUI)
    {
        _gamePlayUI->Free();
        _gamePlayUI.reset();
    }

    GAME->Set_GameplayPaused(false);
}

void ClientInstance::Play_BattleEnterTransition()
{
    if (_gamePlayUI)
        return _gamePlayUI->Play_BattleEnterTransition();
}

void ClientInstance::Stop_BattleEnterTransition()
{
    if (_gamePlayUI)
        return _gamePlayUI->Stop_BattleEnterTransition();
}

bool ClientInstance::Is_BattleEnterTransitionPlaying() const
{
    return _gamePlayUI && _gamePlayUI->Is_BattleEnterTransitionPlaying();
}

void ClientInstance::Queue_LevelAnnounce(LevelType levelType)
{
    if (_gamePlayUI)
        _gamePlayUI->Queue_LevelAnnounce(levelType);
}

bool ClientInstance::Request_ScreenFadeTransition(const function<void()>& CoveredAction)
{
    if (_levelTransitionFade && _levelTransitionFade->Is_Playing())
        return false;

    if (_screenFadeTransition == nullptr)
    {
        if (CoveredAction)
            CoveredAction();
        return true;
    }

    return _screenFadeTransition->Request_Transition(CoveredAction);
}

bool ClientInstance::Request_LevelTransition(LevelType TargetLevel)
{
    if (_screenFadeTransition && _screenFadeTransition->Is_Playing())
        return false;

    if (_levelTransitionFade)
    {
        const bool requested = _levelTransitionFade->Request_LevelTransition(TargetLevel);
        if (requested)
            Clear_EffectPrewarmQueue();

        return requested;
    }

    Shared<Level_Loading> loadingLevel = Level_Loading::Create(_device.Get(), _context.Get(), TargetLevel);
    if (loadingLevel == nullptr)
        return false;

    Clear_EffectPrewarmQueue();
    return SUCCEEDED(GAME->Change_Level(ETOI(LevelType::Loading), loadingLevel));
}

bool ClientInstance::Request_LoadedLevelTransition(LevelType TargetLevel, Shared<Level> TargetLevelInstance)
{
    if (_screenFadeTransition && _screenFadeTransition->Is_Playing())
        return false;

    if (_levelTransitionFade)
    {
        const bool requested = _levelTransitionFade->Request_LoadedLevelTransition(TargetLevel, TargetLevelInstance);
        if (requested)
            Clear_EffectPrewarmQueue();

        return requested;
    }

    if (TargetLevelInstance == nullptr)
        return false;

    Clear_EffectPrewarmQueue();
    return SUCCEEDED(GAME->Change_Level(ETOI(TargetLevel), TargetLevelInstance));
}

bool ClientInstance::Request_CurrentLevelReplay()
{
    if (_screenFadeTransition && _screenFadeTransition->Is_Playing())
        return false;

    if (_levelTransitionFade && _levelTransitionFade->Is_Playing())
        return false;

    if (Is_BattleActive() || Is_BattleEntering() || Is_SoulLikeBattleActive())
        return false;

    return Request_ScreenFadeTransition(
        [this]()
        {
            Reset_CurrentLevelRuntimeState();
        });
}

void ClientInstance::Reset_CurrentLevelRuntimeState()
{
    if (_dialogueManager && _dialogueManager->IsActive())
        _dialogueManager->EndDialogue(false);

    vector<Shared<GameObject>> duplicatePlayers{};
    vector<Shared<NPC_Base>> replayNpcs{};

    GAME->Visit_CurrentLevelObjects(
        [this, &duplicatePlayers, &replayNpcs](const wstring&, const Shared<GameObject>& gameObject)
        {
            if (const Shared<NPC_Base> npc = dynamic_pointer_cast<NPC_Base>(gameObject))
            {
                replayNpcs.push_back(npc);
                return;
            }

            const Shared<Player_WorldBase> player =
                dynamic_pointer_cast<Player_WorldBase>(gameObject);
            if (player == nullptr)
                return;

            bool managed = false;
            if (_heroManager)
            {
                for (const HeroPartyEntry& entry : _heroManager->Get_HeroEntries())
                {
                    if (entry.hero == player)
                    {
                        managed = true;
                        break;
                    }
                }
            }

            if (!managed)
                duplicatePlayers.push_back(gameObject);
        });

    for (const Shared<GameObject>& player : duplicatePlayers)
        GAME->Remove_GameObject(GAME->Current_LevelIndex(), player);

    if (!duplicatePlayers.empty())
        GAME->Flush_RemovedGameObjects();

    if (_heroManager)
        _heroManager->Reset_RuntimeToStart();

    Restore_ReplayWorldObjectsFromSnapshot();

    switch (static_cast<LevelType>(GAME->Current_LevelIndex()))
    {
    case LevelType::GamePlay:
        RuntimeRuleStateService::Apply_ToCurrentRuntime(
            RuntimeRuleStateService::Build_GameplayStartUnlocks(), false);
        break;

    case LevelType::GestralVillage:
        RuntimeRuleStateService::Apply_ToCurrentRuntime(
            RuntimeRuleStateService::Build_GestralVillageStartUnlocks(), false);
        break;

    default:
        break;
    }

    for (const Shared<NPC_Base>& npc : replayNpcs)
    {
        if (npc == nullptr)
            continue;

        npc->Reset_DialogueVisualOffset();
        npc->Reset_ToDefaultAnimation();
        npc->Restore_AfterBattleExit();
    }

    GAME->Set_GameplayInputMode(GameplayInputMode::Normal);
    GAME->Set_PlayerInputBlocked(false);
    GAME->Lock_Mouse();
}

void ClientInstance::Capture_InitialWorldSnapshot()
{
    Clear_ReplayWorldSnapshot();
    Capture_ReplayWorldSnapshotIfNeeded();
    Capture_ReplayCameraSnapshot();
}

void ClientInstance::Capture_ReplayWorldSnapshotIfNeeded()
{
    const uint32 levelIndex = GAME->Current_LevelIndex();
    if (_replayWorldSnapshotLevelIndex != levelIndex)
    {
        _replayWorldSnapshotLevelIndex = levelIndex;
        _replayWorldSpawnObjects.clear();
        _replayWorldRuntimeAuthoringLoaded.clear();
        _replayWorldInventoryJson = json::object();
        _replayWorldUnlockManagerJson = json::object();
        _replayWorldSnapshotCaptured = false;
    }

    if (_replayWorldSnapshotCaptured)
        return;

    if (_playerInventory)
        _replayWorldInventoryJson = _playerInventory->Create_SaveJson();

    if (_unlockManager)
        _replayWorldUnlockManagerJson = _unlockManager->Create_SaveJson();

    GAME->Visit_CurrentLevelObjects(
        [this](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            if (!Should_CaptureReplayWorldObject(layerTag, gameObject))
                return;

            RuntimeCheckpointIO::WorldObject object{};
            object.layerTag = layerTag;
            object.objectJson = gameObject->To_Json();
            _replayWorldSpawnObjects.emplace_back(std::move(object));
            _replayWorldRuntimeAuthoringLoaded.emplace_back(gameObject->Is_RuntimeAuthoringLoaded() ? 1 : 0);
        });

    _replayWorldSnapshotCaptured = true;
}

void ClientInstance::Capture_ReplayCameraSnapshot()
{
    _replayCameraSnapshot = {};

    if (const Shared<Camera> activeCamera = GAME->Get_ActiveCamera())
        _replayCameraSnapshot.activeType = activeCamera->Get_ObjectType();

    const auto captureCameraTransform =
        [](ObjectType cameraType, ReplayCameraTransformState& outState)
        {
            outState = {};

            const Shared<Camera> camera = GAME->Get_CameraByType(cameraType);
            const Shared<TransformCom> transform = camera ? camera->Get_Transform() : nullptr;
            if (camera == nullptr || transform == nullptr)
                return;

            outState.hasCamera = true;
            outState.position = transform->Get_WorldPosition();
            outState.rotation = transform->Get_WorldRotationEuler();
            outState.yaw = camera->Get_Yaw();
            outState.pitch = camera->Get_Pitch();
            outState.fovY = camera->Get_FovY();
            outState.nearPlane = camera->Get_Near();
            outState.farPlane = camera->Get_Far();
        };

    captureCameraTransform(ObjectType::TargetCamera, _replayCameraSnapshot.targetCamera);
    captureCameraTransform(ObjectType::FreeCamera, _replayCameraSnapshot.freeCamera);

    if (const Shared<TargetCamera> targetCamera =
        dynamic_pointer_cast<TargetCamera>(GAME->Get_CameraByType(ObjectType::TargetCamera)))
    {
        _replayCameraSnapshot.targetMode = targetCamera->Get_CurrentMode();
    }

    _replayCameraSnapshot.captured = true;
}

void ClientInstance::Restore_ReplayWorldObjectsFromSnapshot()
{
    Capture_ReplayWorldSnapshotIfNeeded();

    if (_playerInventory && _replayWorldInventoryJson.is_object())
        _playerInventory->Apply_SaveJson(_replayWorldInventoryJson);

    if (_unlockManager && _replayWorldUnlockManagerJson.is_object())
        _unlockManager->Apply_SaveJson(_replayWorldUnlockManagerJson, false, false);

    Clear_EquippedPictosMissingFromInventory(Get_HeroEntries(), _playerInventory);

    GAME->Flush_RemovedGameObjects();

    const uint32 levelIndex = GAME->Current_LevelIndex();
    struct CurrentReplayObject
    {
        wstring layerTag{};
        Shared<GameObject> gameObject{};
        bool consumed = false;
    };

    vector<CurrentReplayObject> currentObjects{};

    GAME->Visit_CurrentLevelObjects(
        [&currentObjects](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            if (Should_CaptureReplayWorldObject(layerTag, gameObject))
                currentObjects.push_back(CurrentReplayObject{ layerTag, gameObject, false });
        });

    const auto matchesSnapshotObject =
        [](const CurrentReplayObject& currentObject, const RuntimeCheckpointIO::WorldObject& snapshotObject)
        {
            if (currentObject.gameObject == nullptr || currentObject.layerTag != snapshotObject.layerTag)
                return false;

            const json& snapshotJson = snapshotObject.objectJson;
            const string snapshotClass = snapshotJson.value("static_class", string{});
            const string currentClass = currentObject.gameObject->To_Json().value("static_class", string{});
            if (snapshotClass.empty() || currentClass != snapshotClass)
                return false;

            if (snapshotJson.contains("__runtime_authoring_source") &&
                snapshotJson["__runtime_authoring_source"].is_object())
            {
                if (!currentObject.gameObject->Has_RuntimeAuthoringSource())
                    return false;

                const json& source = snapshotJson["__runtime_authoring_source"];
                return String::ToString(currentObject.gameObject->Get_RuntimeAuthoringMapPath()) ==
                    source.value("map_path", string{}) &&
                    currentObject.gameObject->Get_RuntimeAuthoringCollectionKey() ==
                    source.value("collection_key", string{}) &&
                    currentObject.gameObject->Get_RuntimeAuthoringNodeIndex() ==
                    source.value("node_index", size_t{});
            }

            const string snapshotPrefab = snapshotJson.value("source_prefab", string{});
            if (!snapshotPrefab.empty())
                return currentObject.gameObject->Get_SourcePrefabName() == snapshotPrefab;

            return true;
        };

    const auto restoreObjectState =
        [](const Shared<GameObject>& gameObject, const json& objectJson, bool runtimeAuthoringLoaded)
        {
            if (gameObject == nullptr)
                return;

            gameObject->From_Json(objectJson);
            gameObject->Set_RuntimeAuthoringLoaded(runtimeAuthoringLoaded);
            gameObject->Set_MarkDestroy(false);
            gameObject->Set_Visible(true);

            if (Shared<MovementCom> movement = gameObject->Get_Component<MovementCom>())
            {
                movement->Sync_RuntimePositionToTransform();
                movement->Clear_FollowMoveCommand();
                movement->Apply_Input({});
                movement->Stop_AllMovement();
            }

            if (const Shared<Monster_WorldBase> worldMonster = dynamic_pointer_cast<Monster_WorldBase>(gameObject))
                worldMonster->Restore_AfterBattleExit();
        };

    for (size_t index = 0; index < _replayWorldSpawnObjects.size(); ++index)
    {
        const RuntimeCheckpointIO::WorldObject& object = _replayWorldSpawnObjects[index];
        const bool runtimeAuthoringLoaded =
            index < _replayWorldRuntimeAuthoringLoaded.size() &&
            _replayWorldRuntimeAuthoringLoaded[index] != 0;

        Shared<GameObject> matchedObject{};
        for (CurrentReplayObject& currentObject : currentObjects)
        {
            if (currentObject.consumed || !matchesSnapshotObject(currentObject, object))
                continue;

            currentObject.consumed = true;
            matchedObject = currentObject.gameObject;
            break;
        }

        if (matchedObject != nullptr)
        {
            restoreObjectState(matchedObject, object.objectJson, runtimeAuthoringLoaded);
            continue;
        }

        Shared<GameObject> gameObject = GAME->Instantiate_GameObjectJson(object.objectJson);
        if (gameObject == nullptr)
            continue;

        restoreObjectState(gameObject, object.objectJson, runtimeAuthoringLoaded);
        (void)GAME->Add_GameObject(levelIndex, object.layerTag, gameObject);
    }

    Restore_ReplayCameraSnapshot();
}

void ClientInstance::Restore_ReplayCameraSnapshot()
{
    if (!_replayCameraSnapshot.captured)
        return;

    const auto restoreCameraTransform =
        [](ObjectType cameraType, const ReplayCameraTransformState& state)
        {
            if (!state.hasCamera)
                return;

            const Shared<Camera> camera = GAME->Get_CameraByType(cameraType);
            const Shared<TransformCom> transform = camera ? camera->Get_Transform() : nullptr;
            if (camera == nullptr || transform == nullptr)
                return;

            transform->Set_WorldPosition(state.position);
            transform->Set_WorldRotationEuler(state.rotation);
            transform->Update_WorldMatrix();
            camera->Set_Yaw(state.yaw);
            camera->Set_Pitch(state.pitch);
            camera->Set_FovY(state.fovY);
            camera->Set_Near(state.nearPlane);
            camera->Set_Far(state.farPlane);
        };

    restoreCameraTransform(ObjectType::FreeCamera, _replayCameraSnapshot.freeCamera);

    if (const Shared<TargetCamera> targetCamera =
        dynamic_pointer_cast<TargetCamera>(GAME->Get_CameraByType(ObjectType::TargetCamera)))
    {
        targetCamera->Change_Mode(_replayCameraSnapshot.targetMode);
        restoreCameraTransform(ObjectType::TargetCamera, _replayCameraSnapshot.targetCamera);
        if (_replayCameraSnapshot.targetCamera.hasCamera)
            targetCamera->Set_OrbitAngles(_replayCameraSnapshot.targetCamera.yaw, _replayCameraSnapshot.targetCamera.pitch);
        targetCamera->Refresh_TargetTransform();
        targetCamera->Reset_RuntimeCameraState();
    }

    if (_replayCameraSnapshot.activeType == ObjectType::TargetCamera ||
        _replayCameraSnapshot.activeType == ObjectType::FreeCamera)
    {
        GAME->Activate_CameraByType(_replayCameraSnapshot.activeType);
    }
}

void ClientInstance::Clear_ReplayWorldSnapshot()
{
    _replayWorldSpawnObjects.clear();
    _replayWorldRuntimeAuthoringLoaded.clear();
    _replayWorldInventoryJson = json::object();
    _replayWorldUnlockManagerJson = json::object();
    _replayWorldSnapshotLevelIndex = UINT32_MAX;
    _replayWorldSnapshotCaptured = false;
    _replayCameraSnapshot = {};
}

bool ClientInstance::Is_FadeTransitionPlaying() const
{
    const bool isScreenFadePlaying = _screenFadeTransition && _screenFadeTransition->Is_Playing();
    const bool isLevelFadePlaying = _levelTransitionFade && _levelTransitionFade->Is_Playing();

    return isScreenFadePlaying || isLevelFadePlaying;
}

void ClientInstance::OnEnterBattleField(uint32 FieldID, bool isPlayerAdvantage, const vector<HeroId>& heroIds,
    const vector<BattleMonsterSpawnDesc>& battleMonsterSpawnDescs, const Shared<GameObject>& sourceObject,
    bool completeImmediately, int32 battleStartSpawnerId, const Vec3& battleStartOffset)
{
    if (_dialogueManager && _dialogueManager->IsActive())
        _dialogueManager->EndDialogue(false);

    if (Should_ForcePlayerAdvantage(FieldID))
        isPlayerAdvantage = true;

    return _battleController->OnEnterBattleField(
        FieldID,
        isPlayerAdvantage,
        heroIds,
        battleMonsterSpawnDescs,
        sourceObject,
        completeImmediately,
        battleStartSpawnerId,
        battleStartOffset);
}

bool ClientInstance::Request_DialogueBattlePotatoBag()
{
    if (Is_BattleActive() || Is_BattleEntering())
        return false;

    // 감자자루 전투 진입 시, 특정 해금 진행
    RuntimeRuleStateService::Apply_ToCurrentRuntime(
        RuntimeRuleStateService::Build_PotatoBagStartUnlocks(), false);

    BattleMonsterSpawnDesc spawnDesc{};
    spawnDesc.prefabName = Constants::Prefab::MB_PotatoBag;

    vector<BattleMonsterSpawnDesc> battleMonsterSpawnDescs{};
    battleMonsterSpawnDescs.emplace_back(move(spawnDesc));

    if (_screenFxManager)
        _screenFxManager->Play_BattleInCounterFx();

    GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_Encounter);

    OnEnterBattleField(
        Constants::Value::potatoBagEncounterId,
        false,
        Resolve_BattleEncounterHeroIds(Constants::Value::potatoBagEncounterId),
        battleMonsterSpawnDescs,
        nullptr,
        false,
        Constants::Value::potatoBagBattleStartSpawnerId,
        Vec3::Zero);

    return true;
}

bool ClientInstance::Is_BattleActive() const
{
    return _battleController && _battleController->Is_BattleStarted();
}

bool ClientInstance::Is_BattleEntering() const
{
    return _battleController && _battleController->Is_BattleEntering();
}

bool ClientInstance::Is_PotatoBagBattleActive() const
{
    // 감자자루랑 전투중일 때에만 NPC가 보이게 처리하ㅣㄱ
    return _battleController &&
        _battleController->Is_BattleFieldActiveOrEntering(Constants::Value::potatoBagEncounterId);
}

void ClientInstance::Request_GamePlay()
{
    return _cameraDirector->Request_GamePlay();
}

void ClientInstance::Request_SoulLikeCamera()
{
    return _cameraDirector->Request_SoulLikeCamera();
}

void ClientInstance::Request_Preview()
{
    return _cameraDirector->Request_Preview();
}

void ClientInstance::Request_BattleTurn()
{
    return _cameraDirector->Request_BattleTurn();
}

Shared<CinematicCamera> ClientInstance::Request_Cinematic()
{
    return _cameraDirector->Request_Cinematic();
}

Shared<DialogueCamera> ClientInstance::Request_DialogueCamera()
{
    return _cameraDirector->Request_DialogueCamera();
}

void ClientInstance::Request_TargetCameraMode(TargetCameraMode cameraMode)
{
    return _cameraDirector->Request_ChangeTargetCameraMode(cameraMode);
}

void ClientInstance::Restore_PreviousCamera()
{
    return _cameraDirector->Restore_PreviousCamera();
}

bool ClientInstance::Play_Sequence(
    const wstring& filePath,
    int trackIndex,
    bool applyCameraTrack,
    bool applyEventTrack,
    bool applyActorTransformTrack)
{
    return _cinematicSequenceRunner->Play_Sequence(
        filePath,
        trackIndex,
        applyCameraTrack,
        applyEventTrack,
        applyActorTransformTrack);
}

bool ClientInstance::Play_Sequence(
    const CINEMATIC_SEQUENCE& sequence,
    int trackIndex,
    bool applyCameraTrack,
    bool applyEventTrack,
    bool applyActorTransformTrack)
{
    return _cinematicSequenceRunner->Play_Sequence(
        sequence,
        trackIndex,
        applyCameraTrack,
        applyEventTrack,
        applyActorTransformTrack);
}

void ClientInstance::Stop_Sequence()
{
    return  _cinematicSequenceRunner->Stop_Sequence();
}

void ClientInstance::Stop_SequenceKeepContext()
{
    return _cinematicSequenceRunner->Stop_SequenceKeepContext();
}

void ClientInstance::Pause_Sequence()
{
    if (_cinematicSequenceRunner == nullptr)
        return;

    _cinematicSequenceRunner->Pause_Sequence();
}

void ClientInstance::Resume_Sequence(
    bool applyCameraTrack,
    bool applyEventTrack,
    bool applyActorTransformTrack)
{
    if (_cinematicSequenceRunner == nullptr)
        return;

    _cinematicSequenceRunner->Resume_Sequence(
        applyCameraTrack,
        applyEventTrack,
        applyActorTransformTrack);
}

void ClientInstance::Set_CurrentSequenceFrame(int frame)
{
    if (_cinematicSequenceRunner == nullptr)
        return;

    _cinematicSequenceRunner->Set_CurrentFrame(frame);
}

void ClientInstance::Refresh_CurrentSequenceContext()
{
    if (_cinematicSequenceRunner == nullptr)
        return;

    _cinematicSequenceRunner->Refresh_CurrentFrameContext();
}

void ClientInstance::Preview_CurrentSequenceFrame(
    int frame,
    bool applyCameraTrack,
    bool applyEventTrack,
    bool applyActorTransformTrack,
    bool forceRebuildContext)
{
    if (_cinematicSequenceRunner == nullptr)
        return;

    _cinematicSequenceRunner->Preview_CurrentFrame(
        frame,
        applyCameraTrack,
        applyEventTrack,
        applyActorTransformTrack,
        forceRebuildContext);
}

void ClientInstance::Preview_CurrentSequenceFrameExact(
    float frameExact,
    bool applyCameraTrack,
    bool applyEventTrack,
    bool applyActorTransformTrack,
    bool forceRebuildContext)
{
    if (_cinematicSequenceRunner == nullptr)
        return;

    _cinematicSequenceRunner->Preview_CurrentFrameExact(
        frameExact,
        applyCameraTrack,
        applyEventTrack,
        applyActorTransformTrack,
        forceRebuildContext);
}

bool ClientInstance::Sync_CinematicPreviewSequence(
    const CINEMATIC_SEQUENCE& sequence,
    int trackIndex)
{
    if (_cinematicSequenceRunner == nullptr)
        return false;

    return _cinematicSequenceRunner->Sync_PreviewSequence(sequence, trackIndex);
}

void ClientInstance::Set_ActorTransformTrackPreviewSuspended(bool suspended)
{
    if (_cinematicSequenceRunner == nullptr)
        return;

    _cinematicSequenceRunner->Set_ActorTransformTrackPreviewSuspended(suspended);
}

bool ClientInstance::Is_CinematicPlaying() const
{
    return _cinematicSequenceRunner && _cinematicSequenceRunner->Is_CinematicPlaying();
}

bool ClientInstance::Is_CinematicPaused() const
{
    return _cinematicSequenceRunner && _cinematicSequenceRunner->Is_CinematicPaused();
}

int ClientInstance::Get_CurrentSequenceFrame() const
{
    if (_cinematicSequenceRunner == nullptr)
        return 0;

    return _cinematicSequenceRunner->Get_CurrentFrame();
}

float ClientInstance::Get_CurrentSequenceFrameExact() const
{
    if (_cinematicSequenceRunner == nullptr)
        return 0.f;

    return _cinematicSequenceRunner->Get_CurrentFrameExact();
}

Shared<GameObject> ClientInstance::Get_CinematicBoundActor(const string& targetId) const
{
    if (_cinematicSequenceRunner == nullptr)
        return nullptr;

    return _cinematicSequenceRunner->Get_BoundActor(targetId);
}

bool ClientInstance::Is_ActorTransformTrackPreviewSuspended() const
{
    return _cinematicSequenceRunner && _cinematicSequenceRunner->Is_ActorTransformTrackPreviewSuspended();
}

void ClientInstance::Update_CinematicPreview(float timeDelta)
{
    if (_cinematicSequenceRunner == nullptr)
        return;

    _cinematicSequenceRunner->Update(timeDelta, true);
}

FDelegateHandle ClientInstance::Add_CinematicCompletedCallback(const FOnCinematicCompleted::FunctionType& callback)
{
    if (_cinematicSequenceRunner == nullptr)
        return {};

    return _cinematicSequenceRunner->Add_CompletedCallback(callback);
}

void ClientInstance::Clear_CinematicCompletedCallbacks()
{
    if (_cinematicSequenceRunner == nullptr)
        return;

    _cinematicSequenceRunner->Clear_CompletedCallbacks();
}

Shared<CinematicCamera> ClientInstance::Ensure_CinematicCamera()
{
    if (auto camera = dynamic_pointer_cast<CinematicCamera>(GAME->Get_CameraByType(ObjectType::CinematicCamera)))
    {
        return camera;
    }

    Shared<GameObject> gameObject;

    CinematicCamera::CINEMATICCAMERA_DESC desc{};
    desc.speedPerSec = 10.f;
    desc.degreePerSec = 180.f;
    desc.eye = XMFLOAT4(0.f, 10.f, -7.f, 1.f);
    desc.at = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
    desc.fovy = XMConvertToRadians(60.f);
    desc.nearPlane = 0.1f;
    desc.farPlane = 500.f;
    desc.mouseSensor = 0.05f;

    if (FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), L"GameObject_CinematicCamera", ETOI(LevelType::Static), L"Layer_Camera", gameObject, &desc)))
    {
        return nullptr;
    }

    Shared<CinematicCamera> cinematicCamera = dynamic_pointer_cast<CinematicCamera>(gameObject);

    if (cinematicCamera == nullptr)
        return nullptr;

    GAME->Add_Camera(cinematicCamera);
    return cinematicCamera;
}

Shared<DialogueCamera> ClientInstance::Ensure_DialogueCamera()
{
    if (auto camera = dynamic_pointer_cast<DialogueCamera>(GAME->Get_CameraByType(ObjectType::DialogueCamera)))
    {
        return camera;
    }

    Shared<GameObject> gameObject;

    DialogueCamera::DIALOGUECAMERA_DESC desc{};
    desc.speedPerSec = 10.f;
    desc.degreePerSec = 180.f;
    desc.eye = XMFLOAT4(0.f, 10.f, -7.f, 1.f);
    desc.at = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
    desc.fovy = XMConvertToRadians(60.f);
    desc.nearPlane = 0.1f;
    desc.farPlane = 500.f;
    desc.mouseSensor = 0.05f;

    if (FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), L"GameObject_DialogueCamera", ETOI(LevelType::Static), L"Layer_Camera", gameObject, &desc)))
    {
        return nullptr;
    }

    Shared<DialogueCamera> dialogueCamera = dynamic_pointer_cast<DialogueCamera>(gameObject);

    if (dialogueCamera == nullptr)
        return nullptr;

    GAME->Add_Camera(dialogueCamera);
    return dialogueCamera;
}

Shared<ClientScreenFxManager> ClientInstance::Get_ScreenFxManager() const
{
    return _screenFxManager;
}

bool ClientInstance::Prewarm_Effect(const wstring& effectName, const wstring& layerTag, uint32 count)
{
    if (_effectInstancePool == nullptr)
        return false;

    if (effectName.empty() || layerTag.empty() || count == 0)
        return false;

    const wstring resolvedName = Normalize_EffectNameForPool(effectName);

    EffectInstancePool::AcquireDesc desc{};
    desc.effectName = resolvedName;
    desc.layerLevelIndex = GAME->Current_LevelIndex();
    desc.layerTag = layerTag;
    desc.worldPosition = Vec3::Zero;
    desc.worldRotation = Quat::Identity;
    desc.scale = Vec3::One;

    return SUCCEEDED(_effectInstancePool->Prewarm(desc, count));
}

void ClientInstance::Queue_BattleEffectPrewarmForBattleSetup(const vector<HeroId>& heroIds, const vector<BattleMonsterSpawnDesc>& monsterSpawnDescs, uint32 targetLevelIndex)
{
    vector<EffectPrewarmRequest> requests = BattleEffectPrewarmCollector::Collect_ForBattleSetup(heroIds, monsterSpawnDescs);
    for (EffectPrewarmRequest& request : requests)
        request.targetLevelIndex = targetLevelIndex;

    Queue_EffectPrewarmRequests(requests);
}

void ClientInstance::Queue_EffectPrewarmRequests(const vector<EffectPrewarmRequest>& requests)
{
    for (const EffectPrewarmRequest& request : requests)
    {
        const wstring resolvedName = Normalize_EffectNameForPool(request.effectName);
        if (resolvedName.empty())
            continue;

        const wstring resolvedLayerTag = request.layerTag.empty() ? L"Layer_Effect" : request.layerTag;
        const uint32 resolvedTargetCount = max(1u, request.targetCount);
        const uint32 resolvedTargetLevelIndex =
            request.targetLevelIndex == UINT32_MAX ? GAME->Current_LevelIndex() : request.targetLevelIndex;

        auto iter = find_if(
            _effectPrewarmQueue.begin(),
            _effectPrewarmQueue.end(),
            [&](const EffectPrewarmRequest& queued)
            {
                return queued.effectName == resolvedName &&
                    queued.layerTag == resolvedLayerTag &&
                    queued.targetLevelIndex == resolvedTargetLevelIndex;
            });

        if (iter != _effectPrewarmQueue.end())
        {
            iter->targetCount = max(iter->targetCount, resolvedTargetCount);
            continue;
        }

        EffectPrewarmRequest queuedRequest{};
        queuedRequest.effectName = resolvedName;
        queuedRequest.layerTag = resolvedLayerTag;
        queuedRequest.targetCount = resolvedTargetCount;
        queuedRequest.targetLevelIndex = resolvedTargetLevelIndex;
        _effectPrewarmQueue.push_back(queuedRequest);
    }
}

void ClientInstance::Update_EffectPrewarmQueue(uint32 createBudgetPerFrame)
{
    if (_effectInstancePool == nullptr || createBudgetPerFrame == 0)
        return;

    uint32 remainingBudget = createBudgetPerFrame;
    while (remainingBudget > 0 && !_effectPrewarmQueue.empty())
    {
        EffectPrewarmRequest& request = _effectPrewarmQueue.front();
        if (request.effectName.empty() || request.layerTag.empty() || request.targetCount == 0)
        {
            _effectPrewarmQueue.erase(_effectPrewarmQueue.begin());
            continue;
        }

        EffectInstancePool::AcquireDesc desc{};
        desc.effectName = request.effectName;
        desc.layerLevelIndex = request.targetLevelIndex == UINT32_MAX ? GAME->Current_LevelIndex() : request.targetLevelIndex;
        desc.layerTag = request.layerTag;
        desc.worldPosition = Vec3::Zero;
        desc.worldRotation = Quat::Identity;
        desc.scale = Vec3::One;

        EffectInstancePool::PrewarmResult result{};
        if (FAILED(_effectInstancePool->Prewarm_ToCount(desc, request.targetCount, remainingBudget, &result)))
        {
            LOG_WARN(
                "Effect prewarm failed. effect={}, layer={}, target={}",
                String::ToString(request.effectName),
                String::ToString(request.layerTag),
                request.targetCount);
            _effectPrewarmQueue.erase(_effectPrewarmQueue.begin());
            continue;
        }

        remainingBudget -= result.createdCount;
        if (result.existingCount + result.createdCount >= request.targetCount)
            _effectPrewarmQueue.erase(_effectPrewarmQueue.begin());

        if (result.createdCount == 0)
            break;
    }
}

void ClientInstance::Flush_EffectPrewarmQueue(uint32 createBudget)
{
    const uint32 budget = max(1u, createBudget);

    while (!_effectPrewarmQueue.empty())
        Update_EffectPrewarmQueue(budget);
}

void ClientInstance::Clear_EffectPrewarmQueue()
{
    _effectPrewarmQueue.clear();
}

bool ClientInstance::Play_Effect(const PlayEffectDesc& desc)
{
    if (_effectInstancePool == nullptr)
        return false;

    if (desc.effectName.empty() || desc.layerTag.empty())
        return false;

    wstring resolvedName = desc.effectName;
    if (!resolvedName.ends_with(L".effect.json"))
        resolvedName += L".effect.json";

    Vec3 spawnWorldPosition = desc.worldPosition;
    const Vec3 spawnRotationRad(
        XMConvertToRadians(desc.localRotation.x),
        XMConvertToRadians(desc.localRotation.y),
        XMConvertToRadians(desc.localRotation.z));
    Quat spawnWorldRotation = Quat::CreateFromYawPitchRoll(
        spawnRotationRad.y,
        spawnRotationRad.x,
        spawnRotationRad.z);
    spawnWorldRotation.Normalize();

    if (desc.spawnBasisTarget)
    {
        const Shared<TransformCom> basisTransform = desc.spawnBasisTarget->Get_Transform();
        if (basisTransform == nullptr)
            return false;

        const Vec3 localRotationRad(
            XMConvertToRadians(desc.localRotation.x),
            XMConvertToRadians(desc.localRotation.y),
            XMConvertToRadians(desc.localRotation.z));

        // 단발 기준 local
        const Matrix localMatrix =
            Matrix::CreateFromYawPitchRoll(localRotationRad.y, localRotationRad.x, localRotationRad.z) *
            Matrix::CreateTranslation(desc.localOffset);

        const Matrix finalWorldMatrix = localMatrix * basisTransform->Get_WorldMatrix();

        Vec3 decomposedScale = Vec3::One;
        Quat decomposedRotation = Quat::Identity;
        Vec3 decomposedTranslation = Vec3::Zero;

        Matrix tempMat = finalWorldMatrix;

        if (!tempMat.Decompose(decomposedScale, decomposedRotation, decomposedTranslation))
            return false;

        spawnWorldPosition = decomposedTranslation;
        spawnWorldRotation = decomposedRotation;
        if (spawnWorldRotation.LengthSquared() <= 0.000001f)
            spawnWorldRotation = Quat::Identity;
        else
            spawnWorldRotation.Normalize();
    }

    EffectInstancePool::AcquireDesc acquireDesc{};
    acquireDesc.effectName = resolvedName;
    acquireDesc.layerLevelIndex = GAME->Current_LevelIndex();
    acquireDesc.layerTag = desc.layerTag;
    acquireDesc.worldPosition = spawnWorldPosition;
    acquireDesc.worldRotation = spawnWorldRotation;
    acquireDesc.scale = desc.scale;
    acquireDesc.playbackSpeed = desc.playbackSpeed;

    EffectInstancePool::AttachedHandle handle{};
    if (FAILED(_effectInstancePool->Acquire(acquireDesc, handle)))
    {
        LOG_WARN("Play effect failed. effect={}", String::ToString(resolvedName).c_str());
        return false;
    }

    return true;
}

HRESULT ClientInstance::Reload_EffectJsons()
{
    if (_effectInstancePool == nullptr)
        return E_FAIL;

    const wstring assetRoot = GAME->Get_AssetRoot();
    if (assetRoot.empty())
        return E_FAIL;

    const fs::path effectAssetRoot = fs::path(assetRoot) / L"Effects" / L"Assets";
    CHECK_FAILED(GAME->Refresh_AssetRegistryDirectory(effectAssetRoot.wstring(), effectAssetRoot.wstring()), E_FAIL);

    // 새/수정된 effect registry 반영 뒤, 다음 Play_Effect가 최신 .effect.json을 디스크에서 다시 접게 한다.
    _effectInstancePool->Discard_AllInactive();
    return S_OK;
}

HRESULT ClientInstance::Reload_AnimNotifyJsons()
{
    function<void(const Shared<GameObject>&)> invalidateObjectNotifyData;
    invalidateObjectNotifyData = [&](const Shared<GameObject>& gameObject)
    {
        if (gameObject == nullptr)
            return;

        if (const Shared<ModelCom> modelCom = gameObject->Get_Component<ModelCom>())
            modelCom->Invalidate_AnimNotifyAsset();

        const Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(gameObject);
        if (container == nullptr)
            return;

        for (const auto& [partTag, partObject] : container->Get_PartObjects())
        {
            UNREFERENCED_PARAMETER(partTag);
            invalidateObjectNotifyData(partObject);
        }
    };

    GAME->Visit_CurrentLevelObjects(
        [&](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            UNREFERENCED_PARAMETER(layerTag);
            invalidateObjectNotifyData(gameObject);
        });

    return S_OK;
}

bool ClientInstance::Play_AttachPartEffect(
    const Shared<GameObject>& effectOwner,
    const wstring& effectName,
    const wstring& partTag,
    const Vec3& localOffset,
    const Vec3& localRotation,
    const Vec3& scale)
{
    if (effectOwner == nullptr)
        return false;

    const Shared<EffectCom> effectCom = effectOwner->Get_Component<EffectCom>();
    if (effectCom == nullptr)
        return false;

    return SUCCEEDED(effectCom->Play_AttachedPartEffect(
        GAME->Current_LevelIndex(),
        effectName,
        partTag,
        L"Layer_Effect",
        localOffset,
        localRotation,
        scale));
}

bool ClientInstance::Play_AttachPartEffect(
    const Shared<GameObject>& effectOwner,
    const wstring& effectName,
    const wstring& partTag,
    const string& socketName,
    const Vec3& localOffset,
    const Vec3& localRotation,
    const Vec3& scale)
{
    if (effectOwner == nullptr)
        return false;

    const Shared<EffectCom> effectCom = effectOwner->Get_Component<EffectCom>();
    if (effectCom == nullptr)
        return false;

    return SUCCEEDED(effectCom->Play_AttachedPartEffect(
        GAME->Current_LevelIndex(),
        effectName,
        partTag,
        socketName,
        L"Layer_Effect",
        localOffset,
        localRotation,
        scale));
}

void ClientInstance::Clear_RuntimeEffects()
{
    if (_effectInstancePool == nullptr)
        return;

    // 월드에 남아있는 이팩트들 제거
    _effectInstancePool->Discard_All();
}

void ClientInstance::Notify_FreeAimShotHit()
{
    OnFreeAimShotHit.Broadcast();
}

bool ClientInstance::Is_GameplayMenuOpen() const
{
    return _gamePlayUI && _gamePlayUI->Is_AnyMenuOpen();
}

Shared<GameObject> ClientInstance::Find_Player(uint32 levelIndex)
{
    if (_heroManager)
    {
        auto activeHero = _heroManager->Get_ActiveHero();
        if (activeHero)
            return activeHero;
    }

    return GAME->Find_FirstObject(levelIndex, TEXT("Layer_Player"));
}

HRESULT ClientInstance::Ready_StaticObjects()
{
    if (_isStaticObjectsSpawned)
        return S_OK;

    CHECK_NULL(_persistentObjectManager, E_FAIL);

    CHECK_FAILED(_persistentObjectManager->Spawn_StaticObjects(), E_FAIL);

    _isStaticObjectsSpawned = true;
    return S_OK;
}

void ClientInstance::Sync_PreviewHeroRuntimeData(HeroId heroId)
{
    if (_previewManager == nullptr)
        return;

    _previewManager->Sync_PreviewHeroRuntimeData(heroId);
}

void ClientInstance::Play_PreviewAnimation(HeroId heroId, PreviewAnimState introAnim, PreviewAnimState idleAnim)
{
    if (_previewManager == nullptr)
        return;

    _previewManager->Play_PreviewAnimation(heroId, introAnim, idleAnim);
}

void ClientInstance::Set_SoulLikeController(const Shared<SoulLikeController>& controller)
{
    _soulLikeController = controller;
}

bool ClientInstance::Is_SoulLikeBattleActive() const
{
    return _soulLikeController != nullptr && _soulLikeController->Is_BattleActive();
}

bool ClientInstance::Is_WorldControlSuspended() const
{
    return Is_BattleActive() || Is_SoulLikeBattleActive();
}

const DialogueData* ClientInstance::FindDialogueData(const string& dialogueKey) const
{
    return _dialogueDB->FindDialogueData(dialogueKey);
}

void ClientInstance::StartDialogue(const DialogueRequest& request)
{
    _dialogueManager->StartDialogue(request);
}

void ClientInstance::EndDialogue(bool restoreWorldState)
{
    if (_dialogueManager && _dialogueManager->IsActive())
        _dialogueManager->EndDialogue(restoreWorldState);
}

void ClientInstance::Reset_LastDialogueNpcAnimationToStart()
{
    if (_dialogueManager == nullptr)
        return;

    _dialogueManager->Reset_LastDialogueNpcAnimationToStart();
}

bool ClientInstance::IsDialogueActive() const
{
    return _dialogueManager && _dialogueManager->IsActive();
}

bool ClientInstance::IsDialogueInteractLocked() const
{
    return _dialogueManager && _dialogueManager->IsInteractLocked();
}

void ClientInstance::AddDialogueInteractionOverlap()
{
    ++_dialogueInteractionOverlapCount;
}

void ClientInstance::RemoveDialogueInteractionOverlap()
{
    if (_dialogueInteractionOverlapCount > 0)
        --_dialogueInteractionOverlapCount;
}

void ClientInstance::AddItemInteractionOverlap()
{
    ++_itemInteractionOverlapCount;
}

void ClientInstance::RemoveItemInteractionOverlap()
{
    if (_itemInteractionOverlapCount > 0)
        --_itemInteractionOverlapCount;

}
void ClientInstance::Request_InteractionPrompt(const void* sourceId, InteractionPromptType type)
{
    if (sourceId == nullptr || type == InteractionPromptType::None)
        return;

    const int32 priority = Resolve_InteractionPromptPriority(type);
    if (_hasInteractionPromptRequest && _interactionPromptSourceId != sourceId)
    {
        if (priority <= _interactionPromptPriority)
            return;
    }

    _interactionPromptSourceId = sourceId;
    _interactionPromptType = type;
    _interactionPromptPriority = priority;
    _hasInteractionPromptRequest = true;
}

void ClientInstance::Clear_InteractionPrompt(const void* sourceId)
{
    if (!_hasInteractionPromptRequest)
        return;

    if (sourceId != nullptr && _interactionPromptSourceId != sourceId)
        return;

    _hasInteractionPromptRequest = false;
    _interactionPromptSourceId = nullptr;
    _interactionPromptType = InteractionPromptType::None;
    _interactionPromptPriority = 0;
}

bool ClientInstance::Try_GetInteractionPromptType(InteractionPromptType& outType) const
{
    if (!_hasInteractionPromptRequest)
        return false;

    if (_interactionPromptType == InteractionPromptType::None)
        return false;

    outType = _interactionPromptType;
    return true;
}

void ClientInstance::Set_DialogueOverride(const string& sourceDialogueKey, const string& targetDialogueKey)
{
    if (_dialogueManager == nullptr)
        return;

    _dialogueManager->Set_DialogueOverride(sourceDialogueKey, targetDialogueKey);
}

HRESULT ClientInstance::Ready_ResourceLoader()
{
    _resourceLoader = ResourceLoader::Create(_device, _context);
    CHECK_NULL(_resourceLoader, E_FAIL);

    return S_OK;
}

HRESULT ClientInstance::Ready_GameDataTables()
{
    _gameDataTable = GameDataTable::Create();
    CHECK_NULL(_gameDataTable, E_FAIL);

    CHECK_FAILED(_gameDataTable->Load_All(), E_FAIL);

    return S_OK;
}

void ClientInstance::Release_Client()
{
    // 디바이스 리셋 전 정적 전역 맵 모델 및 콜리전 캐시 리소스를 완전히 방출
    MapLoader::Clear_GlobalCache();

    auto ReleaseManager = [](auto& manager)
        {
            if (manager)
            {
                manager->Free();
                manager.reset();
            }
        };

    ReleaseManager(_gameDataTable);
    ReleaseManager(_previewShotTable);
    ReleaseManager(_previewManager);
    ReleaseManager(_effectInstancePool);
    ReleaseManager(_screenFadeTransition);
    ReleaseManager(_levelTransitionFade);
    ReleaseManager(_persistentObjectManager);
    ReleaseManager(_resourceLoader);
    ReleaseManager(_heroManager);
    ReleaseManager(_playerInventory);
    ReleaseManager(_costumeManager);
    ReleaseManager(_battleController);
    ReleaseManager(_gamePlayUI);
    ReleaseManager(_screenFxManager);
    ReleaseManager(_soulLikeController);

    Clear_ReplayWorldSnapshot();

    _context.Reset();
    _device.Reset();

    _isStaticResourcesLoaded.store(false);
    _isStaticObjectsSpawned = false;
    _isInitialized = false;
}

void ClientInstance::Free()
{
    Release_Client();

    __super::Free();
}
