#include "SL_Skill.h"

#include "SL_Maelle.h"
#include "SL_StateMachineCom.h"

SL_Skill::SL_Skill(const Shared<SL_Maelle>& owner)
    : SL_State(owner)
{
}

bool SL_Skill::EnterInvoke()
{
    Shared<SL_Maelle> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    owner->Set_CombatLocked(true);
    Stop_Movement();

    _anim = Resolve_SkillAnim(_slotIndex);
    Play_Anim(_anim, 0.05f, false);

    return true;
}

void SL_Skill::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!Is_AnimFinished())
        return;

    if (Shared<SL_Maelle> owner = _owner.lock())
        owner->Set_CombatLocked(false);

    if (_stateCom)
        _stateCom->Change_State(SL_PlayerState::Idle);
}

void SL_Skill::Set_SkillRequest(const Skill_Data& skillData, uint32 slotIndex)
{
    _skillData = skillData;
    _slotIndex = slotIndex;
}

SoulLikeAnim SL_Skill::Resolve_SkillAnim(uint32 slotIndex) const
{
    switch (slotIndex)
    {
    case 1:
        return SoulLikeAnim::Skill_2;
    case 2:
        return SoulLikeAnim::SKill_3;
    case 3:
        return SoulLikeAnim::Skill_4;
    case 0:
    default:
        return SoulLikeAnim::Skill_1;
    }
}

Shared<SL_Skill> SL_Skill::Create(const Shared<SL_Maelle>& owner)
{
    auto instance = make_shared<SL_Skill>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_SKILL] : Failed to Create State");
        return nullptr;
    }

    return instance;
}
