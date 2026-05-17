#pragma once

#include "Monster_BattleBase.h"

NS_BEGIN(Client)

class Mon_Lancelier_Body;
class Mon_Lancelier_Weapon;

class MB_Lancelier final : public Monster_BattleBase
{
    GENERATED_GAMEOBJECT(MB_Lancelier, ObjectType::Monster)

public:
    enum class BattleAction
    {
        Idle,
        Attack
    };

public:
    MB_Lancelier(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MB_Lancelier(const MB_Lancelier& prototype);
    virtual ~MB_Lancelier() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Update(float timeDelta) override;
    void    Decide_Action(Action& curTurnAction, const vector<BattleChar>& heroes) override;

protected:
    HRESULT Ready_PartObjects();

private:
    Shared<Mon_Lancelier_Body>      _body = {};
    Shared<Mon_Lancelier_Weapon>    _weapon = {};

    BattleAction _currentAction = BattleAction::Idle;
    BattleAction _previousAction = BattleAction::Idle;

public:
    void Set_CurrentAction(BattleAction action) { _currentAction = action; }

public:
    static Shared<MB_Lancelier> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
