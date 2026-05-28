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
    

 

    return S_OK;
}

json CombatStatCom::To_Json() const
{
    return Component::To_Json();
}

void CombatStatCom::From_Json(const json& data)
{
    Component::From_Json(data);

}


EAliveStatus CombatStatCom::Get_CurAliveState()
{
    return Get_RuntimeStat().CurAliveState;
}

bool CombatStatCom::Try_GetSkillKeyBySlot(uint32 slotIndex, string& outSkillKey) const
{
    if (slotIndex >= _skillKeys.size())
        return false;

    if (_skillKeys[slotIndex].empty())
        return false;

    outSkillKey = _skillKeys[slotIndex];
    return true;
}

string CombatStatCom::Get_FirstSkill()
{
    if (Get_Owner()->Get_Name() == L"PB_Gustave")
    {
        return "Gustave_Default_Attack";
    }
    if (Get_Owner()->Get_Name() == L"PB_Lune")
    {
        return "Lune_Default_Attack";
    }
    if (Get_Owner()->Get_Name() == L"PB_Maelle")
    {
        return "Maelle_Default_Attack";
    }

    return "";
}

void CombatStatCom::Add_AP(uint32 value)
{
    auto& runtime = Get_RuntimeStat();
    const uint32 maxAp = Get_StatInfo().maxAp;

    // 최대 AP 초과 방지
    runtime.CurAp = min(runtime.CurAp + value, maxAp);
}

bool CombatStatCom::Try_ConsumeAP(uint32 value)
{
    auto& runtime = Get_RuntimeStat();

    if (runtime.CurAp < value)
        return false;

    runtime.CurAp -= value;

    return true;
}

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


void CombatStatCom::Free()
{
    Component::Free();
}
