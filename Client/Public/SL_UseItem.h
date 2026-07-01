#pragma once

#include "SL_State.h"

NS_BEGIN(Client)

class SL_UseItem final : public SL_State
{
public:
    explicit SL_UseItem(const Shared<SL_Maelle>& owner);
    ~SL_UseItem() override = default;

public:
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void ExitInvoke() override;

public:
    static Shared<SL_UseItem> Create(const Shared<SL_Maelle>& owner);

private:
    bool _appliedPotion = false;
};

NS_END
