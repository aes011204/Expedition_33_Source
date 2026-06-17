#pragma once

#include "SL_State.h"

class SL_Idle : public SL_State
{
public:
    explicit SL_Idle(const Shared<SL_Maelle>& owner);

    bool EnterInvoke() override;
    void Update(float timeDelta) override;

    bool Request_Move(const Vec3& worldDir, bool sprint, float timeDelta) override;
    bool Request_BasicAttack() override;
    bool Request_Dodge() override;
    bool Request_Skill(const Skill_Data& skillData) override;

    static Shared<SL_Idle> Create(const Shared<SL_Maelle>& owner);
};

