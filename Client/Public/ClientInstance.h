#pragma once
#include "Base.h"
#include "BattleMonsterSpawnDesc.h"
#include "GameDataTable.h"
#include "Delegate.h"
#include "PreviewShotTable.h"
#include "UIPreviewManager.h"

NS_BEGIN(Engine)
    class GameInstance;
    class Level;
NS_END

NS_BEGIN(Client)

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

public: /* 플레이어 찾기 -> 일단 임시 */
    Shared<GameObject> Find_Player(uint32 levelIndex);

public: /* For.GameData Table */
    uint32        Get_NextExp(uint32 level) const;
    GameDataTable::SkillTreeDesc* Get_SkillTreeDesc(const string& skillName);
    const umap<string, vector<string>>* Get_SkillTreeRequirementData();
    const GameDataTable::WeaponDesc* Get_WeaponDesc(const string& characterId, const string& weaponId) const;
    const vector<GameDataTable::WeaponDesc>* Get_WeaponDescs(const string& characterId) const;
    const Skill_Data* Get_Skill(const string& skillId);
    const vector<string> Get_AllSkillIds();

    const Item_Data*  Get_Item(const string& itemId);
    const Item_Data*  Get_Item(BattleItemType itemType);
    bool              Consume_Item(BattleItemType itemType);

public:
    void    Apply_PreviewLayout(const vector<UIPreviewManager::PreviewSlotDesc>& layout);
    void    Begin_Preview(PreviewShotPanel panel);
    void    Show_Preview(HeroId heroId, PreviewShotPanel panel);
    void    Hide_Preview();

public:
    PreviewShotPreset Get_Preset(HeroId hero, PreviewShotPanel panel);

public: /* For.Hero Manager */
    HRESULT        Initialize_Hero(uint32 levelIndex, const wstring& layerTag, const Vec3& startPos = Vec3::Zero);
    void           Update_HeroManager(float timeDelta);
    void           Set_TargetCamera(const Shared<TargetCamera>& camera);
    void           Clear_Party();

    // 컨트롤중인 플레이어
    Shared<Player_WorldBase>  Get_ActiveHero() const;
    HeroPartySlot   Get_ActiveHeroSlot() const;

    // 플레이어 전부 가져오기
    const vector<HeroPartyEntry>& Get_HeroEntries() const;

    // 특정 슬롯 반환
    Shared<Player_WorldBase>  Get_Hero(HeroPartySlot slot) const;
    HeroId          Get_HeroId(HeroPartySlot slot) const;

    // 맨 앞 플레이어. Get_Active와 역할이 거의 동일
    Shared<Player_WorldBase>  Get_FrontHero() const;

    Shared<Player_WorldBase>  Find_Hero(HeroId heroId) const;

    const HeroSpawnDesc* Find_HeroSpawnDesc(HeroId heroId);

    void Reset_AllHeroesToIdle();
    void Restore_PartyFormationAfterEditorStop();

    bool    Is_ActiveHero(const Shared<GameObject>& target) const;

public: /* For.Gameplay UI */
    HRESULT Initialize_GameplayUI(uint32 levelIndex);
    void    Clear_GameplayUI();
    Shared<GameplayUI> Get_GameplayUI() const { return _gamePlayUI; }

    void    Play_BattleEnterTransition();
    void    Stop_BattleEnterTransition();
    bool    Is_BattleEnterTransitionPlaying() const;

public: /* For.Fade Transition */
    bool Request_ScreenFadeTransition(const function<void()>& CoveredAction);
    bool Request_LevelTransition(LevelType TargetLevel);
    bool Request_LoadedLevelTransition(LevelType TargetLevel, Shared<Level> TargetLevelInstance);
    bool Is_FadeTransitionPlaying() const;

public: /* For.Battle_Controller */
    void OnEnterBattleField(
        uint32 FieldID,
        bool isPlayerAdvantage,
        const vector<HeroId>& heroIds,
        const vector<BattleMonsterSpawnDesc>& battleMonsterSpawnDescs,
        const Shared<GameObject>& sourceObject = nullptr);

    Shared<BattleController>    Get_BattleController() { return _battleController; };

    bool Is_BattleActive() const;
    bool Is_BattleEntering() const;

public: /*For.Camera Director*/
    void Request_GamePlay();
    void Request_Preview();
    void Request_BattleTurn();
    Shared<CinematicCamera> Request_Cinematic();
    void Request_TargetCameraMode(TargetCameraMode cameraMode);
    void Restore_PreviousCamera();

public: /*CinematicSequenceRunner*/
    bool Play_Sequence(const wstring& filePath, int trackIndex);
    void Stop_Sequence();

public:
    Shared<CinematicCamera> Ensure_CinematicCamera();

public:
    void Set_TargetCameraEditMode(bool enable) { _isTargetCameraEditMode = enable; }
    bool Is_TargetCameraEditMode() { return _isTargetCameraEditMode;}

public:
    Shared<ClientScreenFxManager> Get_ScreenFxManager() const;
    Shared<UIPreviewManager> Get_PreviewManager() const { return _previewManager; }

public: /* Effect 재생 */
    bool Prewarm_Effect(const wstring& effectName, const wstring& layerTag, uint32 count);

    // 단발성 1회 재생.
    bool Play_Effect(const PlayEffectDesc& desc);

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

public: /* Delegate */
    void Notify_FreeAimShotHit();

private:
	HRESULT Ready_ResourceLoader();
    HRESULT Ready_GameDataTables();

public:
    FOnFreeAimShotHit OnFreeAimShotHit;

private:
	ComPtr<Device>  _device;
	ComPtr<Context> _context;

    atomic<bool> _isStaticResourcesLoaded = false;
    bool         _isStaticObjectsSpawned = false;
	bool         _isInitialized = false;
    bool         _isTargetCameraEditMode = false;

private:
    Shared<CameraDirector>              _cameraDirector;
    Shared<UIPreviewManager>            _previewManager;
    Shared<ResourceLoader>              _resourceLoader;
    Unique<GameDataTable>               _gameDataTable;
    Unique<PreviewShotTable>            _previewShotTable;
    Unique<EffectInstancePool>          _effectInstancePool;
    Unique<ScreenFadeTransition>        _screenFadeTransition;
    Unique<LevelTransitionFade>         _levelTransitionFade;
    Unique<CinematicSequenceRunner>     _cinematicSequenceRunner;

    Shared<GameplayUI>                  _gamePlayUI;
    Unique<PersistentObjectManager>     _persistentObjectManager;
    Unique<Hero_Manager>                _heroManager;
    Shared<ClientScreenFxManager>       _screenFxManager;

    Shared<BattleController>    _battleController;

public:
	void Free() override;

};

NS_END
