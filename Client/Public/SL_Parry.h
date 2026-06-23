#pragma once

#include "SL_State.h"

NS_BEGIN(Client)

class SL_Parry final : public SL_State
{
public:
    explicit SL_Parry(const Shared<SL_Maelle>& owner);
    ~SL_Parry() override = default;

public:
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void ExitInvoke() override;

private:
    SoulLikeAnim Resolve_ParryAnim() const;

public:
    static Shared<SL_Parry> Create(const Shared<SL_Maelle>& owner);
};

NS_END
