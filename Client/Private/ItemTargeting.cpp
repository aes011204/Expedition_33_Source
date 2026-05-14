#include "pch.h"
#include "ItemTargeting.h"

NS_BEGIN(Client)

ItemTargeting::ItemTargeting(const Shared<BattleController>& owner)
    : Battle_State{ owner }
{
}

ItemTargeting::~ItemTargeting()
{
}

HRESULT ItemTargeting::Initialize()
{
    return Battle_State::Initialize();
}

void ItemTargeting::Begin_Play()
{
    Battle_State::Begin_Play();
}

bool ItemTargeting::EnterInvoke()
{
    return Battle_State::EnterInvoke();
}

void ItemTargeting::Update(float timeDelta)
{
}

void ItemTargeting::LateUpdate(float timeDelta)
{
}

void ItemTargeting::ExitInvoke()
{
    Battle_State::ExitInvoke();
}

void ItemTargeting::Free()
{
    Battle_State::Free();
}

Shared<ItemTargeting> ItemTargeting::Create(const Shared<BattleController>& owner)
{
    auto instance = make_shared<ItemTargeting>(owner);

    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}

NS_END
