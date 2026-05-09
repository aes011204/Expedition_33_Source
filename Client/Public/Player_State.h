#pragma once
#include "State.h"

NS_BEGIN(Client)

class Player;
class Body_Player;

class PlayerControllerCom;
class PlayerStateMachineCom;
class InputCom;

class Player_State abstract : public State
{
public:
    Player_State(const Shared<Player>& owner);
    ~Player_State() override = default;

public:
    virtual HRESULT Initialize() override;
    virtual void Begin_Play() override;
    virtual bool EnterInvoke() override;
    virtual void Update(float timeDelta) override;
    virtual void LateUpdate(float timeDelta) override;
    virtual void ExitInvoke() override;

protected:
    Weak<Player>            _owner{};

    PlayerControllerCom* _controllerCom{ nullptr };
    PlayerStateMachineCom* _stateCom{ nullptr };

    Body_Player* _bodyPlayer{ nullptr };

public:
    virtual void Free() override;
    
};

NS_END
