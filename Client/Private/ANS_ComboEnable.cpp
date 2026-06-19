#include "ANS_ComboEnable.h"

#include "GameObject.h"
#include "SL_Maelle.h"
#include "SL_StateMachineCom.h"

IMPLEMENT_REFLECTION(ANS_ComboEnable)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "ANS_ComboEnable";
    info.category = "SoulLike";

    return true;
}

void ANS_ComboEnable::On_Begin(const AnimNotifyContext& context)
{
    Set_ComboWindow(context, true);
}

void ANS_ComboEnable::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
}

void ANS_ComboEnable::On_End(const AnimNotifyContext& context)
{
    Set_ComboWindow(context, false);
}

void ANS_ComboEnable::Set_ComboWindow(const AnimNotifyContext& context, bool enabled) const
{
    if (context.isPreview)
        return;

    Shared<GameObject> current = context.owner ? context.owner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        if (Shared<SL_Maelle> maelle = dynamic_pointer_cast<SL_Maelle>(current))
        {
            if (Shared<SL_StateMachineCom> stateMachine = maelle->Get_Component<SL_StateMachineCom>())
                stateMachine->Notify_BasicAttackComboWindow(enabled);

            return;
        }

        current = current->Get_Owner();
    }
}
