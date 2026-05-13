#include "CombatStatCom.h"
#include "ClientInstance.h"

IMPLEMENT_REFLECTION(CombatStatCom)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.className = "CombatStat";
    info.displayName = "Combat Stat";

    PROPERTY_INT("Level", _statDesc.level, 1.f, 99.f, 1.f);
    PROPERTY_INT("Exp", _statDesc.exp, 0.f, 999999.f, 1.f);

 
    return true;
}

CombatStatCom::CombatStatCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component(device, context)
{
}

CombatStatCom::CombatStatCom(const CombatStatCom& Prototype)
    : Component{ Prototype }
    , _statDesc{ Prototype._statDesc }
    , _runtimeStat{ Prototype._runtimeStat }

{
}

HRESULT CombatStatCom::Initialize_Prototype()
{
    return Component::Initialize_Prototype();
}

HRESULT CombatStatCom::Initialize(void* arg)
{
    CHECK_FAILED(Component::Initialize(arg), E_FAIL);

    CombatStatDesc desc{};

    if (arg != nullptr)
    {
        desc = *static_cast<CombatStatDesc*>(arg);
    }
    _statDesc = desc;

  
  
    _runtimeStat.CurHp = _statDesc.maxHp;
    _runtimeStat.CurAp = _statDesc.maxAp;
    _runtimeStat.CurShield = _statDesc.commonCharacterStats.ShieldPointsAtSpawn;


    return S_OK;
}


void CombatStatCom::ApplyDamage(float damage)\
{
    Recalculate_Stats();

    _hp = _maxHp;

    Update_StatInfo();
        
    return S_OK;
}

json CombatStatCom::To_Json() const
{
    return Component::To_Json();
}

void CombatStatCom::From_Json(const json& data)
{
    Component::From_Json(data);

    Recalculate_Stats();
    _hp = min(_hp, _maxHp);
    Update_StatInfo();
}

bool CombatStatCom::Add_Stat(StatType type, int32 value)
{
    _runtimeStat.CurHp -= damage;

    if (_runtimeStat.CurHp < 0.f)
    {
        _runtimeStat.CurHp = 0.f;
    }
}

void CombatStatCom::Heal(float amount)
{
    _runtimeStat.CurHp += amount;

    if (_runtimeStat.CurHp > _statDesc.maxHp)
    {
        _runtimeStat.CurHp = _statDesc.maxHp;
    }
}

bool CombatStatCom::IsDead() const
{
    return _runtimeStat.CurHp <= 0.f;
}

//void CombatStatCom::Add_Ailment(
//    const Shared<Buff>& ailment)
//{
//    _ailments.push_back(ailment);
//}
//
//void CombatStatCom::Remove_Ailment(
//    const Shared<Buff>& ailment)
//{
//    auto iter = std::find(
//        _ailments.begin(),
//        _ailments.end(),
//        ailment);
//
//    if (iter != _ailments.end())
//    {
//        _ailments.erase(iter);
//    }
//}
//
//void CombatStatCom::Clear_Ailments()
//{
//    _ailments.clear();
//}

void CombatStatCom::Add_Shield(int32 value)
{
    _runtimeStat.CurShield += value;
}

void CombatStatCom::Remove_Shield(int32 value)
{
    _runtimeStat.CurShield -= value;

    if (_runtimeStat.CurShield < 0)
    {
        _runtimeStat.CurShield = 0;
    }
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
