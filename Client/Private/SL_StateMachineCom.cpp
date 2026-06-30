#include "SL_StateMachineCom.h"
#include "SL_Maelle.h"

#include "SL_BasicAttack.h"
#include "SL_Dead.h"
#include "SL_Dodge.h"
#include "SL_Hit.h"
#include "SL_Idle.h"
#include "SL_Jump.h"
#include "SL_JumpCounter.h"
#include "SL_Move.h"
#include "SL_Parry.h"
#include "SL_Skill.h"
#include "SL_State.h"
#include "SL_UseItem.h"

IMPLEMENT_REFLECTION(SL_StateMachineCom)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "SL_StateMachine";
    info.category = "SoulLike";

    return true;
}

SL_StateMachineCom::SL_StateMachineCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : StateMachineCom(device, context)
{
}

SL_StateMachineCom::SL_StateMachineCom(const StateMachineCom& prototype)
    : StateMachineCom(prototype)
{
}

HRESULT SL_StateMachineCom::Initialize_Prototype()
{
    return StateMachineCom::Initialize_Prototype();
}

HRESULT SL_StateMachineCom::Initialize(void* arg)
{
    CHECK_FAILED(StateMachineCom::Initialize(arg), E_FAIL);

    return S_OK;
}

void SL_StateMachineCom::BeginPlay()
{
    StateMachineCom::BeginPlay();

    Shared<SL_Maelle> owner = Get_OwnerMaelle();
    if (owner == nullptr)
        return;

    CHECK_FAILED(Add_State(SL_PlayerState::Idle, SL_Idle::Create(owner)));
    CHECK_FAILED(Add_State(SL_PlayerState::Run, SL_Move::Create(owner)));
    CHECK_FAILED(Add_State(SL_PlayerState::BasicAttack, SL_BasicAttack::Create(owner)));
    CHECK_FAILED(Add_State(SL_PlayerState::Dodge, SL_Dodge::Create(owner)));
    CHECK_FAILED(Add_State(SL_PlayerState::Hit, SL_Hit::Create(owner)));
    CHECK_FAILED(Add_State(SL_PlayerState::Dead, SL_Dead::Create(owner)));
    CHECK_FAILED(Add_State(SL_PlayerState::Skill, SL_Skill::Create(owner)));
    CHECK_FAILED(Add_State(SL_PlayerState::Parry, SL_Parry::Create(owner)));
    CHECK_FAILED(Add_State(SL_PlayerState::Jump, SL_Jump::Create(owner)));
    CHECK_FAILED(Add_State(SL_PlayerState::JumpCounter, SL_JumpCounter::Create(owner)));
    CHECK_FAILED(Add_State(SL_PlayerState::UseItem, SL_UseItem::Create(owner)));

    for (auto& [stateTag, state] : _states)
    {
        state->Begin_Play();
    }
}

HRESULT SL_StateMachineCom::Add_State(SL_PlayerState playerState, const Shared<SL_State>& state)
{
    const wstring stateName = String::ToWString(magic_enum::enum_name(playerState).data());

    return StateMachineCom::Add_State(stateName, state);
}

bool SL_StateMachineCom::Change_State(SL_PlayerState playerState)
{
    const wstring stateName = String::ToWString(magic_enum::enum_name(playerState).data());

    return StateMachineCom::Change_State(stateName);
}

Shared<SL_State> SL_StateMachineCom::Get_State(SL_PlayerState playerState) const
{
    const wstring stateName = String::ToWString(magic_enum::enum_name(playerState).data());

    auto iter = _states.find(stateName);
    if (iter == _states.end())
        return nullptr;

    return static_pointer_cast<SL_State>(iter->second);
}

Shared<SL_State> SL_StateMachineCom::Get_CurrentSLState() const
{
    if (_currentState == nullptr)
        return nullptr;

    return static_pointer_cast<SL_State>(_currentState);
}

SL_PlayerState SL_StateMachineCom::Get_CurrentSoulLikeState() const
{
    const string stateName = String::ToString(_currentStateName);
    if (stateName.empty())
        return SL_PlayerState::End;

    auto state = magic_enum::enum_cast<SL_PlayerState>(stateName);
    if (!state.has_value())
        return SL_PlayerState::End;

    return state.value();
}

bool SL_StateMachineCom::Request_Move(const SL_MoveRequest& request, float timeDelta)
{
    Shared<SL_State> state = Get_CurrentSLState();
    if (state == nullptr)
        return false;

    return state->Request_Move(request, timeDelta);
}

bool SL_StateMachineCom::Request_BasicAttack()
{
    Shared<SL_State> state = Get_CurrentSLState();
    if (state == nullptr)
        return false;

    return state->Request_BasicAttack();
}

bool SL_StateMachineCom::Request_Dodge(const SL_MoveRequest& request)
{
    Shared<SL_State> state = Get_CurrentSLState();
    if (state == nullptr)
        return false;

    auto dodgeState = dynamic_pointer_cast<SL_Dodge>(Get_State(SL_PlayerState::Dodge));
    if (dodgeState != nullptr)
        dodgeState->Set_DodgeRequest(request);

    return state->Request_Dodge(request);
}

bool SL_StateMachineCom::Request_Parry()
{
    Shared<SL_State> state = Get_CurrentSLState();
    if (state == nullptr)
        return false;

    return state->Request_Parry();
}

bool SL_StateMachineCom::Request_Skill(const Skill_Data& skillData, uint32 slotIndex)
{
    Shared<SL_State> state = Get_CurrentSLState();
    if (state == nullptr)
        return false;

    auto skillState = dynamic_pointer_cast<SL_Skill>(Get_State(SL_PlayerState::Skill));
    if (skillState != nullptr)
        skillState->Set_SkillRequest(skillData, slotIndex);

    return state->Request_Skill(skillData, slotIndex);
}

bool SL_StateMachineCom::Request_Jump()
{
    Shared<SL_State> state = Get_CurrentSLState();
    if (state == nullptr)
        return false;

    return state->Request_Jump();
}

bool SL_StateMachineCom::Request_JumpCounter()
{
    Shared<SL_State> state = Get_CurrentSLState();
    if (state == nullptr)
        return false;

    return state->Request_JumpCounter();
}

void SL_StateMachineCom::Notify_BasicAttackComboWindow(bool enabled)
{
    if (Get_CurrentSoulLikeState() != SL_PlayerState::BasicAttack)
        return;

    auto basicAttack = dynamic_pointer_cast<SL_BasicAttack>(Get_State(SL_PlayerState::BasicAttack));
    if (basicAttack == nullptr)
        return;

    basicAttack->Set_ComboWindowEnabled(enabled);
}

void SL_StateMachineCom::Set_CanActionCancel(bool enabled, bool canJump, bool canDodge)
{
    _canActionJump = enabled && canJump;
    _canActionDodge = enabled && canDodge;
}

void SL_StateMachineCom::Clear_CanActionCancel()
{
    _canActionJump = false;
    _canActionDodge = false;
}

Shared<SL_Maelle> SL_StateMachineCom::Get_OwnerMaelle() const
{
    if (_owner.expired())
        return nullptr;

    return dynamic_pointer_cast<SL_Maelle>(_owner.lock());
}

Shared<SL_StateMachineCom> SL_StateMachineCom::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<SL_StateMachineCom>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : SL_StateMachineCom");
        return nullptr;
    }

    return instance;
}

Shared<Component> SL_StateMachineCom::Clone(void* arg)
{
    auto clone = make_shared<SL_StateMachineCom>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : SL_StateMachineCom");
        return nullptr;
    }

    return clone;
}

void SL_StateMachineCom::Free()
{
    StateMachineCom::Free();
}
