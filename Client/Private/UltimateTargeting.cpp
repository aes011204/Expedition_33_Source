#include "pch.h"
#include "UltimateTargeting.h"

NS_BEGIN(Client)

UltimateTargeting::UltimateTargeting(const Shared<BattleController>& owner)
    : Battle_State{ owner }
{
}

UltimateTargeting::~UltimateTargeting()
{
}

HRESULT UltimateTargeting::Initialize()
{
    return Battle_State::Initialize();
}

void UltimateTargeting::Begin_Play()
{
    Battle_State::Begin_Play();
}

bool UltimateTargeting::EnterInvoke()
{
    return Battle_State::EnterInvoke();
}

void UltimateTargeting::Update(float timeDelta)
{
}

void UltimateTargeting::LateUpdate(float timeDelta)
{
}

void UltimateTargeting::ExitInvoke()
{
    Battle_State::ExitInvoke();
}

void UltimateTargeting::Free()
{
    Battle_State::Free();
}

Shared<UltimateTargeting> UltimateTargeting::Create(const Shared<BattleController>& owner)
{
    auto instance = make_shared<UltimateTargeting>(owner);

    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}

NS_END
