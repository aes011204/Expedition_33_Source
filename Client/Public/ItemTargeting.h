#pragma once

#include "Battle_State.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

class BattleController;

class ItemTargeting : public Battle_State
{
public:
    ItemTargeting(const Shared<BattleController>& owner);
    ~ItemTargeting() override;

public:
    HRESULT Initialize() override;
    void    Begin_Play() override;
    bool    EnterInvoke() override;
    void    Update(float timeDelta) override;
    void    LateUpdate(float timeDelta) override;
    void    ExitInvoke() override;

private:
    bool    Set_TargetCandidates();
    bool    Try_MoveTargetIndex(int32 direction);
    bool    Is_ValidTarget(const BattleChar& target) const;

private:
    vector<BattleChar> _targetCandidates{};
    uint32 _targetIndex = 0;

public:
    static Shared<ItemTargeting> Create(const Shared<BattleController>& owner);
    void Free() override;
};

NS_END
