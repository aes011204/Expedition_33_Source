#include "BuffSystem.h"

#include "Buff.h"
#include "CombatStatCom.h"
#include "DamageSystem.h"

HRESULT BuffSystem::Initialize()
{
    _damageCalculatingHandle =
        DamageSystem::OnDamageCalculating.Add(this, &BuffSystem::Handle_DamageCalculating);

    return S_OK;
}

void BuffSystem::Apply_Buff(const BattleChar& target, Buffs buffId, uint32 stack, int32 durationTurn)
{
    if (buffId == Buffs::None || target.statCom == nullptr)
        return;

    // 버프가 있는지?
    auto existing = Find_Buff(target, buffId);
    if (existing)
    {
        existing->Add_Stack(stack);
        OnBuffApplied.Broadcast(target, buffId);
        return;
    }

    auto buff = Buff::Create();
    buff->Initialize(buffId, stack, durationTurn);

    target.statCom->Get_Buff().push_back(buff);
    OnBuffApplied.Broadcast(target, buffId);
}

void BuffSystem::Remove_Buff(const BattleChar& target, Buffs buffId)
{
    if (target.statCom == nullptr)
        return;

    auto& buffs = target.statCom->Get_Buff();

    for (auto it = buffs.begin(); it != buffs.end();)
    {
        if ((*it) && (*it)->Get_BuffId() == buffId)
        {
            it = buffs.erase(it);
            OnBuffRemoved.Broadcast(target, buffId);
            continue;
        }

        ++it;
    }
}

void BuffSystem::Clear_Buffs(const BattleChar& target)
{
    if (target.statCom == nullptr)
        return;

    target.statCom->Get_Buff().clear();
}

void BuffSystem::Clear_All(const vector<BattleChar>& heroes, const vector<BattleChar>& enemies)
{
    for (const auto& hero : heroes)
        Clear_Buffs(hero);

    for (const auto& enemy : enemies)
        Clear_Buffs(enemy);
}

bool BuffSystem::Has_Buff(const BattleChar& target, Buffs buffId) const
{
    return Find_Buff(target, buffId) != nullptr;
}

bool BuffSystem::Try_ConsumeMarkOnDamage(const BattleChar& attacker, const BattleChar& target, float& inOutDamage)
{
    if (inOutDamage <= 0.f)
        return false;

    // 플레이어가 데미지 줄 떄에만 소모
    if (attacker.statCom == nullptr || !attacker.statCom->Is_Player())
        return false;

    auto mark = Find_Buff(target, Buffs::Mark);
    CHECK_NULL(mark, false);

    inOutDamage += MARK_BONUS_DAMAGE;
    Remove_Buff(target, Buffs::Mark);

    OnBuffConsumed.Broadcast(target, Buffs::Mark, MARK_BONUS_DAMAGE);
    return true;
}

void BuffSystem::ProcessTurnStart(Shared<CombatStatCom> statCom)
{
    UNREFERENCED_PARAMETER(statCom);
}

void BuffSystem::ProcessTurnEnd(Shared<CombatStatCom> statCom)
{
    if (statCom == nullptr)
        return;

    auto& buffs = statCom->Get_Buff();

    for (auto it = buffs.begin(); it != buffs.end();)
    {
        if (*it == nullptr || (*it)->Is_PendingRemove())
        {
            it = buffs.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void BuffSystem::Handle_DamageCalculating(const BattleChar& attacker, const BattleChar& target, float& damage,
    AttackType attackType)
{
    // 일단 AttackType은 빼고
    Try_ConsumeMarkOnDamage(attacker, target, damage);
}

Shared<Buff> BuffSystem::Find_Buff(const BattleChar& target, Buffs buffId) const
{
    if (target.statCom == nullptr)
        return nullptr;

    const auto& buffs = target.statCom->Get_Buff();

    for (const auto& buff : buffs)
    {
        if (buff && buff->Get_BuffId() == buffId)
        {
            return buff;
        }
    }

    return nullptr;
}

void BuffSystem::Remove_PendingBuffs(const BattleChar& target)
{
    if (target.statCom == nullptr)
        return;

    auto& buffs = target.statCom->Get_Buff();

    for (auto it = buffs.begin(); it != buffs.end();)
    {
        if (*it == nullptr || (*it)->Is_PendingRemove())
            it = buffs.erase(it);
        else
            ++it;
    }
}

Shared<BuffSystem> BuffSystem::Create()
{
    auto instance = make_shared<BuffSystem>();

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("Failed to Created : BuffSystem");
        return nullptr;
    }

    return instance;
}

void BuffSystem::Free()
{
    if (_damageCalculatingHandle.IsValid())
    {
        DamageSystem::OnDamageCalculating.Remove(_damageCalculatingHandle);
        _damageCalculatingHandle.Reset();
    }

    Base::Free();
}
