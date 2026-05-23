#include "pch.h"
#include "AttackPlaying.h"

NS_BEGIN(Client)

AttackPlaying::AttackPlaying(const Shared<BattleController>& owner)
    : Battle_State(owner)
{
}

HRESULT AttackPlaying::Initialize()
{
    return Battle_State::Initialize();
}

void AttackPlaying::Begin_Play()
{
    Battle_State::Begin_Play();
}

bool AttackPlaying::EnterInvoke()
{
    LOG_DEBUG("AttackPlaying::EnterInvoke");
    return Battle_State::EnterInvoke();
}

void AttackPlaying::Update(float timeDelta)
{
    Battle_State::Update(timeDelta);

}

void AttackPlaying::LateUpdate(float timeDelta)
{
    Battle_State::LateUpdate(timeDelta);
}

void AttackPlaying::ExitInvoke()
{
    LOG_DEBUG("AttackPlaying::ExitInvoke");
    Battle_State::ExitInvoke();
}

Shared<AttackPlaying> AttackPlaying::Create(const Shared<BattleController>& owner)
{
    auto instance = make_shared<AttackPlaying>(owner);

    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}

void AttackPlaying::Free()
{
    Battle_State::Free();
}

NS_END
