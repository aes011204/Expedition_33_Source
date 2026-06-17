#include "SL_StateMachineCom.h"

IMPLEMENT_REFLECTION(SL_StateMachineCom)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    
    return true;
}

SL_StateMachineCom::SL_StateMachineCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
}

SL_StateMachineCom::SL_StateMachineCom(const StateMachineCom& prototype)
{
}

HRESULT SL_StateMachineCom::Initialize_Prototype()
{
    return StateMachineCom::Initialize_Prototype();
}

HRESULT SL_StateMachineCom::Initialize(void* arg)
{
    return StateMachineCom::Initialize(arg);
}

void SL_StateMachineCom::BeginPlay()
{
    StateMachineCom::BeginPlay();
}

HRESULT SL_StateMachineCom::Add_State(SoulLikePlayerState playerState, const Shared<State>& state)
{
}

bool SL_StateMachineCom::Change_State(SoulLikePlayerState playerState)
{
}

Shared<Client::State> SL_StateMachineCom::Get_State(SoulLikePlayerState playerState) const
{
}

SoulLikePlayerState SL_StateMachineCom::Get_CurrentSoulLikeState() const
{
}

bool SL_StateMachineCom::Request_Move(const Vec3& worldDir, bool sprint, float timeDelta)
{
}

bool SL_StateMachineCom::Request_BasicAttack()
{
}

bool SL_StateMachineCom::Request_Dodge()
{
}

bool SL_StateMachineCom::Request_Skill(const Skill_Data& skillData)
{
}

Shared<SL_Maelle> SL_StateMachineCom::Get_OwnerMaelle() const
{
}

Shared<SL_StateMachineCom> SL_StateMachineCom::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
}

Shared<Component> SL_StateMachineCom::Clone(void* arg)
{
}

void SL_StateMachineCom::Free()
{
}
