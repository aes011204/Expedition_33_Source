#include "MonsterStatCom.h"
#include "GameInstance.h"

IMPLEMENT_REFLECTION(MonsterStatCom)
{
    auto& info = GetStaticReflectionInfo();

    info.className = "MonsterStatCom";
    info.displayName = "Monster Stat Com";

    PROPERTY_FLOAT("HP", _MRuntimeStat.CurHp, 0.f, 9999.f, 1.f);
    PROPERTY_FLOAT("AP", _MRuntimeStat.CurAp, 0.f, 9.f, 1.f);

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

    {
        _MRuntimeStat.CurHp = _monsterDesc.maxHp;
        _MRuntimeStat.CurAp = _monsterDesc.maxAp;
        _MRuntimeStat.CurShield = _monsterDesc.commonCharacterStats.ShieldPointsAtSpawn;
        _MRuntimeStat.physicalAttack = _monsterDesc.physicalAttack;
        _MRuntimeStat.physicalDefense = _monsterDesc.physicalDefense;
        _MRuntimeStat.CurSpeed = _monsterDesc.speed;
    }

    _skillKeys.push_back("Lancelier_Slow");
    _skillKeys.push_back("Lancelier_Fast");



    _isPlayer = false;

    return S_OK;
}

void MonsterStatCom::Update(float timeDelta)
{
#ifdef _DEBUG
    GAME->Print(L"몬스터 체력 : {}", _MRuntimeStat.CurHp, duration(0.f));
#endif
}

void MonsterStatCom::Set_RuntimeCombatProfile(float maxHp, uint32 maxAp, float physicalAttack, float physicalDefense,
    float speed)
{
    const float hpRate = (_monsterDesc.maxHp > 0.f) ? (_MRuntimeStat.CurHp / _monsterDesc.maxHp) : 1.f;

    _monsterDesc.maxHp = maxHp;
    _monsterDesc.maxAp = maxAp;
    _monsterDesc.physicalAttack = physicalAttack;
    _monsterDesc.physicalDefense = physicalDefense;
    _monsterDesc.speed = speed;

    // 몬스터는 desc를 runtime으로 복사하는 구조라 여기서 둘 다 같이 맞춘다.
    _MRuntimeStat.CurHp = max(0.f, _monsterDesc.maxHp * hpRate);
    _MRuntimeStat.CurAp = min(_MRuntimeStat.CurAp, _monsterDesc.maxAp);
    _MRuntimeStat.physicalAttack = _monsterDesc.physicalAttack;
    _MRuntimeStat.physicalDefense = _monsterDesc.physicalDefense;
    _MRuntimeStat.CurSpeed = _monsterDesc.speed;
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
