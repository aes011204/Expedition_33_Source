#pragma once
#include "State.h"

NS_BEGIN(Client)

class BattleController;
class BattleStateMachine;

class Battle_State : public State
{
public:
    Battle_State(const Shared<BattleController>& owner);
    ~Battle_State() override;

public:
    HRESULT Initialize() override;

    void Begin_Play() override;
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;

public:
    Shared<BattleController> Get_Owner() const { return _owner.lock(); }
    BattleStateMachine* Get_BattleFSM() const { return _battleFSM; }



protected:
    Weak<BattleController> _owner{};
    BattleStateMachine* _battleFSM = nullptr;

public:
    void Free() override;
};

NS_END
