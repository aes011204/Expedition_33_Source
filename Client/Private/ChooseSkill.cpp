#include "pch.h"
#include "ChooseSkill.h"

NS_BEGIN(Client)

ChooseSkill::ChooseSkill(const Shared<BattleController>& owner)
    : Battle_State{ owner }
{
}

ChooseSkill::~ChooseSkill()
{
}

HRESULT ChooseSkill::Initialize()
{
    return Battle_State::Initialize();
}

void ChooseSkill::Begin_Play()
{
    Battle_State::Begin_Play();
}

bool ChooseSkill::EnterInvoke()
{
    return Battle_State::EnterInvoke();
}

void ChooseSkill::Update(float timeDelta)
{
}

void ChooseSkill::LateUpdate(float timeDelta)
{
}

void ChooseSkill::ExitInvoke()
{
    Battle_State::ExitInvoke();
}

void ChooseSkill::Free()
{
    Battle_State::Free();
}

Shared<ChooseSkill> ChooseSkill::Create(const Shared<BattleController>& owner)
{
    auto instance = make_shared<ChooseSkill>(owner);

    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}

NS_END
