#include "pch.h"
#include "PlayerStateMachineCom.h"

#include "State.h"

PlayerStateMachineCom::PlayerStateMachineCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : StateMachine{ device, context } {}
PlayerStateMachineCom::PlayerStateMachineCom(const StateMachine& prototype)
    : StateMachine{ prototype } {}

HRESULT PlayerStateMachineCom::Initialize_Prototype()
{
    return StateMachine::Initialize_Prototype();
}

HRESULT PlayerStateMachineCom::Initialize(void* arg)
{
    return StateMachine::Initialize(arg);
}

void PlayerStateMachineCom::BeginPlay()
{
    for (auto& [stateTag, state]: _states)
    {
        state->Begin_Play();
    }
}

bool PlayerStateMachineCom::Change_State(PlayerState playerState)
{
    const wstring& str = String::ToWString( magic_enum::enum_name(playerState).data());

    return StateMachine::Change_State(str);
}

PlayerState PlayerStateMachineCom::Get_CurrentPlayerState() const
{
    auto str = String::ToString(_currentStateName);

    return magic_enum::enum_cast<PlayerState>(str).value();
}

Shared<PlayerStateMachineCom> PlayerStateMachineCom::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<PlayerStateMachineCom>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : PlayerStateMachine");
        return nullptr;
    }

    return instance;
}

Shared<Component> PlayerStateMachineCom::Clone(void* arg)
{
    auto clone = make_shared<PlayerStateMachineCom>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Created : PlayerStateMachine");
        return nullptr;
    }

    return clone;
}

void PlayerStateMachineCom::Free()
{
    Component::Free();
}
