#include "SL_BasicAttack.h"

#include "SL_Maelle.h"
#include "SL_StateMachineCom.h"

SL_BasicAttack::SL_BasicAttack(const Shared<SL_Maelle>& owner)
    : SL_State(owner)
{
}

bool SL_BasicAttack::EnterInvoke()
{
    Shared<SL_Maelle> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    owner->Set_CombatLocked(true);

    _comboIndex = 0;
    _comboWindowEnabled = false;
    _comboQueued = false;

    Play_CurrentCombo();

    return true;
}

void SL_BasicAttack::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!Is_AnimFinished())
        return;

    Finish_Attack();
}

bool SL_BasicAttack::Request_BasicAttack()
{
    if (!_comboWindowEnabled)
        return false;

    if (!Can_AdvanceCombo())
        return false;

    _comboQueued = true;
    return true;
}

bool SL_BasicAttack::Request_Skill(const Skill_Data& skillData, uint32 slotIndex)
{
    UNREFERENCED_PARAMETER(skillData);
    UNREFERENCED_PARAMETER(slotIndex);

    if (!_comboWindowEnabled)
        return false;

    _comboQueued = false;

    return _stateCom != nullptr && _stateCom->Change_State(SL_PlayerState::Skill);
}

void SL_BasicAttack::Set_ComboWindowEnabled(bool enabled)
{
    _comboWindowEnabled = enabled;

    if (enabled)
        return;

    if (!_comboQueued)
        return;

    if (!Can_AdvanceCombo())
        return;

    ++_comboIndex;
    _comboQueued = false;
    _comboWindowEnabled = false;

    Play_CurrentCombo();
}

void SL_BasicAttack::Play_CurrentCombo()
{
    Play_Anim(Get_CurrentComboAnim(), 0.05f, false);
}

SoulLikeAnim SL_BasicAttack::Get_CurrentComboAnim() const
{
    switch (_comboIndex)
    {
    case 0:
        return SoulLikeAnim::Attack_1;
    case 1:
        return SoulLikeAnim::Attack_2;
    case 2:
        return SoulLikeAnim::Attack_3;
    default:
        return SoulLikeAnim::Attack_3;
    }
}

bool SL_BasicAttack::Can_AdvanceCombo() const
{
    return _comboIndex + 1 < ComboMaxCount;
}

void SL_BasicAttack::Finish_Attack()
{
    _comboIndex = 0;
    _comboWindowEnabled = false;
    _comboQueued = false;

    if (Shared<SL_Maelle> owner = _owner.lock())
        owner->Set_CombatLocked(false);

    if (_stateCom)
        _stateCom->Change_State(SL_PlayerState::Idle);
}

Shared<SL_BasicAttack> SL_BasicAttack::Create(const Shared<SL_Maelle>& owner)
{
    auto instance = make_shared<SL_BasicAttack>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_BASIC_ATTACK] : Failed to Create State");
        return nullptr;
    }

    return instance;
}
