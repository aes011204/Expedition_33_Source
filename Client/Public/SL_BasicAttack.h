#pragma once

#include "SL_State.h"

NS_BEGIN(Client)

class SL_BasicAttack final : public SL_State
{
public:
    explicit SL_BasicAttack(const Shared<SL_Maelle>& owner);
    ~SL_BasicAttack() override = default;

public:
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void ExitInvoke() override;

    bool Request_BasicAttack() override;
    bool Request_Skill(const Skill_Data& skillData, uint32 slotIndex) override;
    bool Request_Parry() override;

    bool Request_Dodge(const SL_MoveRequest& request) override;
    bool Request_Jump() override;

public:
    void Set_ComboWindowEnabled(bool enabled);

private:
    void Play_CurrentCombo();
    SoulLikeAnim Get_CurrentComboAnim() const;
    bool Can_AdvanceCombo() const;
    void Finish_Attack();

private:
    uint32 _comboIndex = 0;
    bool _comboWindowEnabled = false;
    bool _comboQueued = false;

    // 최대 3타
    static constexpr uint32 ComboMaxCount = 3;

public:
    static Shared<SL_BasicAttack> Create(const Shared<SL_Maelle>& owner);


};

NS_END
