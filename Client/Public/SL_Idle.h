#pragma once

#include "SL_State.h"

NS_BEGIN(Client)

class SL_Idle : public SL_State
{
public:
    explicit SL_Idle(const Shared<SL_Maelle>& owner);
    ~SL_Idle() override = default;

public:
    bool EnterInvoke() override;
    void Update(float timeDelta) override;

    bool Request_Move(const SL_MoveRequest& request, float timeDelta) override;
    bool Request_BasicAttack() override;
    bool Request_Dodge(const SL_MoveRequest& request) override;
    bool Request_Skill(const Skill_Data& skillData, uint32 slotIndex) override;

    static Shared<SL_Idle> Create(const Shared<SL_Maelle>& owner);
};

NS_END
