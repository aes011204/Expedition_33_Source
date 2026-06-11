#pragma once

#include "Base.h"
#include "BStateMachine.h"
#include "Battle_Enum.h"
#include "Client_Defines.h"

NS_BEGIN(Client)

DECLARE_DELEGATE(FOnBattleTurnStateChanged, BattleTurnState /*prev*/, BattleTurnState /*next*/);

class BattleStateMachine : public BStateMachine
{
public:
    BattleStateMachine();
    //BattleStateMachine(const BattleStateMachine& prototype);
    virtual ~BattleStateMachine()  = default;

public:
    HRESULT Initialize_Prototype() ;
    HRESULT Initialize(void* arg);
    //void BeginPlay() override;

public:
    HRESULT Add_State(BattleTurnState playerState, const Shared<State>& state);
    bool Change_State(BattleTurnState playerState);

public:
    BattleTurnState Get_CurrentPlayerState() const;

public:
    FOnBattleTurnStateChanged OnBattleTurnStateChanged;

public:
    static Shared<BattleStateMachine> Create();
  //  virtual Shared<Component> Clone(void* arg) = 0;
    virtual void Free() override;
};

NS_END
