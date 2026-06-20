#pragma once
#include "Battle_Enum.h"
#include "Battle_State.h"

NS_BEGIN(Client)

class BattleController;

class SkillTargeting : public Battle_State
{
public:
    SkillTargeting(const Shared<BattleController>& owner);
    ~SkillTargeting() override;

public:
    HRESULT Initialize() override;
    void    Begin_Play() override;
    bool    EnterInvoke() override;
    void    Update(float timeDelta) override;
    void    LateUpdate(float timeDelta) override;
    void    ExitInvoke() override;

private:
    bool    Try_MoveTargetIndex(int32 direction, size_t enemyCount);
    bool    Requires_SkillTarget(const Skill_Data* skillData);

private:
    uint32 _targetIndex = {};

public:
    static Shared<SkillTargeting> Create(const Shared<BattleController>& owner);
    void Free() override;
};

NS_END
