#include "pch.h"
#include "PlayerStateMachineCom.h"

#include "Player_FreeAim.h"
#include "Player_GameMenu.h"
#include "Player_WorldBase.h"
#include "Player_Idle.h"
#include "Player_Jump.h"
#include "Player_Move.h"
#include "Player_Climb.h"
#include "Player_RopeAction.h"
#include "Player_GrappleAction.h"
#include "Player_State.h"
#include "Player_ParkourAction.h"

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
    _statesBegunPlay = false;
    return S_OK;
}

void PlayerStateMachineCom::BeginPlay()
{
    if (_owner.expired()) return;

    CHECK_FAILED(Add_State(PlayerState::Idle, Player_Idle::Create(static_pointer_cast<Player_WorldBase>(_owner.lock()))));
    CHECK_FAILED(Add_State(PlayerState::Move, Player_Move::Create(static_pointer_cast<Player_WorldBase>(_owner.lock()))));
    CHECK_FAILED(Add_State(PlayerState::Jump, Player_Jump::Create(static_pointer_cast<Player_WorldBase>(_owner.lock()))));
    CHECK_FAILED(Add_State(PlayerState::Parkour, Player_ParkourAction::Create(static_pointer_cast<Player_WorldBase>(_owner.lock()))));
    CHECK_FAILED(Add_State(PlayerState::Climb, Player_Climb::Create(static_pointer_cast<Player_WorldBase>(_owner.lock()))));
    CHECK_FAILED(Add_State(PlayerState::RopeAction, Player_RopeAction::Create(static_pointer_cast<Player_WorldBase>(_owner.lock()))));
    CHECK_FAILED(Add_State(PlayerState::GrappleAction, Player_GrappleAction::Create(static_pointer_cast<Player_WorldBase>(_owner.lock()))));
    CHECK_FAILED(Add_State(PlayerState::GameMenu, Player_GameMenu::Create(static_pointer_cast<Player_WorldBase>(_owner.lock()))));
    CHECK_FAILED(Add_State(PlayerState::FreeAim, Player_FreeAim::Create(static_pointer_cast<Player_WorldBase>(_owner.lock()))));
}

void PlayerStateMachineCom::BeginPlay_RegisteredStates()
{
    if (_statesBegunPlay)
        return;

    for (auto& [stateTag, state]: _states)
    {
        if (state != nullptr)
            state->Begin_Play();
    }

    _statesBegunPlay = true;
}

HRESULT PlayerStateMachineCom::Add_State(PlayerState playerState, const Shared<State>& state)
{
    const wstring& str = String::ToWString(magic_enum::enum_name(playerState).data());

    return StateMachineCom::Add_State(str, state);
}

bool PlayerStateMachineCom::Change_State(PlayerState playerState)
{
    const wstring& str = String::ToWString( magic_enum::enum_name(playerState).data());

    if (!_statesBegunPlay)
    {
        LOG_WARN("[PlayerStateMachine] Change_State ignored before registered states Begin_Play: {}",
            String::ToString(str));
        return false;
    }

    if (auto iter = _states.find(str); iter != _states.end())
    {
        if (Shared<Player_State> targetPlayerState = dynamic_pointer_cast<Player_State>(iter->second))
            targetPlayerState->Refresh_BodyPlayer();
    }
    
    return StateMachineCom::Change_State(str);
}

Shared<State> PlayerStateMachineCom::Get_State(PlayerState playerState) const
{
    const wstring stateName = String::ToWString(magic_enum::enum_name(playerState).data());

    auto iter = _states.find(stateName);
    if (iter == _states.end())
        return nullptr;

    return iter->second;
}

void PlayerStateMachineCom::Refresh_BodyPlayerForStates()
{
    for (auto& statePair : _states)
    {
        Shared<Player_State> playerState = dynamic_pointer_cast<Player_State>(statePair.second);
        if (playerState == nullptr)
            continue;

        playerState->Refresh_BodyPlayer();
    }
}

bool PlayerStateMachineCom::Reenter_CurrentState()
{
    if (_currentState == nullptr)
        return false;

    if (Shared<Player_State> playerState = dynamic_pointer_cast<Player_State>(_currentState))
        playerState->Refresh_BodyPlayer();

    return _currentState->EnterInvoke();
}

PlayerState PlayerStateMachineCom::Get_CurrentPlayerState() const
{
    auto str = String::ToString(_currentStateName);

    if (str.empty())
        return PlayerState::End;

    return magic_enum::enum_cast<PlayerState>(str).value();
}

void PlayerStateMachineCom::Enter_GameMenuAnimation(uint32 introAnim, uint32 idleAnim)
{
    if (Get_CurrentPlayerState() != PlayerState::GameMenu)
    {
        Change_State(PlayerState::GameMenu);
    }
    
    if (auto gameMenuState = static_pointer_cast<Player_GameMenu>(_currentState))
    {
        gameMenuState->Change_GameMenuAnimation(introAnim, idleAnim);
    }
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
    _statesBegunPlay = false;
    Component::Free();
}

NS_END
