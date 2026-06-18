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
    HRESULT Initialize(void* arg) override;

public:
    bool IsUnlocked(const string& skillId);
    void TryUnlock(const string& skillId);
    bool CanUnlock(const string& skillId);
    bool HasRequiredSkill(const string& skillId);
    bool HasEnoughSkillPoint(const string& skillId);

    // TEMP : 테스트용 강제해금
    void ForceUnlock(const string& skillId);

    const vector<string>& Get_UnlockedSkillIds() const { return _unlockedSkillId; } // Character Progression이 해금 스킬 목록을 표시할 때만 읽는다.

private:
    vector<string> _unlockedSkillId;

public:
    static Shared<SkillTreeCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<Component>   Clone(void* arg) override;
    virtual void Free() override;
};
NS_END
