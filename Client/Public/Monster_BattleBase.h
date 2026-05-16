#pragma once

#include "Character_BattleBase.h"
#include "Battle_Enum.h"

NS_BEGIN(Engine)
class ColliderCom;
class AIController;
NS_END

NS_BEGIN(Client)

class MonsterStatCom;

class Monster_BattleBase abstract : public Character_BattleBase
{
public:
    Monster_BattleBase(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Monster_BattleBase(const Monster_BattleBase& prototype);
    virtual ~Monster_BattleBase() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;

    virtual void Decide_Action(Action& curTurnAction, const vector<BattleChar>& heroes);

protected:
    virtual HRESULT Ready_Components();

protected:
    Shared<AIController>            _aiCtrl;
    Shared<MonsterStatCom>          _combatStat;

public:
    void Free() override;

};

NS_END
