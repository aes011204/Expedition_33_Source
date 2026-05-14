
#include "MonsterStatCom.h"

IMPLEMENT_REFLECTION(MonsterStatCom)
{
    auto& info = GetStaticReflectionInfo();

    info.className = "MonsterStatCom";
    info.displayName = "Monster Stat Com";

    return true;
}

MonsterStatCom::MonsterStatCom(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context)
    : CombatStatCom(device, context)
{
}

MonsterStatCom::MonsterStatCom(
    const MonsterStatCom& Prototype)
    : CombatStatCom(Prototype)
    , _monsterDesc(Prototype._monsterDesc)
    , _MRuntimeStat(Prototype._MRuntimeStat)
{
}

HRESULT MonsterStatCom::Initialize_Prototype()
{
    return CombatStatCom::Initialize_Prototype();
}

HRESULT MonsterStatCom::Initialize(void* arg)
{
    CHECK_FAILED(CombatStatCom::Initialize(arg), E_FAIL);


    MonsterCombatStatDesc desc{};

    if (arg != nullptr)
    {
        desc = *static_cast<MonsterCombatStatDesc*>(arg);
    }
    _monsterDesc = desc;



    _isPlayer = false;

    return S_OK;
}


Shared<MonsterStatCom> MonsterStatCom::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context)
{
    auto instance =
        make_shared<MonsterStatCom>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Create : MonsterStatCom");
        return nullptr;
    }

    return instance;
}

Shared<Component> MonsterStatCom::Clone(void* arg)
{
    auto clone = make_shared<MonsterStatCom>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Clone : MonsterStatCom");
        return nullptr;
    }

    return clone;
}

void MonsterStatCom::Free()
{
    CombatStatCom::Free();
}
