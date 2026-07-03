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
    _reviveBlocked = false;

 

    return S_OK;
}

json CombatStatCom::To_Json() const
{
    json root = Component::To_Json();

    if (!_skillKeys.empty())
        root["_skillKeys"] = _skillKeys;

    return root;
}

void CombatStatCom::From_Json(const json& data)
{
    Component::From_Json(data);
    _reviveBlocked = false;

    const auto skillKeysIt = data.find("_skillKeys");
    if (skillKeysIt == data.end() || !skillKeysIt->is_array())
        return;

    _skillKeys.clear();

    for (const json& entry : *skillKeysIt)
    {
        if (!entry.is_string())
            continue;

        _skillKeys.push_back(entry.get<string>());
    }
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

void CombatStatCom::Set_CurAp(uint32 value)
{
    auto& runtime = Get_RuntimeStat();
    const uint32 maxAp = Get_StatInfo().maxAp;

    runtime.CurAp = min(value, maxAp);
}

void CombatStatCom::Add_GradientGauge(uint32 value)
{
    const uint32 maxProgress =
        GradientSkill::MaxGaugeStacks * GradientSkill::GaugeProgressPerStack;

    const uint32 currentProgress =
        _gradientGaugeStack * GradientSkill::GaugeProgressPerStack +
        _gradientGaugeProgressTick;

    const uint32 nextProgress = min(currentProgress + value, maxProgress);

    _gradientGaugeStack = nextProgress / GradientSkill::GaugeProgressPerStack;
    _gradientGaugeProgressTick = nextProgress % GradientSkill::GaugeProgressPerStack;
}

bool CombatStatCom::Try_ConsumeGradientGauge(uint32 value)
{
    if (_gradientGaugeStack < value)
        return false;

    _gradientGaugeStack -= value;
    return true;
}

void CombatStatCom::Add_Shield(int32 value)
{
    Get_RuntimeStat().CurShield += value;
}

void CombatStatCom::Remove_Shield(int32 value)
{
    if (value <= 0)
        return;

    if (Get_RuntimeStat().CurShield <= static_cast<uint32>(value))
        Get_RuntimeStat().CurShield = 0;
    else
        Get_RuntimeStat().CurShield -= static_cast<uint32>(value);
}


void CombatStatCom::Free()
{
    Component::Free();
}
