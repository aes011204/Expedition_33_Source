#include "pch.h"
#include "PlayerStateMachineCom.h"

#include "Player.h"
#include "Player_Idle.h"
#include "Player_Jump.h"
#include "Player_Move.h"
#include "Player_State.h"

NS_BEGIN(Client);

IMPLEMENT_REFLECTION(PlayerStateMachineCom)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "PlayerStateMachine";

    return true;
}


PlayerStateMachineCom::PlayerStateMachineCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : StateMachineCom{ device, context } {}
PlayerStateMachineCom::PlayerStateMachineCom(const StateMachineCom& prototype)
    : StateMachineCom{ prototype } {}

HRESULT PlayerStateMachineCom::Initialize_Prototype()
{
    return StateMachineCom::Initialize_Prototype();
}

HRESULT PlayerStateMachineCom::Initialize(void* arg)
{
    CHECK_FAILED(StateMachineCom::Initialize(arg), E_FAIL);
    return S_OK;
}

void PlayerStateMachineCom::BeginPlay()
{
    if (_owner.expired()) return;

    CHECK_FAILED(Add_State(PlayerState::Idle, Player_Idle::Create(static_pointer_cast<Player>(_owner.lock()))));
    CHECK_FAILED(Add_State(PlayerState::Move, Player_Move::Create(static_pointer_cast<Player>(_owner.lock()))));
    CHECK_FAILED(Add_State(PlayerState::Jump, Player_Jump::Create(static_pointer_cast<Player>(_owner.lock()))));

    for (auto& [stateTag, state]: _states)
    {
        state->Begin_Play();
    }
}

HRESULT PlayerStateMachineCom::Add_State(PlayerState playerState, const Shared<State>& state)
{
    const wstring& str = String::ToWString(magic_enum::enum_name(playerState).data());

    return StateMachineCom::Add_State(str, state);
}

bool PlayerStateMachineCom::Change_State(PlayerState playerState)
{
    const wstring& str = String::ToWString( magic_enum::enum_name(playerState).data());

    return StateMachineCom::Change_State(str);
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

NS_END
