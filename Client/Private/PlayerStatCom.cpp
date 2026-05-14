#include "PlayerStatCom.h"
#include "ClientInstance.h"

IMPLEMENT_REFLECTION(PlayerStatCom)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.className = "PlayerCombatStat";
    info.displayName = "PlayerCombat Stat";

    PROPERTY_INT("Stat Point", _PRuntimeStat.CurStatPoint, 0.f, 999.f, 1.f);
    PROPERTY_INT("Skill Point", _PRuntimeStat.CurSkillPoint, 0.f, 999.f, 1.f);
    PROPERTY_INT("Respec Count", _PRuntimeStat.CurRespecCount, 0.f, 99.f, 1.f);

    PROPERTY_INT("Vitality", _PRuntimeStat.CurVitality, 0.f, 99.f, 1.f);
    PROPERTY_INT("Might", _PRuntimeStat.CurMight, 0.f, 99.f, 1.f);
    PROPERTY_INT("Agility", _PRuntimeStat.CurAgility, 0.f, 99.f, 1.f);
    PROPERTY_INT("Defense", _PRuntimeStat.CurDefense, 0.f, 99.f, 1.f);
    PROPERTY_INT("Luck", _PRuntimeStat.CurLuck, 0.f, 99.f, 1.f);

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

    _PRuntimeStat.CurStatPoint = _playerDesc.statPoint;
    _PRuntimeStat.CurSkillPoint = _playerDesc.skillPoint;
    _PRuntimeStat.CurRespecCount = _playerDesc.respecCount;

    _PRuntimeStat.CurVitality = _playerDesc.vitality;
    _PRuntimeStat.CurMight = _playerDesc.might;
    _PRuntimeStat.CurAgility = _playerDesc.agility;
    _PRuntimeStat.CurDefense = _playerDesc.defense;
    _PRuntimeStat.CurLuck = _playerDesc.luck;

    _PRuntimeStat.CurLevel = _statDesc.level;
    _PRuntimeStat.CurExp = _statDesc.exp;
    _PRuntimeStat.CurChroma = _statDesc.chroma;

    _PRuntimeStat.CurMaxHp = _statDesc.maxHp;
    _PRuntimeStat.CurMaxAp = _statDesc.maxAp;
   
    _PRuntimeStat.CurAccuracy = _statDesc.accuracy;
    _PRuntimeStat.CurMastery = _statDesc.mastery;
    _PRuntimeStat.CurCriticalRate = _statDesc.criticalRate;
    _PRuntimeStat.CurCriticalChance = _statDesc.criticalRate;

    //_PRuntimeStat.CurShield = _runtimeStat.CurShield;


    /*  float CurHp = {};
        uint32 CurShield = {};
        uint32 CurAp = {};

        float physicalAttack = 0.f;
        float physicalDefense = 0.f;

        float CurSpeed = 0.f;
        이거 뺴고 다 */

    Recalculate_Stats();

   // _PRuntimeStat.CurHp = _runtimeStat.CurHp;
   // _PRuntimeStat.CurAp = _runtimeStat.CurAp;

    Update_StatInfo();

    _isPlayer = true;

    return S_OK;
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

    const float hpRate = (_statDesc.maxHp > 0.f) ? (_runtimeStat.CurHp / _statDesc.maxHp) : 1.f;

    *stat += value;
    _PRuntimeStat.CurStatPoint -= static_cast<uint32>(value);

    Recalculate_Stats();
    _runtimeStat.CurHp = _statDesc.maxHp * hpRate;
    _PRuntimeStat.CurHp = _runtimeStat.CurHp;
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
    _runtimeStat.CurHp = min(_runtimeStat.CurHp, _statDesc.maxHp);
    _PRuntimeStat.CurHp = _runtimeStat.CurHp;
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
    if (_statDesc.level >= 99)
    {
        _statDesc.exp = 0;
        Update_StatInfo();
        return;
    }

    _statDesc.exp += value;

    while (_statDesc.level < 99)
    {
        const uint32 nextExp = Get_NextExp(_statDesc.level);
        if (_statDesc.exp < nextExp)
            break;

        _statDesc.exp -= nextExp;
        ++_statDesc.level;
        ++_PRuntimeStat.CurStatPoint;
        ++_PRuntimeStat.CurSkillPoint;
    }

    if (_statDesc.level >= 99)
        _statDesc.exp = 0;

    const float hpRate = (_statDesc.maxHp > 0.f) ? (_runtimeStat.CurHp / _statDesc.maxHp) : 1.f;

    Recalculate_Stats();

    _runtimeStat.CurHp = _statDesc.maxHp * hpRate;
    _PRuntimeStat.CurHp = _runtimeStat.CurHp;

    Update_StatInfo();

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
    // 33 원정대 데이터 공식 일단 유지해서 해봄
    _statDesc.maxHp = 100.f + static_cast<float>(_PRuntimeStat.CurVitality) * 10.f;
    _statDesc.physicalAttack = 10.f + static_cast<float>(_PRuntimeStat.CurMight) * 2.f;
    _statDesc.physicalDefense = static_cast<float>(_PRuntimeStat.CurDefense) * 2.f;
    _statDesc.speed = 100.f + static_cast<float>(_PRuntimeStat.CurAgility) * 2.f;
    _statDesc.criticalRate = min(100.f, static_cast<float>(_PRuntimeStat.CurLuck) * 0.5f);

    // 최대 체력이 줄어든 경우 현재 체력이 최대 체력을 넘지 않게 보정
    _runtimeStat.CurHp = min(_runtimeStat.CurHp, _statDesc.maxHp);
}

void PlayerStatCom::Update_StatInfo()
{
    _PRuntimeStat.CurLevel = _statDesc.level;
    _PRuntimeStat.CurExp = _statDesc.exp;
    _PRuntimeStat.CurChroma = _statDesc.chroma;
    _PRuntimeStat.nextExp = Get_NextExp(_statDesc.level);

    _PRuntimeStat.CurMaxHp = _statDesc.maxHp;
    _PRuntimeStat.CurMaxAp = _statDesc.maxAp;
    _PRuntimeStat.CurPhysicalAttack = _statDesc.physicalAttack;
    _PRuntimeStat.CurPhysicalDefense = _statDesc.physicalDefense;
    _PRuntimeStat.CurAccuracy = _statDesc.accuracy;
    _PRuntimeStat.CurSpeed = _statDesc.speed;
    _PRuntimeStat.CurCriticalRate = _statDesc.criticalRate;
    _PRuntimeStat.CurMastery = _statDesc.mastery;
    _PRuntimeStat.CurCriticalChance = _statDesc.criticalChance;

    _PRuntimeStat.CurHp = _runtimeStat.CurHp;
    _PRuntimeStat.CurAp = _runtimeStat.CurAp;
    _PRuntimeStat.CurShield = _runtimeStat.CurShield;
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
