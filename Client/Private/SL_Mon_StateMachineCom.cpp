#include "SL_Mon_StateMachineCom.h"

#include "SL_Dualliste.h"
#include "SL_Mon_AttackState.h"
#include "SL_Mon_ChaseState.h"
#include "SL_Mon_DeadState.h"
#include "SL_Mon_HitState.h"
#include "SL_Mon_IdleState.h"
#include "SL_Mon_IntroState.h"
#include "SL_Mon_StunState.h"
#include "SL_Mon_State.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(SL_Mon_StateMachineCom)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "SL_Mon_StateMachine";
    info.category = "SoulLike";

    return true;
}

SL_Mon_StateMachineCom::SL_Mon_StateMachineCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : StateMachineCom(device, context)
{
}

SL_Mon_StateMachineCom::SL_Mon_StateMachineCom(const StateMachineCom& prototype)
    : StateMachineCom(prototype)
{
}

HRESULT SL_Mon_StateMachineCom::Initialize_Prototype()
{
    return StateMachineCom::Initialize_Prototype();
}

HRESULT SL_Mon_StateMachineCom::Initialize(void* arg)
{
    CHECK_FAILED(StateMachineCom::Initialize(arg), E_FAIL);

    return S_OK;
}

void SL_Mon_StateMachineCom::BeginPlay()
{
    StateMachineCom::BeginPlay();

    Shared<SL_Dualliste> owner = Get_OwnerDualliste();
    if (owner == nullptr)
        return;

    CHECK_FAILED(Add_State(SL_MonState::Intro, SL_Mon_IntroState::Create(owner)));
    CHECK_FAILED(Add_State(SL_MonState::Idle, SL_Mon_IdleState::Create(owner)));
    CHECK_FAILED(Add_State(SL_MonState::Chase, SL_Mon_ChaseState::Create(owner)));
    CHECK_FAILED(Add_State(SL_MonState::Attack, SL_Mon_AttackState::Create(owner)));
    CHECK_FAILED(Add_State(SL_MonState::Hit, SL_Mon_HitState::Create(owner)));
    CHECK_FAILED(Add_State(SL_MonState::Dead, SL_Mon_DeadState::Create(owner)));
    CHECK_FAILED(Add_State(SL_MonState::Stun, SL_Mon_StunState::Create(owner)));

    for (auto& [stateTag, state] : _states)
    {
        UNREFERENCED_PARAMETER(stateTag);
        state->Begin_Play();
    }
}

HRESULT SL_Mon_StateMachineCom::Add_State(SL_MonState monState, const Shared<SL_Mon_State>& state)
{
    const wstring stateName = String::ToWString(magic_enum::enum_name(monState).data());

    return StateMachineCom::Add_State(stateName, state);
}

bool SL_Mon_StateMachineCom::Change_State(SL_MonState monState)
{
    const wstring stateName = String::ToWString(magic_enum::enum_name(monState).data());

    return StateMachineCom::Change_State(stateName);
}

Shared<SL_Mon_State> SL_Mon_StateMachineCom::Get_State(SL_MonState monState) const
{
    const wstring stateName = String::ToWString(magic_enum::enum_name(monState).data());

    auto iter = _states.find(stateName);
    if (iter == _states.end())
        return nullptr;

    return static_pointer_cast<SL_Mon_State>(iter->second);
}

Shared<SL_Mon_State> SL_Mon_StateMachineCom::Get_CurrentMonState() const
{
    if (_currentState == nullptr)
        return nullptr;

    return static_pointer_cast<SL_Mon_State>(_currentState);
}

SL_MonState SL_Mon_StateMachineCom::Get_CurrentMonStateType() const
{
    const string stateName = String::ToString(_currentStateName);
    if (stateName.empty())
        return SL_MonState::End;

    auto state = magic_enum::enum_cast<SL_MonState>(stateName);
    if (!state.has_value())
        return SL_MonState::End;

    return state.value();
}

Shared<SL_Dualliste> SL_Mon_StateMachineCom::Get_OwnerDualliste() const
{
    if (_owner.expired())
        return nullptr;

    return dynamic_pointer_cast<SL_Dualliste>(_owner.lock());
}

Shared<SL_Mon_StateMachineCom> SL_Mon_StateMachineCom::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<SL_Mon_StateMachineCom>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : SL_Mon_StateMachineCom");
        return nullptr;
    }

    return instance;
}

Shared<Component> SL_Mon_StateMachineCom::Clone(void* arg)
{
    auto clone = make_shared<SL_Mon_StateMachineCom>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : SL_Mon_StateMachineCom");
        return nullptr;
    }

    return clone;
}

void SL_Mon_StateMachineCom::Free()
{
    StateMachineCom::Free();
}

NS_END
