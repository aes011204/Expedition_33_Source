#include "pch.h"
#include "ChooseUltimate.h"

NS_BEGIN(Client)

ChooseUltimate::ChooseUltimate(const Shared<BattleController>& owner)
    : Battle_State{ owner }
{
}

ChooseUltimate::~ChooseUltimate()
{
}

HRESULT ChooseUltimate::Initialize()
{
    return Battle_State::Initialize();
}

void ChooseUltimate::Begin_Play()
{
    Battle_State::Begin_Play();
}

bool ChooseUltimate::EnterInvoke()
{
    LOG_DEBUG("ChooseUltimate::EnterInvoke");
    return Battle_State::EnterInvoke();
}

void ChooseUltimate::Update(float timeDelta)
{
    Battle_State::Update(timeDelta);
}

void ChooseUltimate::LateUpdate(float timeDelta)
{
    Battle_State::LateUpdate(timeDelta);
}

void ChooseUltimate::ExitInvoke()
{
    LOG_DEBUG("ChooseUltimate::ExitInvoke");
    Battle_State::ExitInvoke();
}

void ChooseUltimate::Free()
{
    Battle_State::Free();
}

Shared<ChooseUltimate> ChooseUltimate::Create(const Shared<BattleController>& owner)
{
    auto instance = make_shared<ChooseUltimate>(owner);

    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}

NS_END
