#include "MonsterStatCom.h"
#include "GameInstance.h"
#include "BattleMonsterSpawnDesc.h"

IMPLEMENT_REFLECTION(MonsterStatCom)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.className = "MonsterStatCom";
    info.displayName = "Monster Stat Com";

    PROPERTY_FLOAT("Base Max HP", _monsterDesc.maxHp, 0.f, 999999.f, 1.f);
    PROPERTY_INT("Base Max AP", _monsterDesc.maxAp, 0.f, 999.f, 1.f);
    PROPERTY_FLOAT("Base Attack", _monsterDesc.physicalAttack, 0.f, 999999.f, 1.f);
    PROPERTY_FLOAT("Base Defense", _monsterDesc.physicalDefense, 0.f, 999999.f, 1.f);
    PROPERTY_FLOAT("Base Speed", _monsterDesc.speed, 0.f, 999999.f, 1.f);
    PROPERTY_FLOAT("Battle Run Move Speed", _monsterDesc.battleRunMoveSpeed, 0.f, 30.f, 0.1f);
    PROPERTY_FLOAT("Max Break", _monsterDesc.maxBreak, 0.f, 999999.f, 1.f);
    PROPERTY_FLOAT("Break Stun Duration", _monsterDesc.breakBarStunDuration, 0.f, 30.f, 0.1f);
    PROPERTY_FLOAT("Current Break", _MRuntimeStat.curBreak, 0.f, 999999.f, 0.1f);
    PROPERTY_BOOL("Is Break", _MRuntimeStat.isBreak);

    PROPERTY_INT("Reward Exp", _monsterDesc.exp, 0.f, 9999999.f, 1.f);
    PROPERTY_INT("Reward Chroma", _monsterDesc.chroma, 0.f, 9999999.f, 1.f);

    PROPERTY_ENUM("Enemy Class", _monsterDesc.enemyClass, EEnemyClass);
    PROPERTY_BOOL("V2", _monsterDesc.v2);
    PROPERTY_INT("Shield Points At Spawn", _monsterDesc.commonCharacterStats.ShieldPointsAtSpawn, 0.f, 99.f, 1.f);
    PROPERTY_STRING("Portrait", _monsterDesc.portraitGuid);
    PROPERTY_WSTRING("Battle Announce Name", _monsterDesc.battleAnnounceName);

    PROPERTY_FLOAT("criticalRatio", _monsterDesc._criticalRatio, 0.f, 1.f, .1f);
    PROPERTY_FLOAT("midRatio", _monsterDesc._midRatio, 0.f, 1.f, .1f);


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
    , _skillRuntimeByKey(Prototype._skillRuntimeByKey)
    , _battleTurnCount(Prototype._battleTurnCount)
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
    _MRuntimeStat = {};

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

    _isPlayer = false;

    return S_OK;
}

void MonsterStatCom::From_Json(const json& data)
{
    CombatStatCom::From_Json(data);

    if (!data.contains("_monsterDesc.enemyClass") &&
        data.contains("_monsterDesc.isBoss") &&
        data["_monsterDesc.isBoss"].is_boolean() &&
        data["_monsterDesc.isBoss"].get<bool>())
    {
        _monsterDesc.enemyClass = EEnemyClass::Boss;
    }

    _MRuntimeStat.CurHp = _monsterDesc.maxHp;
    _MRuntimeStat.CurAp = _monsterDesc.maxAp;
    _MRuntimeStat.CurShield = _monsterDesc.commonCharacterStats.ShieldPointsAtSpawn;
    _MRuntimeStat.physicalAttack = _monsterDesc.physicalAttack;
    _MRuntimeStat.physicalDefense = _monsterDesc.physicalDefense;
    _MRuntimeStat.CurSpeed = _monsterDesc.speed;
    _MRuntimeStat.isBreak = false;
    _MRuntimeStat.curBreak = 0.f;
}

void MonsterStatCom::Update(float timeDelta)
{
#ifdef _DEBUG
    //GAME->Print(L"몬스터 체력 : {}", _MRuntimeStat.CurHp, duration(0.f));
#endif
}

void MonsterStatCom::Add_Break(float value)
{
    if (value <= 0.f)
        return;

    Set_Break(_MRuntimeStat.curBreak + value);
}

void MonsterStatCom::Set_Break(float value)
{
    _MRuntimeStat.curBreak = clamp(value, 0.f, _monsterDesc.maxBreak);
}

void MonsterStatCom::Clear_Break()
{
    _MRuntimeStat.curBreak = 0.f;
}

void MonsterStatCom::Rebuild_SkillRuntimeState()
{
    _skillRuntimeByKey.clear();

    for (const string& skillKey : _skillKeys)
    {
        if (skillKey.empty())
            continue;

        _skillRuntimeByKey.emplace(skillKey, Skill_Data_Inst{});
    }

    _battleTurnCount = 0;
}

Skill_Data_Inst* MonsterStatCom::Find_SkillRuntimeState(const string& skillKey)
{
    auto it = _skillRuntimeByKey.find(skillKey);
    if (it == _skillRuntimeByKey.end())
        return nullptr;

    return &it->second;
}

const Skill_Data_Inst* MonsterStatCom::Find_SkillRuntimeState(const string& skillKey) const
{
    auto it = _skillRuntimeByKey.find(skillKey);
    if (it == _skillRuntimeByKey.end())
        return nullptr;

    return &it->second;
}

bool MonsterStatCom::Can_UseSkill(const Skill_Data& skill) const
{
    if (skill.skillKey.empty())
        return false;

    if (_battleTurnCount < skill.minTurn)
        return false;

    const Skill_Data_Inst* inst = Find_SkillRuntimeState(skill.skillKey);
    if (inst == nullptr)
        return true;

    if (skill.maxUseCount >= 0 && inst->curUsedCount >= skill.maxUseCount)
        return false;

    if (inst->curCoolDownTurn > 0)
        return false;

    return true;
}

void MonsterStatCom::Notify_SkillUsed(const Skill_Data& skill)
{
    if (skill.skillKey.empty())
        return;

    Skill_Data_Inst* inst = Find_SkillRuntimeState(skill.skillKey);
    if (inst == nullptr)
        return;

    inst->curUsedCount++;
    inst->curCoolDownTurn = max(0, skill.coolDownTurn);
}

void MonsterStatCom::Advance_SkillCooldownTurn()
{
    ++_battleTurnCount;

    for (auto& [skillKey, inst] : _skillRuntimeByKey)
    {
        UNREFERENCED_PARAMETER(skillKey);

        if (inst.curCoolDownTurn > 0)
            --inst.curCoolDownTurn;
    }
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

  _monsterDesc._criticalRatio = _monsterDesc._criticalRatio;
  _monsterDesc._midRatio = _monsterDesc._midRatio;
}

BattleMonsterStatSnapshot MonsterStatCom::Create_BattleSyncSnapshot() const
{
    BattleMonsterStatSnapshot snapshot{};
    snapshot.valid = true; // 이 값이 true일 때만 BattleController가 MB에 적용한다.

    snapshot.maxHp = _monsterDesc.maxHp;
    snapshot.maxAp = _monsterDesc.maxAp;
    snapshot.physicalAttack = _monsterDesc.physicalAttack;
    snapshot.physicalDefense = _monsterDesc.physicalDefense;
    snapshot.speed = _monsterDesc.speed;
    snapshot.battleRunMoveSpeed = _monsterDesc.battleRunMoveSpeed;
    snapshot.maxBreak = _monsterDesc.maxBreak;
    snapshot.breakBarStunDuration = _monsterDesc.breakBarStunDuration;

    snapshot.shieldPointsAtSpawn = _monsterDesc.commonCharacterStats.ShieldPointsAtSpawn;
    snapshot.exp = _monsterDesc.exp;
    snapshot.chroma = _monsterDesc.chroma;
    snapshot.enemyClass = _monsterDesc.enemyClass;
    snapshot.battleAnnounceName = _monsterDesc.battleAnnounceName;

    return snapshot;
}

void MonsterStatCom::Apply_BattleSyncSnapshot(const BattleMonsterStatSnapshot& snapshot)
{
    if (!snapshot.valid)
        return;

    _monsterDesc.maxHp = snapshot.maxHp;
    _monsterDesc.maxAp = snapshot.maxAp;
    _monsterDesc.physicalAttack = snapshot.physicalAttack;
    _monsterDesc.physicalDefense = snapshot.physicalDefense;
    _monsterDesc.speed = snapshot.speed;
    _monsterDesc.battleRunMoveSpeed = snapshot.battleRunMoveSpeed;
    _monsterDesc.maxBreak = snapshot.maxBreak;
    _monsterDesc.breakBarStunDuration = snapshot.breakBarStunDuration;

    _monsterDesc.commonCharacterStats.ShieldPointsAtSpawn = snapshot.shieldPointsAtSpawn;
    _monsterDesc.exp = snapshot.exp;
    _monsterDesc.chroma = snapshot.chroma;
    _monsterDesc.enemyClass = snapshot.enemyClass;
    _monsterDesc.battleAnnounceName = snapshot.battleAnnounceName;

    // 전투 진입 직후 적용이므로 runtime도 새 desc 기준으로 다시 맞춘다.
    _MRuntimeStat.CurHp = _monsterDesc.maxHp;
    _MRuntimeStat.CurAp = _monsterDesc.maxAp;
    _MRuntimeStat.CurShield = _monsterDesc.commonCharacterStats.ShieldPointsAtSpawn;
    _MRuntimeStat.physicalAttack = _monsterDesc.physicalAttack;
    _MRuntimeStat.physicalDefense = _monsterDesc.physicalDefense;
    _MRuntimeStat.CurSpeed = _monsterDesc.speed;
    _MRuntimeStat.isBreak = false;
    _MRuntimeStat.curBreak = 0.f;
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
