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
    enum class DamageLevel { LOW, NORMAL, HIGH, EXTREM };

    struct HealAction
    {
        int healPercent;
    };

    struct GainChargeAction
    {
        int min;
        int max;
    };

    struct DamageAction
    {
        int        hitCount;
        DamageType damageType = DamageType::PHYSICAL;
        uint32     bonusChargeOnCritical;

    };

    struct ShatterAction
    {
        bool   fullChargeOnShatter;
    };

    struct ResetChargeAction
    {
        
    };

    struct ApplyMarkAction
    {
        
    };

    struct ClearStatusAction
    {
        
    };

    struct BuffAction
    {
        uint32 durationTurn;    
    };

    using SkillActionPayLoad = variant<DamageAction, ShatterAction, ApplyMarkAction, ClearStatusAction, ResetChargeAction, GainChargeAction, HealAction>;

    struct SkillAction
    {
        //트리거도 만들어야 하나
        SkillActionPayLoad skillActionPayLoad;
    };

    struct SkillDesc
    {
       wstring             name;
       string              skillId; // 스킬 영어이름
       uint32              apCost;
       vector<SkillAction> skillActions;
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

private:
    HRESULT Load_ExpLevel(const wstring& jsonRootPath);
    HRESULT Load_SkillData(const wstring& jsonRootPath);

private:
    wstring Get_DefaultJsonRootPath() const;
    uint32  Get_FallbackNextExp(uint32 level) const;

private:
    // index == level. 0번은 사용하지 않는다.
    vector<uint32> _nextExpByLevel;
    bool _isLoaded = false;
    map<string, SkillDesc> _skillData;

public:
    static Unique<GameDataTable> Create();
    void Free() override;
};

NS_END
