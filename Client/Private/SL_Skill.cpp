#include "SL_Skill.h"
#include "Event_Bus.h"
#include "SL_Maelle.h"
#include "SL_StateMachineCom.h"
#include "Body_Player.h"
#include "ModelCom.h"
#include "SoulLikeController.h"
#include "ClientInstance.h"

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
    Resolve_PlaySection();

    if (Shared<SoulLikeController> soulLikeController = CLIENT->Get_SoulLikeController())
        soulLikeController->Apply_SkillCameraPreset(_skillData.skillKey, _playSectionStartSec);

    const float blendDuration = _playSectionStartSec > 0.f ? 0.f : 0.05f;
    Play_Anim(_anim, blendDuration, false, _playSectionStartSec);

    return true;
}

void SL_Skill::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (Shared<SoulLikeController> soulLikeController = CLIENT->Get_SoulLikeController())
    {
        if (_body != nullptr && _body->Get_Model() != nullptr)
            soulLikeController->Sync_SkillCameraClipTime(_body->Get_Model()->Get_CurrentAnimationTimeSec());
    }

    if (!Is_PlaySectionFinished() && !Is_AnimFinished())
        return;

    Finish_Skill();
}

void SL_Skill::ExitInvoke()
{
    if (Shared<SoulLikeController> soulLikeController = CLIENT->Get_SoulLikeController())
        soulLikeController->Restore_SkillCameraPreset();

    _playSectionStartSec = 0.f;
    _playSectionEndSec = -1.f;

    if (Shared<SL_Maelle> owner = _owner.lock())
        owner->Set_CombatLocked(false);

    SL_State::ExitInvoke();
}

void SL_Skill::Set_SkillRequest(const Skill_Data& skillData, uint32 slotIndex)
{
    _skillData = skillData;
    _slotIndex = slotIndex;
}

bool SL_Skill::Request_Dodge(const SL_MoveRequest& request)
{
    if (_stateCom == nullptr || !_stateCom->Can_CancelToDodge())
        return false;

    _stateCom->Clear_CanActionCancel();

    return _stateCom->Change_State(SL_PlayerState::Dodge);
}

bool SL_Skill::Request_Jump()
{
    if (_stateCom == nullptr || !_stateCom->Can_CancelToJump())
        return false;

    _stateCom->Clear_CanActionCancel();

    return _stateCom->Change_State(SL_PlayerState::Jump);
}

bool SL_Skill::Request_Parry()
{
    if (_stateCom == nullptr || !_stateCom->Can_CancelToParry())
        return false;

    _stateCom->Clear_CanActionCancel();

    return _stateCom->Change_State(SL_PlayerState::Parry);
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

void SL_Skill::Resolve_PlaySection()
{
    _playSectionStartSec = 0.f;
    _playSectionEndSec = -1.f;

    if (_body == nullptr || _body->Get_Model() == nullptr)
        return;

    const AnimNotifyData* animationData = _body->Get_Model()->Find_AnimNotifyDataByState(ETOI(_anim));
    if (animationData == nullptr)
        return;

    _playSectionStartSec = max(0.f, animationData->playSectionStartSec);
    _playSectionEndSec = animationData->playSectionEndSec;
}

bool SL_Skill::Is_PlaySectionFinished() const
{
    if (_playSectionEndSec < 0.f)
        return false;

    if (_body == nullptr || _body->Get_Model() == nullptr)
        return false;

    return _body->Get_Model()->Get_CurrentAnimationTimeSec() + 0.0001f >= _playSectionEndSec;
}

void SL_Skill::Finish_Skill()
{
    if (Shared<SL_Maelle> owner = _owner.lock())
        owner->Set_CombatLocked(false);

    if (Shared<SoulLikeController> soulLikeController = CLIENT->Get_SoulLikeController())
    {
        soulLikeController->Restore_SkillCameraPreset();
        soulLikeController->End_BattleShotCamera();
    }

    if (_stateCom)
        _stateCom->Change_State(SL_PlayerState::Idle);
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
