#pragma once
#include "Base.h"
#include "BattleMonsterSpawnDesc.h"
#include "CameraPresetTypes.h"
#include "Cinematic_Types.h"
#include "GameDataTable.h"
#include "GameplayUI_Types.h"
#include "Delegate.h"
#include "PreviewShotTable.h"
#include "RuntimeCheckpointIO.h"
#include "UIPreviewManager.h"
#include "BattleEffectPrewarmTypes.h"

NS_BEGIN(Engine)
class GameInstance;
class Level;
NS_END

NS_BEGIN(Client)

struct DialogueRequest;
struct EventDialogueUIRequest;
struct ActionRequest;
struct BattleChar;
enum class BattleTurnState;

    class DialogueCamera;
class DialogueManager;
struct DialogueData;
class DialogueDB;
class CinematicSequenceRunner;
class CinematicCamera;
class BattleController;
class Player_WorldBase;
class TargetCamera;
class ResourceLoader;
class GameplayUI;
class PersistentObjectManager;
class Hero_Manager;
class CameraDirector;
class EffectInstancePool;
class ScreenFadeTransition;
class LevelTransitionFade;
class ClientScreenFxManager;
class PlayerInventory;
class CostumeManager;

class SoulLikeController;
class Unlock_Manager;
class BattleFollowupManager;
class SimonCriticalGlowController;
class BattleTargetingVoicePreset;
class BattleTurnStartVoicePreset;

DECLARE_DELEGATE(FOnFreeAimShotHit);

class ClientInstance : public Base
{
    DECLARE_SINGLETON(ClientInstance)

public:
	ClientInstance();
	~ClientInstance() override;

public:
	HRESULT Initialize_Client(const ComPtr<Device>& device, const ComPtr<Context>& context);
    HRESULT Initialize_Client_ForEffectEditor(const ComPtr<Device>& device, const ComPtr<Context>& context);
    void    Update_Client(float timeDelta);
    void    Update_FadeTransitions(float timeDelta);
    void    Update_UIPreviewOnly(float timeDelta);
	void    Release_Client();

	/* 레벨 진입 전에 공용/정적 클라 리소스를 준비할 때 호출한다. */
	HRESULT Ready_StaticResources();
	HRESULT Ready_LevelResources(LevelType levelType);
    HRESULT Ready_StaticObjects();

	Shared<ResourceLoader> Get_ResourceLoader() const;
    EffectInstancePool* Get_EffectInstancePool() const { return _effectInstancePool.get(); } // attached/runtime effect acquire/release를 맡는 Client-local pool owner다.

    bool    Is_StaticResourcesLoaded() const { return _isStaticResourcesLoaded.load(); }
    void    Set_StaticResourcesLoaded(bool isLoaded) { _isStaticResourcesLoaded.store(isLoaded); }

    bool    Is_GameplayMenuOpen() const;

    static bool Is_DebugDisableDynamicShadowRenderPass();
    static void Set_DebugDisableDynamicShadowRenderPass(bool disabled);
    static bool Is_DebugDisableMapInstancedMeshRender();
    static void Set_DebugDisableMapInstancedMeshRender(bool disabled);
    static bool Is_DebugDisableGrassInstancedRender();
    static void Set_DebugDisableGrassInstancedRender(bool disabled);
    static bool Is_DebugDisableMapStaticMeshRender();
    static void Set_DebugDisableMapStaticMeshRender(bool disabled);
    static void Request_DebugMapInstancedVisibleCountReadback();
    static uint32 Get_DebugMapInstancedVisibleCountReadbackRequestId();
    static bool Is_DebugForceGestralPalisadeInstancedCull();
    static void Set_DebugForceGestralPalisadeInstancedCull(bool enabled);
    static bool Is_DebugDisableMapInstancedMeshShadow();
    static void Set_DebugDisableMapInstancedMeshShadow(bool disabled);
    static bool Is_DebugUseMapInstancedMeshShadowLOD();
    static void Set_DebugUseMapInstancedMeshShadowLOD(bool enabled);
    static bool Is_DebugDisableGrassInstancedShadow();
    static void Set_DebugDisableGrassInstancedShadow(bool disabled);
    static bool Is_DebugDisableMapStaticMeshShadow();
    static void Set_DebugDisableMapStaticMeshShadow(bool disabled);
    static void Set_PotatoBagArenaMapRenderGateBasis(const Vec3& origin, const Vec3& right, const Vec3& forward);
    static void Clear_PotatoBagArenaMapRenderGateBasis();
    static void Set_SoulLikeArenaMapRenderGateBasis(const Vec3& origin, const Vec3& right, const Vec3& forward);
    static void Clear_SoulLikeArenaMapRenderGateBasis();
    static bool Is_PotatoBagArenaMapRenderGateActive();
    static uint32 Get_PotatoBagArenaMapRenderGateRevision();
    static bool Should_RenderMapObjectInPotatoBagArena(const BoundingBox& worldBounds);
    static bool Should_RenderMapInstanceInPotatoBagArena(const BoundingSphere& worldSphere);
    static void Update_PotatoBagArenaMapInstancedShadowOverride();
    static void Toggle_PotatoBagArenaMapRenderGateDebug();
    static void Draw_PotatoBagArenaMapRenderGateDebug();

public: /* 플레이어 찾기 -> 일단 임시 */
    Shared<GameObject> Find_Player(uint32 levelIndex);

public: /* For.GameData Table */
    uint32        Get_NextExp(uint32 level) const;
    GameDataTable::SkillTreeDesc* Get_SkillTreeDesc(const string& skillName);
    const umap<string, vector<string>>* Get_SkillTreeRequirementData();
    const GameDataTable::WeaponDesc* Get_WeaponDesc(const string& characterId, const string& weaponId) const;
    const vector<GameDataTable::WeaponDesc>* Get_WeaponDescs(const string& characterId) const;
    const GameDataTable::WeaponUpgradeDesc* Get_WeaponUpgradeDesc(const string& characterId, const string& weaponId, uint32 level) const;
    const GameDataTable::WeaponUpgradeDesc* Get_NextWeaponUpgradeDesc(const string& characterId, const string& weaponId, uint32 currentLevel) const;
    uint32 Resolve_WeaponEnhanceLevel(const string& characterId, const string& weaponId) const;
    uint32 Resolve_WeaponPower(const string& characterId, const string& weaponId) const;
    const Skill_Data* Get_Skill(const string& skillId);
    const vector<string> Get_AllSkillIds();
    HRESULT Reload_GameDataTables();
    HRESULT Reload_CameraPresetData();

    const Item_Data*  Get_Item(const string& itemId);
    const Item_Data*  Get_Item(BattleItemType itemType);
    bool              Consume_Item(BattleItemType itemType);

    const GameDataTable::PictosDesc* Get_PictosDesc(const string& pictosId) const;
    const vector<GameDataTable::PictosDesc>& Get_PictosDescs() const;
    const vector<string> Get_AllPictosIds() const;

public:
    void    Apply_PreviewLayout(const vector<UIPreviewManager::PreviewSlotDesc>& layout);
    void    Begin_Preview(PreviewShotPanel panel);
    void    Show_Preview(HeroId heroId, PreviewShotPanel panel);
    void    Hide_Preview();

public:
    PreviewShotPreset Get_Preset(HeroId hero, PreviewShotPanel panel);

public: /* For.Hero Manager */
    HRESULT        Initialize_Hero(uint32 levelIndex, const wstring& layerTag, const Vec3& startPos = Vec3::Zero, const Vec3& startRot = Vec3::Zero);
    void           Update_HeroManager(float timeDelta);
    void           Set_TargetCamera(const Shared<TargetCamera>& camera);
    void           Clear_Party();

    // 컨트롤중인 플레이어
    Shared<Player_WorldBase>  Get_ActiveHero() const;
    HeroPartySlot   Get_ActiveHeroSlot() const;
    void            Set_ActiveHeroSlotForCheckpoint(HeroPartySlot slot);
    void            Set_DialogueFocusActive(bool active);

    // 플레이어 전부 가져오기
    const vector<HeroPartyEntry>& Get_HeroEntries() const;
    void Restore_CurrentHeroesHpToMax();

    // 특정 슬롯 반환
    Shared<Player_WorldBase>  Get_Hero(HeroPartySlot slot) const;
    HeroId          Get_HeroId(HeroPartySlot slot) const;

    // 맨 앞 플레이어. Get_Active와 역할이 거의 동일
    Shared<Player_WorldBase>  Get_FrontHero() const;

    Shared<Player_WorldBase>  Find_Hero(HeroId heroId) const;

    const HeroSpawnDesc* Find_HeroSpawnDesc(HeroId heroId);

    // 장착 무기 비어있으면 디폴트 세팅
    bool Ensure_DefaultEquippedWeapon(HeroId heroId); 

    void Reset_AllHeroesToIdle();
    void Restore_PartyFormationAfterEditorStop();

    bool    Is_ActiveHero(const Shared<GameObject>& target) const;
    void    Warp_PartyToFormation(const Vec3& anchorPos, const Vec3& anchorRot);

public: /* For.Gameplay UI */
    HRESULT Initialize_GameplayUI(uint32 levelIndex);
    void    Clear_GameplayUI();
    Shared<GameplayUI> Get_GameplayUI() const { return _gamePlayUI; }

    void    Play_BattleEnterTransition();
    void    Stop_BattleEnterTransition();
    bool    Is_BattleEnterTransitionPlaying() const;
    void    Queue_LevelAnnounce(LevelType levelType);
    bool    Open_Journal(JournalOpenAnimation openAnimation = JournalOpenAnimation::JournalUI);
    bool    Open_EventDialogueUI(const EventDialogueUIRequest& request);
    void    Close_EventDialogueUI();

public: /* For.Fade Transition */
    bool Request_ScreenFadeTransition(const function<void()>& CoveredAction);
    bool Request_LevelTransition(LevelType TargetLevel);
    bool Request_LoadedLevelTransition(LevelType TargetLevel, Shared<Level> TargetLevelInstance);
    bool Request_CurrentLevelReplay();
    void Reset_CurrentLevelRuntimeState();
    void Capture_InitialWorldSnapshot();
    void Clear_ReplayWorldSnapshot();

    bool Is_FadeTransitionPlaying() const;

public: /* For.Battle_Controller */
    void OnEnterBattleField(
        uint32 FieldID,
        bool isPlayerAdvantage,
        const vector<HeroId>& heroIds,
        const vector<BattleMonsterSpawnDesc>& battleMonsterSpawnDescs,
        const Shared<GameObject>& sourceObject = nullptr,
        bool completeImmediately = false,
        int32 battleStartSpawnerId = 0,
        const Vec3& battleStartOffset = Vec3::Zero);

    bool Request_DialogueBattlePotatoBag();

    Shared<BattleController>    Get_BattleController() { return _battleController; };

    bool Is_BattleActive() const;
    bool Is_BattleEntering() const;

    bool Is_PotatoBagBattleActive() const;
    bool Play_BattleMonsterTargetingVoice(
        const ActionRequest& request,
        const vector<BattleChar>& targetedHeroes);
    bool Play_BattleTurnStartVoice();

public: /*For.Camera Director*/
    void Request_GamePlay();
    void Request_SoulLikeCamera();
    void Request_Preview();
    void Request_BattleTurn();
    Shared<CinematicCamera> Request_Cinematic();
    Shared<DialogueCamera> Request_DialogueCamera();
    void Request_TargetCameraMode(TargetCameraMode cameraMode);
    void Restore_PreviousCamera();

public: /*CinematicSequenceRunner*/
    bool Play_Sequence(
        const wstring& filePath,
        int trackIndex,
        bool applyCameraTrack = true,
        bool applyEventTrack = true,
        bool applyActorTransformTrack = true);
    bool Play_Sequence(
        const CINEMATIC_SEQUENCE& sequence,
        int trackIndex,
        bool applyCameraTrack = true,
        bool applyEventTrack = true,
        bool applyActorTransformTrack = true);
    void Pause_Sequence();
    void Resume_Sequence(
        bool applyCameraTrack = true,
        bool applyEventTrack = true,
        bool applyActorTransformTrack = true);
    void Set_CurrentSequenceFrame(int frame);
    void Refresh_CurrentSequenceContext();
    void Preview_CurrentSequenceFrame(
        int frame,
        bool applyCameraTrack,
        bool applyEventTrack,
        bool applyActorTransformTrack,
        bool forceRebuildContext = false);
    void Preview_CurrentSequenceFrameExact(
        float frameExact,
        bool applyCameraTrack,
        bool applyEventTrack,
        bool applyActorTransformTrack,
        bool forceRebuildContext = false);
    bool Sync_CinematicPreviewSequence(const CINEMATIC_SEQUENCE& sequence, int trackIndex = -1);
    void Set_ActorTransformTrackPreviewSuspended(bool suspended);
    void Stop_Sequence();
    void Stop_SequenceKeepContext();
    bool Is_CinematicPlaying() const;
    bool Is_CinematicPaused() const;
    int Get_CurrentSequenceFrame() const;
    float Get_CurrentSequenceFrameExact() const;
    Shared<GameObject> Get_CinematicBoundActor(const string& targetId) const;
    bool Is_ActorTransformTrackPreviewSuspended() const;
    void Update_CinematicPreview(float timeDelta);

    // 시네마틱 종료됐을 떄 알림
    FDelegateHandle Add_CinematicCompletedCallback(const FOnCinematicCompleted::FunctionType& callback);
    void Clear_CinematicCompletedCallbacks();

    void Queue_CinematicPrewarmForLevel(uint32 targetLevelIndex);
    void Flush_CinematicPrewarmQueue(uint32 createBudget = UINT32_MAX);
    void Clear_CinematicPrewarmQueue();

public:
    Shared<CinematicCamera> Ensure_CinematicCamera();
    Shared<DialogueCamera> Ensure_DialogueCamera();

public:
    void Set_TargetCameraEditMode(bool enable) { _isTargetCameraEditMode = enable; }
    bool Is_TargetCameraEditMode() { return _isTargetCameraEditMode;}

public:
    Shared<ClientScreenFxManager> Get_ScreenFxManager() const;
    Shared<UIPreviewManager> Get_PreviewManager() const { return _previewManager; }

public: /* Effect 재생 */
    bool Prewarm_Effect(const wstring& effectName, const wstring& layerTag, uint32 count);
    void Queue_BattleEffectPrewarmForBattleSetup(const vector<HeroId>& heroIds, const vector<BattleMonsterSpawnDesc>& monsterSpawnDescs, uint32 targetLevelIndex = UINT32_MAX);
    void Queue_EffectPrewarmRequests(const vector<EffectPrewarmRequest>& requests);
    void Update_EffectPrewarmQueue(uint32 createBudgetPerFrame = 2);
    void Flush_EffectPrewarmQueue(uint32 createBudget = UINT32_MAX);
    void Clear_EffectPrewarmQueue();

    // 단발성 1회 재생.
    bool Play_Effect(const PlayEffectDesc& desc);
    HRESULT Reload_EffectJsons();
    HRESULT Reload_AnimNotifyJsons();

    // 약점 파츠처럼 모델/소켓이 없을 때
    bool Play_AttachPartEffect(
        const Shared<GameObject>& effectOwner,
        const wstring& effectName,
        const wstring& partTag,
        const Vec3& localOffset = Vec3::Zero,
        const Vec3& localRotation = Vec3::Zero,
        const Vec3& scale = Vec3::One);

    // 모델 있어서, socket 추적할 때
    bool Play_AttachPartEffect(
        const Shared<GameObject>& effectOwner,
        const wstring& effectName,
        const wstring& partTag,
        const string& socketName,
        const Vec3& localOffset = Vec3::Zero,
        const Vec3& localRotation = Vec3::Zero,
        const Vec3& scale = Vec3::One);

    void Clear_RuntimeEffects();

public: /* Delegate */
    void Notify_FreeAimShotHit();

public: /* Player Inventory */
    Shared<PlayerInventory> Get_PlayerInventory() const { return _playerInventory; }

public: /* Costume Manager */
    CostumeManager* Get_CostumeManager() const { return _costumeManager.get(); }

    void    Sync_PreviewHeroRuntimeData(HeroId heroId);
    void    Play_PreviewAnimation(HeroId heroId, PreviewAnimState introAnim, PreviewAnimState idleAnim);

public: /* SoulLike */
    void Set_SoulLikeController(const Shared<SoulLikeController>& controller);
    Shared<SoulLikeController> Get_SoulLikeController() const { return _soulLikeController; }

    bool Is_SoulLikeBattleActive() const;
    bool Is_WorldControlSuspended() const;

public: /* Unlock Manager */
    Unlock_Manager* Get_UnlockManager() const { return _unlockManager.get(); }

public:
    const DialogueData* FindDialogueData(const string& dialogueKey) const;
    

  public:
    void StartDialogue(const DialogueRequest& request);
    void EndDialogue(bool restoreWorldState = true);
    void Reset_LastDialogueNpcAnimationToStart();
    bool IsDialogueActive() const;
    bool IsDialogueInteractLocked() const;
    void AddDialogueInteractionOverlap();
    void RemoveDialogueInteractionOverlap();
    bool HasDialogueInteractionOverlap() const { return _dialogueInteractionOverlapCount > 0; }
    void AddItemInteractionOverlap();
    void RemoveItemInteractionOverlap();
    bool HasItemInteractionOverlap() const { return _itemInteractionOverlapCount > 0; }

    void Request_InteractionPrompt(const void* sourceId, InteractionPromptType type);
    void Clear_InteractionPrompt(const void* sourceId);
    bool Try_GetInteractionPromptType(InteractionPromptType& outType) const;

    void Set_DialogueOverride(const string& sourceDialogueKey, const string& targetDialogueKey);

public: /* Battle Followup Manager */
     BattleFollowupManager* Get_BattleFollowupManager() const { return _battleFollowupManager.get(); }

public: // Simon Critical Glow Controller
    SimonCriticalGlowController* Get_SimonCriticalGlowController() const { return _simonCriticalGlowController.get(); }

private:
	HRESULT Ready_ResourceLoader();
    HRESULT Ready_GameDataTables();
    void Handle_BattleTargetingVoiceTurnStarted(const BattleChar& actor, BattleTurnState state);
    void Capture_ReplayWorldSnapshotIfNeeded();
    void Restore_ReplayWorldObjectsFromSnapshot();
    void Capture_ReplayCameraSnapshot();
    void Restore_ReplayCameraSnapshot();

public:
    FOnFreeAimShotHit OnFreeAimShotHit;

private:
	ComPtr<Device>  _device;
	ComPtr<Context> _context;

    atomic<bool> _isStaticResourcesLoaded = false;
    bool         _isStaticObjectsSpawned = false;
    bool         _isInitialized = false;
    bool         _isTargetCameraEditMode = false;
    int          _dialogueInteractionOverlapCount = 0;
    int          _itemInteractionOverlapCount = 0;
    bool         _hasInteractionPromptRequest = false;
    const void*  _interactionPromptSourceId = nullptr;
    InteractionPromptType _interactionPromptType = InteractionPromptType::None;
    int32        _interactionPromptPriority = 0;

private:
    Shared<CameraDirector>              _cameraDirector;
    Shared<UIPreviewManager>            _previewManager;
    Shared<ResourceLoader>              _resourceLoader;
    Unique<GameDataTable>               _gameDataTable;
    Unique<PreviewShotTable>            _previewShotTable;
    Unique<EffectInstancePool>          _effectInstancePool;
    vector<EffectPrewarmRequest>        _effectPrewarmQueue{};
    Unique<ScreenFadeTransition>        _screenFadeTransition;
    Unique<LevelTransitionFade>         _levelTransitionFade;
    Unique<CinematicSequenceRunner>     _cinematicSequenceRunner;
    Unique<DialogueDB>                  _dialogueDB;
    Unique<DialogueManager>             _dialogueManager;
    Shared<GameplayUI>                  _gamePlayUI;
    Unique<PersistentObjectManager>     _persistentObjectManager;
    Unique<Hero_Manager>                _heroManager;
    Shared<ClientScreenFxManager>       _screenFxManager;

    Shared<BattleController>    _battleController;
    Unique<BattleTargetingVoicePreset> _battleTargetingVoicePreset;
    Unique<BattleTurnStartVoicePreset> _battleTurnStartVoicePreset;
    FDelegateHandle             _battleTargetingVoiceTurnStartedHandle{};
    Shared<PlayerInventory>     _playerInventory;

    Unique<CostumeManager>      _costumeManager;
    Shared<SoulLikeController> _soulLikeController;

    Unique<Unlock_Manager>      _unlockManager;
    Unique<BattleFollowupManager>   _battleFollowupManager;
    vector<RuntimeCheckpointIO::WorldObject> _replayWorldSpawnObjects;
    vector<uint8> _replayWorldRuntimeAuthoringLoaded;
    json _replayWorldInventoryJson = json::object();
    json _replayWorldUnlockManagerJson = json::object();
    uint32 _replayWorldSnapshotLevelIndex = UINT32_MAX;
    bool _replayWorldSnapshotCaptured = false;

    struct ReplayCameraTransformState
    {
        bool hasCamera = false;
        Vec3 position = Vec3::Zero;
        Vec3 rotation = Vec3::Zero;
        float yaw = 0.f;
        float pitch = 0.f;
        float fovY = 0.f;
        float nearPlane = 0.f;
        float farPlane = 0.f;
    };

    struct ReplayCameraSnapshot
    {
        bool captured = false;
        ObjectType activeType = ObjectType::None;
        TargetCameraMode targetMode = TargetCameraMode::NORMAL;
        ReplayCameraTransformState targetCamera{};
        ReplayCameraTransformState freeCamera{};
    };

    ReplayCameraSnapshot _replayCameraSnapshot{};

    Unique<SimonCriticalGlowController> _simonCriticalGlowController;

public:
	void Free() override;

};

NS_END
