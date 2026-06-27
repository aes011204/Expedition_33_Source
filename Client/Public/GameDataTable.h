#pragma once

#include <variant>

#include "Client_Defines.h"
#include "Base.h"
#include "Battle_Enum.h"

// 게임 플레이 시에 필요한 정적 테이블 데이터 담당.
// ResourceLoader는 리소스/프로토타입 생성,
// GameDataTable은 경험치, 스킬, 아이템 같은 데이터 담당

NS_BEGIN(Client)

class GameDataTable final : public Base
{
public:

    enum class DamageType  { LIGHTNING, PHYSICAL };
    enum class DamageLevel { LOW, NORMAL, HIGH, EXTREME };
    enum class TargetScope { SINGLE, ALL };
    enum class SkillActionTrigger { OnCritical, OnMark };

    struct HealAction
    {
        int healPercent; //치유 비율
    };

    struct GainChargeAction
    {
        int min; // 스킬 사용시 최소 충전 횟수
        int max; // 스킬 사용시 최대 충전 횟수
    };

    struct DamageAction
    {
        int         hitCount; // 스킬 하나당 공격 횟수
        DamageType  damageType = DamageType::PHYSICAL; //공격 타입
        DamageLevel damageLevel; // 공격 레벨 (세기)
        TargetScope targetScope;
        uint32      bonusChargeOnCritical; // 치명타시 보너스 충전 횟수
    };

    struct ShatterAction
    {
        bool   fullChargeOnShatter; //파괴 스킬 작동시 즉시 충전 여부
    };

    struct ApplyMarkAction
    {

    };

    struct ClearStatusAction
    {

    };

    struct BuffAction
    {
        uint32 durationTurn; // 몇 턴동안 버프가 유지되는가
    };

    using SkillActionPayLoad = variant<DamageAction, ShatterAction, ApplyMarkAction, ClearStatusAction, GainChargeAction, HealAction, BuffAction>;

    struct SkillAction
    {
        SkillActionTrigger skillActionTrigger;
        SkillActionPayLoad skillActionPayLoad; //skillaction을 담는 바구니같은 개념. variant에 있는 구조체를 담을 수 있다.
    };

    //struct SkillDesc
    //{
    //   wstring             name;    //스킬 한글 이름
    //   string              skillId; // 스킬 영어 이름
    //   uint32              apCost;  // ap 소모량
    //   vector<SkillAction> skillActions; // skill 효과 목록, 한 스킬당 여러 효과를 가질 수 있다.
    //};

    struct StainCost
    {
        string stainType;
        uint32 stainCost;
    };

    struct StainConsumeRule
    {
        string skillId;
        vector<StainCost> stainCost;
    };

    struct SkillTreeDesc
    {
        // string      requiredSkillId;  //해금시 필요한 선행 스킬
        uint32 unlockCost;               //해금시 필요한 스킬 포인트
        uint32 useCost = 0;              // Progression/ChangeSkill의 GP 표시
        string  portraitTextureName{};   // 스킬 텍스처
        string  descUiName{};            // desc ui
        wstring skillName{};             // 스킬트리/툴팁 표시명

    };

    struct WeaponDesc
    {
        string  characterId;
        string  weaponId;
        wstring weaponName;
        wstring portraitTexturePath;

        string  weaponPrefabName;       // 기본 무기
        string  subWeaponPrefabName;    // 구스타브 총

        uint32  level = 1;
        uint32  power = 0;
    };

    struct WeaponUpgradeDesc
    {
        string characterId;
        string weaponId;
        uint32 level = 1;
        uint32 power = 0;
        uint32 requiredChromaCatalyst = 0;
        string vitalityGrade{};
        string mightGrade{};
        string agilityGrade{};
        string defenseGrade{};
        string luckGrade{};
    };

    enum class PictosEffectType
    {
        None,
        EnergisingTurn,     // 자기 턴 시작 AP 추가 회복
        GlassCannon,        // 주는 피해/받는 피해 증가
        WarmingUp,          // 자기 턴마다 주는 피해 누적 증가
        Roulette,           // 자기 턴마다 50% 또는 150% 피해 배율 결정
        AugmentedAttack,    // 기본 공격 피해 증가
        AugmentedAim,       // 자유 조준 피해 증가
    };

    struct PictosDesc
    {
        string  id{};
        wstring name{};
        int32   hp = 0;
        int32   speed = 0;
        int32   critical = 0;
        string  buttonUi{};   // UI_Pictos_EnergisingTurn
        string  tooltipUi{};  // UI_EnergisingTurn_Pictos_Tooltip

        PictosEffectType effectType = PictosEffectType::None;
        float effectValue = 0.f;
    };

public:
    GameDataTable() = default;
    ~GameDataTable() override = default;

public:
    HRESULT Initialize();
    HRESULT Load_All();
    void Clear();

    const Skill_Data* Get_Skill(const string& skillId);
    vector<TargetType> ParseTargetTypes(const string& data);
    const Item_Data* Get_Item(const string& itemId);
    const Item_Data* Get_Item(BattleItemType itemType);

    bool Consume_Item(BattleItemType itemType);

    HRESULT Load_SkillPhaseData(const wstring& jsonRootPath);
public:
    uint32 Get_NextExp(uint32 level) const;
    SkillTreeDesc* Get_SkillTreeDesc(const string& skillName);
    const umap<string, vector<string>>* Get_SkillTreeRequirementData();
    const WeaponDesc* Get_WeaponDesc(const string& characterId, const string& weaponId) const;
    const vector<WeaponDesc>* Get_WeaponDescs(const string& characterId) const;
    const WeaponUpgradeDesc* Get_WeaponUpgradeDesc(const string& characterId, const string& weaponId, uint32 level) const;
    const WeaponUpgradeDesc* Get_NextWeaponUpgradeDesc(const string& characterId, const string& weaponId, uint32 currentLevel) const;
    const vector<WeaponUpgradeDesc>* Get_WeaponUpgradeDescs(const string& characterId, const string& weaponId) const;
    vector<string> Get_AllSkillIds() const;
    vector<string> Split_String(const string& value, char delimiter);

    const PictosDesc* Get_PictosDesc(const string& pictosId) const;
    const vector<PictosDesc>& Get_PictosDescs() const { return _pictosDefs; }
    vector<string> Get_AllPictosIds() const;

private:
    HRESULT Load_ExpLevel(const wstring& jsonRootPath);
    HRESULT Load_SkillActionData(const wstring& jsonRootPath);
    HRESULT Load_SkillTreeData(const wstring& jsonRootPath);
    HRESULT Load_SkillTreeRequirementData(const wstring& jsonRootPath);
    HRESULT Load_Weapon(const wstring& jsonRootPath);
    HRESULT Load_WeaponUpgrade(const wstring& jsonRootPath);

    // 스킬
    HRESULT Load_SkillData(const wstring& jsonRootPath);

    // 아이템
    HRESULT Load_ItemData(const wstring& jsonRootPath);

    // 픽토스
    HRESULT Load_PictosData(const wstring& jsonRootPath);

private:
    wstring         Get_DefaultJsonRootPath() const;
    uint32          Get_FallbackNextExp(uint32 level) const;

private:
    // index == level. 0번은 사용하지 않는다.
    vector<uint32>              _nextExpByLevel;
    bool                        _isLoaded = false;
    map<string, SkillTreeDesc>  _skillTreeData;
    umap<string, vector<string>> _requirementSkills;
    umap<string, vector<WeaponDesc>> _weaponDataByCharacter;
    umap<string, vector<WeaponUpgradeDesc>> _weaponUpgradeDataByKey;

    // 스킬
    vector<Skill_Data> _skillDefs;
    unordered_map<string, size_t> _skillIndexById;

    // 아이템 -> 틴트인데.. 이거 쓸모있나
    vector<Item_Data> _itemDefs;
    umap<string, size_t> _itemIndexById;

    // 픽토스
    vector<PictosDesc> _pictosDefs{};
    umap<string, size_t> _pictosIndexById{};

public:
    static Unique<GameDataTable> Create();
    void Free() override;
};

NS_END
