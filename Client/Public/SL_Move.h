#pragma once

#include "SL_State.h"

NS_BEGIN(Client)

class SL_Move : public SL_State
{
public:
    explicit SL_Move(const Shared<SL_Maelle>& owner);
    ~SL_Move() override = default;

public:
    bool EnterInvoke() override;
    bool Request_Move(const SL_MoveRequest& request, float timeDelta) override;
    bool Request_BasicAttack() override;
    bool Request_Dodge(const SL_MoveRequest& request) override;
    bool Request_Skill(const Skill_Data& skillData, uint32 slotIndex) override;

public:
    static Shared<SL_Move> Create(const Shared<SL_Maelle>& owner);
};

NS_END
