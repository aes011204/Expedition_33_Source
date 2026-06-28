#pragma once

#include "SL_State.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

class SL_Skill final : public SL_State
{
public:
    explicit SL_Skill(const Shared<SL_Maelle>& owner);
    ~SL_Skill() override = default;

public:
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void ExitInvoke() override;

public:
    void Set_SkillRequest(const Skill_Data& skillData, uint32 slotIndex);
    const string& Get_CurrentSkillKey() const { return _skillData.skillKey; }

    bool Request_Dodge(const SL_MoveRequest& request) override;
    bool Request_Jump() override;

private:
    SoulLikeAnim Resolve_SkillAnim(uint32 slotIndex) const;
    void Resolve_PlaySection();
    bool Is_PlaySectionFinished() const;
    void Finish_Skill();

private:
    Skill_Data _skillData{};
    uint32 _slotIndex = 0;
    SoulLikeAnim _anim = SoulLikeAnim::Skill_1;
    float _playSectionStartSec = 0.f;
    float _playSectionEndSec = -1.f;

public:
    static Shared<SL_Skill> Create(const Shared<SL_Maelle>& owner);
};

NS_END
