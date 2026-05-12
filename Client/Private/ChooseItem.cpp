#include "pch.h"
#include "ChooseItem.h"

NS_BEGIN(Client)

ChooseItem::ChooseItem(const Shared<BattleController>& owner)
    : Battle_State{ owner }
{
}

ChooseItem::~ChooseItem()
{
}

HRESULT ChooseItem::Initialize()
{
    return Battle_State::Initialize();
}

void ChooseItem::Begin_Play()
{
    Battle_State::Begin_Play();
}

bool ChooseItem::EnterInvoke()
{
    return Battle_State::EnterInvoke();
}

void ChooseItem::Update(float timeDelta)
{
}

void ChooseItem::LateUpdate(float timeDelta)
{
}

void ChooseItem::ExitInvoke()
{
    Battle_State::ExitInvoke();
}

void ChooseItem::Free()
{
    Battle_State::Free();
}

Shared<ChooseItem> ChooseItem::Create(const Shared<BattleController>& owner)
{
    auto instance = make_shared<ChooseItem>(owner);

    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}

NS_END
