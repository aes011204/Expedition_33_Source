#include "SL_Idle.h"

SL_Idle::SL_Idle(const Shared<SL_Maelle>& owner)
{
}

bool SL_Idle::EnterInvoke()
{
    return SL_State::EnterInvoke();
}

void SL_Idle::Update(float timeDelta)
{
    SL_State::Update(timeDelta);
}

bool SL_Idle::Request_Move(const Vec3& worldDir, bool sprint, float timeDelta)
{
    return SL_State::Request_Move(worldDir, sprint, timeDelta);
}

bool SL_Idle::Request_BasicAttack()
{
    return SL_State::Request_BasicAttack();
}

bool SL_Idle::Request_Dodge()
{
    return SL_State::Request_Dodge();
}

bool SL_Idle::Request_Skill(const Skill_Data& skillData)
{
    return SL_State::Request_Skill(skillData);
}

Shared<SL_Idle> SL_Idle::Create(const Shared<SL_Maelle>& owner)
{
}
