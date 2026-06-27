#include "SL_Mon_AttackState.h"

#include "SL_Types.h"
#include "AIController.h"
#include "BlackBoard.h"
#include "SL_Dualliste.h"
#include "SL_Mon_StateMachineCom.h"
#include "MovementCom.h"



SL_MonAttackType Resolve_SelectedAttack(const Shared<SL_Dualliste>& owner)
{
    if (owner == nullptr)
        return SL_MonAttackType::SingleSlash_1;

    const Shared<AIController> aiCtrl = owner->Get_Component<AIController>();
    if (aiCtrl == nullptr)
        return SL_MonAttackType::SingleSlash_1;

    const Shared<BlackBoard> bb = aiCtrl->Get_BlackBoard();
    if (bb == nullptr)
        return SL_MonAttackType::SingleSlash_1;

    const any* value = bb->Find_Value("SelectedAttack");
    if (value == nullptr)
        return SL_MonAttackType::SingleSlash_1;

    const int selectedAttack = any_cast<int>(*value);
    if (selectedAttack < 0 || selectedAttack >= static_cast<int>(SL_MonAttackType::End))
        return SL_MonAttackType::SingleSlash_1;

    return static_cast<SL_MonAttackType>(selectedAttack);
}

SL_Dualliste::Anim Resolve_AttackAnim(SL_MonAttackType attackType)
{
    switch (attackType)
    {
    case SL_MonAttackType::SingleSlash_1:
        return SL_Dualliste::Anim::SingleSlash_1;

    case SL_MonAttackType::SingleSlash_2:
        return SL_Dualliste::Anim::SingleSlash_2;

    case SL_MonAttackType::SingleSlash_3:
        return SL_Dualliste::Anim::SingleSlash_3;

    case SL_MonAttackType::DashBack_Spine:
        return SL_Dualliste::Anim::DashBack_Spain;

    case SL_MonAttackType::DashIn_Stab:
        return SL_Dualliste::Anim::DashIn_Stab;

    case SL_MonAttackType::DashIn_Stab_Estoc:
        return SL_Dualliste::Anim::DashIn_Stab_Estoc;

    case SL_MonAttackType::Att_Projectile:
        return SL_Dualliste::Anim::Att_Projectile;

    case SL_MonAttackType::Combo_1:
        return SL_Dualliste::Anim::Attack_Combo_1;

    case SL_MonAttackType::Combo_2:
        return SL_Dualliste::Anim::Attack_Combo_2;

    case SL_MonAttackType::End:
    default:
        return SL_Dualliste::Anim::Attack_Combo_1;
    }
}


SL_Mon_AttackState::SL_Mon_AttackState(const Shared<SL_Dualliste>& owner)
    : SL_Mon_State(owner)
{
}

void SL_Mon_AttackState::Begin_Play()
{
    SL_Mon_State::Begin_Play();

    const Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr)
        return;

    _movementCom = owner->Get_MovementCom();
    _modelCom = owner->Get_ModelCom();
}

bool SL_Mon_AttackState::EnterInvoke()
{
    if (!SL_Mon_State::EnterInvoke())
        return false;

    const Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    owner->Set_ActionLocked(true);
    Clear_CachedPlayerPosition();

    const SL_MonAttackType selectedAttack = Resolve_SelectedAttack(owner);
    if (const Shared<AIController> aiCtrl = owner->Get_Component<AIController>())
    {
        if (const Shared<BlackBoard> bb = aiCtrl->Get_BlackBoard())
            bb->Set_Value<int>("LastAttack", static_cast<int>(selectedAttack));
    }

    const SL_Dualliste::Anim attackAnim = Resolve_AttackAnim(selectedAttack);

    return owner->Play_Anim(attackAnim, 0.1f, false);
}

void SL_Mon_AttackState::Update(float timeDelta)
{
    SL_Mon_State::Update(timeDelta);
    _movementCom->Set_Velocity(_modelCom->Get_RootTransformVelocity().position);

    const Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr || _stateCom == nullptr)
        return;

    if (owner->Is_Dead())
    {
        _stateCom->Change_State(SL_MonState::Dead);
        return;
    }

    if (!owner->Is_AnimFinished())
        return;


    _stateCom->Change_State(SL_MonState::Idle);
}
void SL_Mon_AttackState::LateUpdate(float timeDelta)
{
    
}
void SL_Mon_AttackState::ExitInvoke()
{
    const Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr)
        return;

    Clear_CachedPlayerPosition();
    owner->Set_ActionLocked(false);
}

void SL_Mon_AttackState::Cache_PlayerPosition(const Vec3& playerPosition)
{
    _cachedPlayerPos = playerPosition;
    _hasCachedPlayerPos = true;
}

bool SL_Mon_AttackState::Try_GetCachedPlayerPosition(Vec3& outPlayerPosition) const
{
    if (!_hasCachedPlayerPos)
        return false;

    outPlayerPosition = _cachedPlayerPos;
    return true;
}

void SL_Mon_AttackState::Clear_CachedPlayerPosition()
{
    _cachedPlayerPos = Vec3::Zero;
    _hasCachedPlayerPos = false;
}
Shared<SL_Mon_AttackState> SL_Mon_AttackState::Create(const Shared<SL_Dualliste>& owner)
{
    auto instance = make_shared<SL_Mon_AttackState>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_MON_ATTACK] : Failed to Create State");
        return nullptr;
    }

    return instance;
}
