#pragma once

#include <variant>

#include "Client_Defines.h"
#include "Base.h"

// 게임 플레이 시에 필요한 정적 테이블 데이터 담당.
// ResourceLoader는 리소스/프로토타입 생성,
// GameDataTable은 경험치, 스킬, 아이템 같은 데이터 담당

NS_BEGIN(Client)

class GameDataTable final : public Base
{
public:

    enum class DamageType { LIGHTNING, PHYSICAL};
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

    struct SkillDesc
    {
       wstring             name;    //스킬 한글 이름
       string              skillId; // 스킬 영어 이름
       uint32              apCost;  // ap 소모량
       vector<SkillAction> skillActions; // skill 효과 목록, 한 스킬당 여러 효과를 가질 수 있다.
    };

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
        string      requiredSkillId; //해금시 필요한 선행 스킬
        uint32      unlockCost; //해금시 필요한 스킬 포인트
    };

public:
    GameDataTable() = default;
    ~GameDataTable() override = default;

public:
    HRESULT Initialize();
    HRESULT Load_All();
    void Clear();

public:
    uint32 Get_NextExp(uint32 level) const;
    SkillTreeDesc*   Get_SkillTreeDesc(const string& skillName);

private:
    HRESULT Load_ExpLevel(const wstring& jsonRootPath);
    HRESULT Load_SkillData(const wstring& jsonRootPath);
    HRESULT Load_SkillActionData(const wstring& jsonRootPath);
    HRESULT Load_SkillTreeData(const wstring& jsonRootPath);

private:
    wstring         Get_DefaultJsonRootPath() const;
    uint32          Get_FallbackNextExp(uint32 level) const;

private:
    // index == level. 0번은 사용하지 않는다.
    vector<uint32>              _nextExpByLevel;
    bool                        _isLoaded = false;
    map<string, SkillDesc>      _skillData;
    map<string, SkillTreeDesc>  _skillTreeData;

public:
    static Unique<GameDataTable> Create();
    void Free() override;
};

NS_END
