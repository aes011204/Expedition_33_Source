#include "CombatStatCom.h"
#include "GameObject.h"
#include "ClientInstance.h"

CombatStatCom::CombatStatCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component(device, context)
{
}

CombatStatCom::CombatStatCom(const CombatStatCom& Prototype)
    : Component{ Prototype }


{
}

HRESULT CombatStatCom::Initialize_Prototype()
{


    return Component::Initialize_Prototype();
}

HRESULT CombatStatCom::Initialize(void* arg)
{
    CHECK_FAILED(Component::Initialize(arg), E_FAIL);

    //CombatStatDesc desc{};

    //if (arg != nullptr)
    //{
    //    desc = *static_cast<CombatStatDesc*>(arg);
    //}
    //_statDesc = desc;

  
  
    //_runtimeStat.CurHp = _statDesc.maxHp;
    //_runtimeStat.CurAp = _statDesc.maxAp;
    //_runtimeStat.CurShield = _statDesc.commonCharacterStats.ShieldPointsAtSpawn;
    //_runtimeStat.physicalAttack = _statDesc.physicalAttack;
    //_runtimeStat.physicalDefense = _statDesc.physicalDefense;


    //if (Get_Owner()->Get_Name() == L"Player_Gustave")
    //    _playerType = PlayerCharactorType::Gustave;
    //else if (Get_Owner()->Get_Name() == L"Player_Lune")
    //    _playerType = PlayerCharactorType::Lune;
    //else if (Get_Owner()->Get_Name() == L"Player_Maelle")
    //    _playerType = PlayerCharactorType::Maelle;


 

    return S_OK;
}


//void CombatStatCom::ApplyDamage(float damage)
//{
//    Recalculate_Stats();
//
//    _hp = _maxHp;
//
//    Update_StatInfo();
//  
//    _runtimeStat.CurHp -= damage;
//}

json CombatStatCom::To_Json() const
{
    return Component::To_Json();
}

void CombatStatCom::From_Json(const json& data)
{
    Component::From_Json(data);

}

//bool CombatStatCom::Add_Stat(StatType type, int32 value)
//{
//    _runtimeStat.CurHp -= damage;
//
//    if (_runtimeStat.CurHp < 0.f)
//    {
//        _runtimeStat.CurHp = 0.f;
//    }
//}

//void CombatStatCom::Heal(float amount)
//{
//    _runtimeStat.CurHp += amount;
//
//    if (_runtimeStat.CurHp > _statDesc.maxHp)
//    {
//        _runtimeStat.CurHp = _statDesc.maxHp;
//    }
//}

EAliveStatus CombatStatCom::Get_CurAliveState()
{
    return Get_RuntimeStat().CurAliveState;
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
    Get_RuntimeStat().CurShield += value;
}

void CombatStatCom::Remove_Shield(int32 value)
{
    Get_RuntimeStat().CurShield -= value;

    if (Get_RuntimeStat().CurShield < 0)
    {
        Get_RuntimeStat().CurShield = 0;
    }
}

//Shared<CombatStatCom> CombatStatCom::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
//{
//    auto instance = make_shared<CombatStatCom>(device, context);
//
//    if (FAILED(instance->Initialize_Prototype()))
//    {
//        LOG_ERROR("Failed to Created : CombatStatCom");
//        return nullptr;
//    }
//
//    return instance;
//}

//Shared<Component> CombatStatCom::Clone(void* arg)
//{
//    auto clone = make_shared<CombatStatCom>(*this);
//
//    if (FAILED(clone->Initialize(arg)))
//    {
//        MSG_BOX("Failed to Cloned : CombatStatCom");
//        return nullptr;
//    }
//
//    return clone;
//}

void CombatStatCom::Free()
{
    Component::Free();
}
