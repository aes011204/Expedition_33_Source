#include "PlayerStatCom.h"

#include <GameObject.h>
#include "ClientInstance.h"

IMPLEMENT_REFLECTION(PlayerStatCom)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.className = "PlayerCombatStat";
    info.displayName = "PlayerCombat Stat";

     // 캐릭터별 초기 세팅값을 프리팹에서 직접 주기 위해
    PROPERTY_INT("Level", _playerDesc.level, 1.f, 99.f, 1.f);
    PROPERTY_INT("Exp", _playerDesc.exp, 0.f, 999999.f, 1.f);

    PROPERTY_INT("Stat Point", _playerDesc.statPoint, 0.f, 999.f, 1.f);
    PROPERTY_INT("Skill Point", _playerDesc.skillPoint, 0.f, 999.f, 1.f);
    PROPERTY_INT("Respec Count", _playerDesc.respecCount, 0.f, 99.f, 1.f);

    PROPERTY_INT("Vitality", _playerDesc.vitality, 0.f, 99.f, 1.f);
    PROPERTY_INT("Might", _playerDesc.might, 0.f, 99.f, 1.f);
    PROPERTY_INT("Agility", _playerDesc.agility, 0.f, 99.f, 1.f);
    PROPERTY_INT("Defense", _playerDesc.defense, 0.f, 99.f, 1.f);
    PROPERTY_INT("Luck", _playerDesc.luck, 0.f, 99.f, 1.f);

    // 필요하면 테스트용으로 기본 전투값도 프리팹에서 분리 가능
    PROPERTY_FLOAT("Base Max HP", _playerDesc.maxHp, 0.f, 9999.f, 1.f);
    PROPERTY_INT("Base Max AP", _playerDesc.maxAp, 0.f, 99.f, 1.f);

    return true;
}

PlayerStatCom::PlayerStatCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : CombatStatCom(device, context)
{
}

PlayerStatCom::PlayerStatCom(const PlayerStatCom& Prototype)
    : CombatStatCom(Prototype)
    , _playerDesc(Prototype._playerDesc)
    , _PRuntimeStat(Prototype._PRuntimeStat)
{
}

HRESULT PlayerStatCom::Initialize_Prototype()
{
    return CombatStatCom::Initialize_Prototype();
}

HRESULT PlayerStatCom::Initialize(void* arg)
{
    CHECK_FAILED(CombatStatCom::Initialize(arg), E_FAIL);

    PlayerCombatStatDesc desc{};
    _PRuntimeStat = {};

    if (arg != nullptr)
    {
        _playerDesc = *static_cast<PlayerCombatStatDesc*>(arg);
    }
    else
    {
        _playerDesc = desc;
    }

    // desc -> runtime 초기 반영
    _PRuntimeStat.CurStatPoint = _playerDesc.statPoint;
    _PRuntimeStat.CurSkillPoint = _playerDesc.skillPoint;
    _PRuntimeStat.CurRespecCount = _playerDesc.respecCount;

    _PRuntimeStat.CurVitality = _playerDesc.vitality;
    _PRuntimeStat.CurMight = _playerDesc.might;
    _PRuntimeStat.CurAgility = _playerDesc.agility;
    _PRuntimeStat.CurDefense = _playerDesc.defense;
    _PRuntimeStat.CurLuck = _playerDesc.luck;

    _PRuntimeStat.CurLevel = _playerDesc.level;
    _PRuntimeStat.CurExp = _playerDesc.exp;
    _PRuntimeStat.CurChroma = _playerDesc.chroma;

    _PRuntimeStat.CurMaxHp = _playerDesc.maxHp;
    _PRuntimeStat.CurMaxAp = _playerDesc.maxAp;

    Recalculate_Stats(); // 활력/무력/민첩/방어/행운으로 재계산
    Update_StatInfo();   // UI가 읽는 CurMaxHp, CurAttack, CurSpeed 값 갱신용

    _isPlayer = true;
   

    return S_OK;
}

void PlayerStatCom::From_Json(const json& data)
{
    CombatStatCom::From_Json(data);

    _PRuntimeStat.CurStatPoint = _playerDesc.statPoint;
    _PRuntimeStat.CurSkillPoint = _playerDesc.skillPoint;
    _PRuntimeStat.CurRespecCount = _playerDesc.respecCount;

    _PRuntimeStat.CurVitality = _playerDesc.vitality;
    _PRuntimeStat.CurMight = _playerDesc.might;
    _PRuntimeStat.CurAgility = _playerDesc.agility;
    _PRuntimeStat.CurDefense = _playerDesc.defense;
    _PRuntimeStat.CurLuck = _playerDesc.luck;

    _PRuntimeStat.CurLevel = _playerDesc.level;
    _PRuntimeStat.CurExp = _playerDesc.exp;
    _PRuntimeStat.CurChroma = _playerDesc.chroma;

    _PRuntimeStat.CurHp = _playerDesc.maxHp; // 테스트는 캐릭터별 풀피 상태로 통일
    _PRuntimeStat.CurAp = _playerDesc.maxAp;
    _PRuntimeStat.CurShield = _playerDesc.commonCharacterStats.ShieldPointsAtSpawn;

    Recalculate_Stats(); // desc 기준 파생 스탯 확정
    Update_StatInfo();   // CharacterStatus가 읽는 값 최종 반영


    

   


}

bool PlayerStatCom::Add_Stat(StatType type, int32 value)
{
    if (value <= 0)
        return false;

    if (_PRuntimeStat.CurStatPoint < static_cast<uint32>(value))
        return false;

    int32* stat = Find_StatValue(type);
    if (stat == nullptr)
        return false;

    const float hpRate = (_playerDesc.maxHp > 0.f) ? (_PRuntimeStat.CurHp / _playerDesc.maxHp) : 1.f;

    *stat += value;
    _PRuntimeStat.CurStatPoint -= static_cast<uint32>(value);

    Recalculate_Stats();
    _PRuntimeStat.CurHp = _playerDesc.maxHp * hpRate;
    _PRuntimeStat.CurHp = _PRuntimeStat.CurHp;
    Update_StatInfo();

    return true;
}

bool PlayerStatCom::Reset_Stat()
{
    if (_PRuntimeStat.CurRespecCount == 0)
        return false;

    uint32 usedPoint = 0;

    for (int32* stat : Get_StatValues())
    {
        usedPoint += static_cast<uint32>(max(0, *stat));
        *stat = 0;
    }

    _PRuntimeStat.CurStatPoint += usedPoint;
    --_PRuntimeStat.CurRespecCount;

    Recalculate_Stats();
    _PRuntimeStat.CurHp = min(_PRuntimeStat.CurHp, _playerDesc.maxHp);
    _PRuntimeStat.CurHp = _PRuntimeStat.CurHp;
    Update_StatInfo();

    return true;
}




uint32 PlayerStatCom::Get_NextExp(uint32 level) const
{
    return CLIENT->Get_NextExp(level);
}

void PlayerStatCom::Add_Exp(uint32 value)
{
    // 만렙
    if (_playerDesc.level >= 99)
    {
        _playerDesc.exp = 0;
        Update_StatInfo();
        return;
    }

    _playerDesc.exp += value;

    while (_playerDesc.level < 99)
    {
        const uint32 nextExp = Get_NextExp(_playerDesc.level);
        if (_playerDesc.exp < nextExp)
            break;

        _playerDesc.exp -= nextExp;
        ++_playerDesc.level;
        ++_PRuntimeStat.CurStatPoint;
        ++_PRuntimeStat.CurSkillPoint;
    }

    if (_playerDesc.level >= 99)
        _playerDesc.exp = 0;

    const float hpRate = (_playerDesc.maxHp > 0.f) ? (_PRuntimeStat.CurHp / _playerDesc .maxHp) : 1.f;

    Recalculate_Stats();

    _PRuntimeStat.CurHp = _playerDesc.maxHp * hpRate;
    //_PRuntimeStat.CurHp = _PRuntimeStat.CurHp;

    Update_StatInfo();

}

PlayerStatCom::PreviewStat PlayerStatCom::Calculate_PreviewStat(int32 vitality, int32 might, int32 agility,
    int32 defense, int32 luck)
{
    PreviewStat preview{};
    preview.maxHp = 100.f + static_cast<float>(vitality) * 10.f;                  // 기존 HP 공식
    preview.physicalAttack = 10.f + static_cast<float>(might) * 2.f;              // 기존 공격력 공식
    preview.speed = 100.f + static_cast<float>(agility) * 2.f;                    // 기존 속도 공식
    preview.physicalDefense = static_cast<float>(defense) * 2.f;                  // 기존 방어 공식
    preview.criticalRate = min(100.f, static_cast<float>(luck) * 0.5f); // 기존 치명타율 공식

    return preview;
}

int32* PlayerStatCom::Find_StatValue(StatType type)
{
    switch (type)
    {
    case StatType::Vitality: return &_PRuntimeStat.CurVitality;
    case StatType::Might:    return &_PRuntimeStat.CurMight;
    case StatType::Agility:  return &_PRuntimeStat.CurAgility;
    case StatType::Defense:  return &_PRuntimeStat.CurDefense;
    case StatType::Luck:     return &_PRuntimeStat.CurLuck;
    default:                 return nullptr;
    }
}

array<int32*, static_cast<size_t>(StatType::END)> PlayerStatCom::Get_StatValues()
{
    return { &_PRuntimeStat.CurVitality, &_PRuntimeStat.CurMight, &_PRuntimeStat.CurAgility, &_PRuntimeStat.CurDefense, &_PRuntimeStat.CurLuck };
}

void PlayerStatCom::Recalculate_Stats()
{
    const PreviewStat preview = Calculate_PreviewStat(
        _PRuntimeStat.CurVitality,
        _PRuntimeStat.CurMight,
        _PRuntimeStat.CurAgility,
        _PRuntimeStat.CurDefense,
        _PRuntimeStat.CurLuck);

    // 33 원정대 데이터 공식 일단 유지해서 해봄
    _playerDesc.maxHp = preview.maxHp;
    _playerDesc.physicalAttack = preview.physicalAttack;
    _playerDesc.physicalDefense = preview.physicalDefense;
    _playerDesc.speed = preview.speed;
    _playerDesc.criticalRate = preview.criticalRate;

    _PRuntimeStat.CurHp = min(_PRuntimeStat.CurHp, _playerDesc.maxHp);
}

void PlayerStatCom::Update_StatInfo()
{
    _PRuntimeStat.CurLevel = _playerDesc.level;
    _PRuntimeStat.CurExp = _playerDesc.exp;
    _PRuntimeStat.CurChroma = _playerDesc.chroma;
    _PRuntimeStat.nextExp = Get_NextExp(_playerDesc.level);

    _PRuntimeStat.CurMaxHp = _playerDesc.maxHp;
    _PRuntimeStat.CurMaxAp = _playerDesc.maxAp;
    _PRuntimeStat.CurPhysicalAttack = _playerDesc.physicalAttack;
    _PRuntimeStat.CurPhysicalDefense = _playerDesc.physicalDefense;
    _PRuntimeStat.CurAccuracy = _playerDesc.accuracy;
    _PRuntimeStat.CurSpeed = _playerDesc.speed;
    _PRuntimeStat.CurCriticalRate = _playerDesc.criticalRate;
    _PRuntimeStat.CurMastery = _playerDesc.mastery;
    _PRuntimeStat.CurCriticalChance = _playerDesc.criticalChance;

    _PRuntimeStat.CurHp = _PRuntimeStat.CurHp;
    _PRuntimeStat.CurAp = _PRuntimeStat.CurAp;
    _PRuntimeStat.CurShield = _PRuntimeStat.CurShield;
}

Shared<PlayerStatCom> PlayerStatCom::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<PlayerStatCom>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : PlayerStatCom");
        return nullptr;
    }

    return instance;
}

Shared<Component> PlayerStatCom::Clone(void* arg)
{
    auto clone = make_shared<PlayerStatCom>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : PlayerStatCom");
        return nullptr;
    }

    return clone;
}

void PlayerStatCom::Free()
{
    CombatStatCom::Free();
}
