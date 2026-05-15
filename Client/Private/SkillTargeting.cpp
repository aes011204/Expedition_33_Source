#include "pch.h"
#include "SkillTargeting.h"

NS_BEGIN(Client)

SkillTargeting::SkillTargeting(const Shared<BattleController>& owner)
    : Battle_State{ owner }
{
}

SkillTargeting::~SkillTargeting()
{
}

HRESULT SkillTargeting::Initialize()
{
    return Battle_State::Initialize();
}

void SkillTargeting::Begin_Play()
{
    Battle_State::Begin_Play();
}

bool SkillTargeting::EnterInvoke()
{
    LOG_DEBUG("SkillTargeting::EnterInvoke");
    return Battle_State::EnterInvoke();
}

void SkillTargeting::Update(float timeDelta)
{
    Battle_State::Update(timeDelta);
}

void SkillTargeting::LateUpdate(float timeDelta)
{
    Battle_State::LateUpdate(timeDelta);
}

void SkillTargeting::ExitInvoke()
{
    LOG_DEBUG("SkillTargeting::ExitInvoke");
    Battle_State::ExitInvoke();
}

void SkillTargeting::Free()
{
    Battle_State::Free();
}

Shared<SkillTargeting> SkillTargeting::Create(const Shared<BattleController>& owner)
{
    auto instance = make_shared<SkillTargeting>(owner);

    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}

NS_END
