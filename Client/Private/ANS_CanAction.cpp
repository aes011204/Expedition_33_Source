#include "ANS_CanAction.h"

#include "GameObject.h"
#include "SL_Maelle.h"
#include "SL_StateMachineCom.h"

IMPLEMENT_REFLECTION(ANS_CanAction)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "ANS_CanAction";
    info.category = "SoulLike";

    PROPERTY_BOOL("Can Jump", _canJump);
    PROPERTY_BOOL("Can Dodge", _canDodge);
    PROPERTY_BOOL("Can Parry", _canParry);

    return true;
}

void ANS_CanAction::On_Begin(const AnimNotifyContext& context)
{
    Set_CanAction(context, true);
}

void ANS_CanAction::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
}

void ANS_CanAction::On_End(const AnimNotifyContext& context)
{
    Set_CanAction(context, false);
}

void ANS_CanAction::Set_CanAction(const AnimNotifyContext& context, bool enabled) const
{
    if (context.isPreview || (context.isCinematic && enabled))
        return;

    Shared<GameObject> current = context.owner ? context.owner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        if (Shared<SL_Maelle> maelle = dynamic_pointer_cast<SL_Maelle>(current))
        {
            if (Shared<SL_StateMachineCom> stateMachine = maelle->Get_Component<SL_StateMachineCom>())
                stateMachine->Set_CanActionCancel(enabled, _canJump, _canDodge, _canParry);

            return;
        }

        current = current->Get_Owner();
    }
}
