#pragma once
#include "Component.h"

NS_BEGIN(Client)
class SkillTreeCom : public Component
{
    GENERATED_COMPONENT(SkillTreeCom)

public:
    SkillTreeCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual ~SkillTreeCom();

public:
    bool IsUnlocked(const string& skillId);
    void TryUnlock(const string& skillId);
    bool CanUnlock(const string& skillId);
    bool HasRequiredSkill(const string& skillId);
    bool HasEnoughSkillPoint(const string& skillId);

public:
    static Shared<SkillTreeCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<Component>   Clone(void* arg) override;
    virtual void Free() override;

private:
    vector<string> _unlockedSkillId;
    
};
NS_END
