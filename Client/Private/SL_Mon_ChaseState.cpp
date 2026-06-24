#include "SL_Mon_ChaseState.h"

#include "AIController.h"
#include "BlackBoard.h"
#include "SL_Dualliste.h"
#include "SL_Mon_StateMachineCom.h"

NS_BEGIN(Client)

SL_Mon_ChaseState::SL_Mon_ChaseState(const Shared<SL_Dualliste>& owner)
    : SL_Mon_State(owner)
{
}

bool SL_Mon_ChaseState::EnterInvoke()
{
    if (!SL_Mon_State::EnterInvoke())
        return false;

    const Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    return owner->Play_Anim(SL_Dualliste::Anim::Run, 0.1f, true);
}

void SL_Mon_ChaseState::Update(float timeDelta)
{
    SL_Mon_State::Update(timeDelta);

    const Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr || _stateCom == nullptr)
        return;

    if (owner->Is_Dead())
    {
        _stateCom->Change_State(SL_MonState::Dead);
        return;
    }

    const Shared<AIController> aiCtrl = owner->Get_Component<AIController>();
    if (aiCtrl == nullptr)
        return;

    const Shared<BlackBoard> bb = aiCtrl->Get_BlackBoard();
    if (bb == nullptr)
        return;

    const any* targetPosValue = bb->Find_Value("TargetPos");
    if (targetPosValue == nullptr)
        return;

    const Shared<TransformCom> transform = owner->Get_Transform();
    if (transform == nullptr)
        return;

    const Vec3 targetPos = any_cast<Vec3>(*targetPosValue);
    const float distanceToTarget = Vec3::Distance(transform->Get_WorldPosition(), targetPos);

    constexpr float kRunDistance = 6.0f;
    constexpr float kWalkSpeed = 3.0f;
    constexpr float kRunSpeed = 6.0f;
    bool shouldRun = false;
    float moveSpeed = kWalkSpeed;
    SL_Dualliste::Anim chaseAnim = SL_Dualliste::Anim::Walk;

    if (distanceToTarget > kRunDistance)
    {
        shouldRun = true;
        moveSpeed = kRunSpeed;
        chaseAnim = SL_Dualliste::Anim::Run;
    }

    transform->Set_SpeedPerSec(moveSpeed);
    owner->Play_Anim(chaseAnim, 0.1f, true);
}

Shared<SL_Mon_ChaseState> SL_Mon_ChaseState::Create(const Shared<SL_Dualliste>& owner)
{
    auto instance = make_shared<SL_Mon_ChaseState>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_MON_CHASE] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

NS_END
