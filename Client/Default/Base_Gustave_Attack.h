#pragma once
#include "SkillScript.h"

NS_BEGIN(Client)
class Base_Gustave_Attack :
    public SkillScript
{
     GENERATED_SKILLSCRIPT(Base_Gustave_Attack)

public:
    Base_Gustave_Attack();
    virtual ~Base_Gustave_Attack() = default;
public:
    virtual void Execute(const Skill_Data& skill,
        const Action& action);

private:
	uint32 charging = {};
public:
    static Shared<Base_Gustave_Attack> Create();
    //Shared<Component> Clone(void* arg) override;
    void Free() override;
private:
    uint32 CurIndex = {};
};

NS_END
