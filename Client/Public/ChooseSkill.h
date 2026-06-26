#pragma once

#include "Battle_State.h"
#include "Battle_Event.h"
#include "Event_Bus.h"

NS_BEGIN(Client)

class BattleController;
class CombatStatCom;

class ChooseSkill : public Battle_State
{
public:
    ChooseSkill(const Shared<BattleController>& owner);
    ~ChooseSkill() override;

public:
    HRESULT Initialize() override;
    void Begin_Play() override;
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;

private:
    bool Has_SecondSkillPage(const Shared<CombatStatCom>& statCom) const;

public:
    static Shared<ChooseSkill> Create(const Shared<BattleController>& owner);
    void Free() override;
};

NS_END
