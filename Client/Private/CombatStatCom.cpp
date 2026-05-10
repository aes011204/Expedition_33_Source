#include "CombatStatCom.h"
#include "ClientInstance.h"

IMPLEMENT_REFLECTION(CombatStatCom)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.className = "CombatStat";
    info.displayName = "Combat Stat";

    PROPERTY_INT("Level", _level, 1.f, 99.f, 1.f);
    PROPERTY_INT("Exp", _exp, 0.f, 999999.f, 1.f);

    PROPERTY_INT("Stat Point", _statPoint, 0.f, 999.f, 1.f);
    PROPERTY_INT("Skill Point", _skillPoint, 0.f, 999.f, 1.f);
    PROPERTY_INT("Respec Count", _respecCount, 0.f, 99.f, 1.f);

    PROPERTY_INT("Vitality", _vitality, 0.f, 99.f, 1.f);
    PROPERTY_INT("Might", _might, 0.f, 99.f, 1.f);
    PROPERTY_INT("Agility", _agility, 0.f, 99.f, 1.f);
    PROPERTY_INT("Defense", _defenseStat, 0.f, 99.f, 1.f);
    PROPERTY_INT("Luck", _luck, 0.f, 99.f, 1.f);

    return true;
}

CombatStatCom::CombatStatCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component(device, context)
{
}

CombatStatCom::CombatStatCom(const CombatStatCom& Prototype)
     : Component{ Prototype }
    , _level{ Prototype._level }
    , _exp{ Prototype._exp }
    , _statPoint{ Prototype._statPoint }
    , _skillPoint{ Prototype._skillPoint }
    , _respecCount{ Prototype._respecCount }
    , _vitality{ Prototype._vitality }
    , _might{ Prototype._might }
    , _agility{ Prototype._agility }
    , _defenseStat{ Prototype._defenseStat }
    , _luck{ Prototype._luck }
    , _maxHp{ Prototype._maxHp }
    , _hp{ Prototype._hp }
    , _attack{ Prototype._attack }
    , _defense{ Prototype._defense }
    , _speed{ Prototype._speed }
    , _criticalRate{ Prototype._criticalRate }
    , _statInfo{ Prototype._statInfo }

{
}

HRESULT CombatStatCom::Initialize_Prototype()
{
    return Component::Initialize_Prototype();
}

HRESULT CombatStatCom::Initialize(void* arg)
{
    CHECK_FAILED(Component::Initialize(arg), E_FAIL);

    if (arg != nullptr)
    {
        const CombatStatDesc& desc = *static_cast<CombatStatDesc*>(arg);

        _level = max(1u, desc.level);
        _exp = desc.exp;
        _statPoint = desc.statPoint;
        _skillPoint = desc.skillPoint;
        _respecCount = desc.respecCount;

        _vitality = desc.vitality;
        _might = desc.might;
        _agility = desc.agility;
        _defenseStat = desc.defense;
        _luck = desc.luck;
    }
    else
    {
        _level = 1;
        _exp = 0;
        _statPoint = 0;
        _skillPoint = 0;
        _respecCount = 1;

        _vitality = 0;
        _might = 0;
        _agility = 0;
        _defenseStat = 0;
        _luck = 0;
    }

    Recalculate_Stats();

    _hp = _maxHp;

    Update_StatInfo();

    return S_OK;
}

bool CombatStatCom::Add_Stat(StatType type, int32 value)
{
    if (value <= 0)
        return false;

    if (_statPoint < static_cast<uint32>(value))
        return false;

    int32* stat = Find_StatValue(type);
    if (stat == nullptr)
        return false;

    const float hpRate = (_maxHp > 0.f) ? (_hp / _maxHp) : 1.f;

    *stat += value;
    _statPoint -= static_cast<uint32>(value);

    Recalculate_Stats();
    _hp = _maxHp * hpRate;
    Update_StatInfo();

    return true;
}

bool CombatStatCom::Reset_Stat()
{
    if (_respecCount == 0)
        return false;

    uint32 usedPoint = 0;

    for (int32* stat : Get_StatValues())
    {
        usedPoint += static_cast<uint32>(max(0, *stat));
        *stat = 0;
    }

    _statPoint += usedPoint;
    --_respecCount;

    Recalculate_Stats();
    _hp = min(_hp, _maxHp);
    Update_StatInfo();

    return true;
}

void CombatStatCom::Add_Exp(uint32 value)
{
    // 만렙
    if (_level >= 99)
    {
        _exp = 0;
        Update_StatInfo();
        return;
    }

    _exp += value;

    while (_level < 99)
    {
        const uint32 nextExp = Get_NextExp(_level);
        if (_exp < nextExp)
            break;

        _exp -= nextExp;
        ++_level;
        ++_statPoint;
        ++_skillPoint;
    }

    if (_level >= 99)
        _exp = 0;

    const float hpRate = (_maxHp > 0.f) ? (_hp / _maxHp) : 1.f;
    Recalculate_Stats();
    _hp = _maxHp * hpRate;
    Update_StatInfo();

}

uint32 CombatStatCom::Get_NextExp(uint32 level) const
{
    return CLIENT->Get_NextExp(level);
}

int32* CombatStatCom::Find_StatValue(StatType type)
{
    switch (type)
    {
    case StatType::Vitality: return &_vitality;
    case StatType::Might:    return &_might;
    case StatType::Agility:  return &_agility;
    case StatType::Defense:  return &_defenseStat;
    case StatType::Luck:     return &_luck;
    default:                 return nullptr;
    }
}

array<int32*, static_cast<size_t>(StatType::END)> CombatStatCom::Get_StatValues()
{
    return { &_vitality, &_might, &_agility, &_defenseStat, &_luck };
}

void CombatStatCom::Recalculate_Stats()
{
    // 33 원정대 데이터 공식 일단 유지해서 해봄
    _maxHp = 100.f + static_cast<float>(_vitality) * 10.f;
    _attack = 10.f + static_cast<float>(_might) * 2.f;
    _defense = static_cast<float>(_defenseStat) * 2.f;
    _speed = 100.f + static_cast<float>(_agility) * 2.f;
    _criticalRate = min(100.f, static_cast<float>(_luck) * 0.5f);

    // 최대 체력이 줄어든 경우 현재 체력이 최대 체력을 넘지 않게 보정
    _hp = min(_hp, _maxHp);
}

void CombatStatCom::Update_StatInfo()
{
    _statInfo.level = _level;
    _statInfo.exp = _exp;
    _statInfo.nextExp = Get_NextExp(_level);
    _statInfo.statPoint = _statPoint;
    _statInfo.skillPoint = _skillPoint;
    _statInfo.respecCount = _respecCount;

    _statInfo.vitality = _vitality;
    _statInfo.might = _might;
    _statInfo.agility = _agility;
    _statInfo.defense = _defenseStat;
    _statInfo.luck = _luck;

    _statInfo.maxHp = _maxHp;
    _statInfo.hp = _hp;
    _statInfo.attack = _attack;
    _statInfo.defenseValue = _defense;
    _statInfo.speed = _speed;
    _statInfo.criticalRate = _criticalRate;
}

Shared<CombatStatCom> CombatStatCom::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<CombatStatCom>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : CombatStatCom");
        return nullptr;
    }

    return instance;
}

Shared<Component> CombatStatCom::Clone(void* arg)
{
    auto clone = make_shared<CombatStatCom>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : CombatStatCom");
        return nullptr;
    }

    return clone;
}

void CombatStatCom::Free()
{
    Component::Free();
}
