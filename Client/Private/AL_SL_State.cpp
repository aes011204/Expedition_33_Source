#include "AL_SL_State.h"

#include "GameObject.h"
#include "SL_Maelle.h"
#include "SL_StateMachineCom.h"

IMPLEMENT_REFLECTION(AN_SL_State)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "AN_SL_State";
    info.category = "SoulLike";

    PROPERTY_ENUM("Next State", _nextState, SL_PlayerState);

    return true;
}

void AN_SL_State::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    if (_nextState == SL_PlayerState::Sprint ||
        _nextState == SL_PlayerState::Parry ||
        _nextState == SL_PlayerState::End)
        return;

    Shared<GameObject> current = context.owner ? context.owner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        if (Shared<SL_Maelle> maelle = dynamic_pointer_cast<SL_Maelle>(current))
        {
            if (Shared<SL_StateMachineCom> stateMachine = maelle->Get_Component<SL_StateMachineCom>())
                stateMachine->Change_State(_nextState);

            return;
        }

        current = current->Get_Owner();
    }
}
