#include "AN_Change_Anim.h"

#include "ModelCom.h"
#include "BattleController.h"
#include "ClientInstance.h"
#include "CombatStatCom.h"

IMPLEMENT_REFLECTION(AN_Change_Anim)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_ENUM("AnimType", _animType, BattleAnimType);
    PROPERTY_BOOL("루프", _isLoop);


    return true;
}

void AN_Change_Anim::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    if (context.model == nullptr)
        return;

    const auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr || battle->Is_BattleStarted() == false)
        return;

    const Shared<GameObject> notifyOwner = context.owner ? context.owner->Get_Owner() : nullptr;
    if (notifyOwner == nullptr)
        return;

    const BattleAction& action = battle->Get_TurnAction();
    const ActionRequest& request = battle->Get_TurnActionRequest();

    const BattleChar* notifyActor = nullptr;

    if (action.Get_CurChar().actor &&
        dynamic_pointer_cast<GameObject>(action.Get_CurChar().actor) == notifyOwner)
    {
        notifyActor = &action.Get_CurChar();
    }
    else if (action.GetInitTarget().actor &&
        dynamic_pointer_cast<GameObject>(action.GetInitTarget().actor) == notifyOwner)
    {
        notifyActor = &action.GetInitTarget();
    }
    else if (action.GetCurStepIndex() < action.GetSteps().size())
    {
        for (const BattleChar& target : action.GetCurStepTarget())
        {
            if (target.actor &&
                dynamic_pointer_cast<GameObject>(target.actor) == notifyOwner)
            {
                notifyActor = &target;
                break;
            }
        }
    }

    if (notifyActor == nullptr &&
        request.actor.actor &&
        dynamic_pointer_cast<GameObject>(request.actor.actor) == notifyOwner)
    {
        notifyActor = &request.actor;
    }

    if (notifyActor == nullptr)
    {
        for (const BattleChar& hero : battle->Get_BattleHeroes())
        {
            if (hero.actor &&
                dynamic_pointer_cast<GameObject>(hero.actor) == notifyOwner)
            {
                notifyActor = &hero;
                break;
            }
        }
    }

    if (notifyActor == nullptr)
    {
        for (const BattleChar& enemy : battle->Get_BattleEnemies())
        {
            if (enemy.actor &&
                dynamic_pointer_cast<GameObject>(enemy.actor) == notifyOwner)
            {
                notifyActor = &enemy;
                break;
            }
        }
    }

    if (notifyActor == nullptr)
        return;

    if (notifyActor->statCom == nullptr ||
        notifyActor->statCom->Get_CurAliveState() == EAliveStatus::Dead)
        return;

    context.model->Set_AnimationByState(ETOI(_animType), 0.3f, _isLoop);
}
