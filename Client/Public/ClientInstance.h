#pragma once
#include "Base.h"
#include "GameDataTable.h"
#include "Delegate.h"
#include "PreviewShotTable.h"
#include "UIPreviewManager.h"

NS_BEGIN(Engine)
    class GameInstance;
NS_END

NS_BEGIN(Client)
class BattleController;
class Player_WorldBase;
class TargetCamera;
class ResourceLoader;
class GameplayUI;
class PersistentObjectManager;
class Hero_Manager;

class ClientInstance : public Base
{
    DECLARE_SINGLETON(ClientInstance)

public:
	ClientInstance();
	virtual ~ClientInstance() = default;

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

    bool    Is_StaticResourcesLoaded() const { return _isStaticResourcesLoaded.load(); }
    void    Set_StaticResourcesLoaded(bool isLoaded) { _isStaticResourcesLoaded.store(isLoaded); }

public: /* 플레이어 찾기 -> 일단 임시 */
    Shared<GameObject> Find_Player(uint32 levelIndex);

public: /* For.GameData Table */
    uint32        Get_NextExp(uint32 level) const;
    GameDataTable::SkillTreeDesc* Get_SkillTreeDesc(const string& skillName);
    const umap<string, vector<string>>* Get_SkillTreeRequirementData();

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

public: /* For.Gameplay UI */
    HRESULT Initialize_GameplayUI(uint32 levelIndex);
    void    Clear_GameplayUI();

public: /* For.Battle_Controller */
    void OnEnterBattleField(
        uint32 FieldID,
        bool IsFirstHero,
        const vector<HeroId>& heroIds,
        vector<string>& MonsterProtoTag);

    Shared<BattleController>    Get_BattleController() { return _battleController; };

    bool Is_BattleActive() const;
    bool Is_BattleEntering() const;

private:
	HRESULT Ready_ResourceLoader();
    HRESULT Ready_GameDataTables();

private:
	ComPtr<Device>  _device;
	ComPtr<Context> _context;

    atomic<bool> _isStaticResourcesLoaded = false;
    bool _isStaticObjectsSpawned = false;
	bool _isInitialized = false;

private:
    Shared<UIPreviewManager>    _previewManager;
    Shared<ResourceLoader>      _resourceLoader;
    Unique<GameDataTable>       _gameDataTable;
    Unique<PreviewShotTable>    _previewShotTable;

    Unique<GameplayUI>          _gamePlayUI;
    Unique<PersistentObjectManager> _persistentObjectManager;
    Unique<Hero_Manager>        _heroManager;

    Shared<BattleController>    _battleController;

public:
	void Free() override;

};

NS_END
