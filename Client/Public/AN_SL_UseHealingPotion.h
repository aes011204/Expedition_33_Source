#pragma once

#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_SL_UseHealingPotion final : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_SL_UseHealingPotion)

public:
    AN_SL_UseHealingPotion() = default;
    ~AN_SL_UseHealingPotion() override = default;

public:
    string Get_TypeName() const override { return "AN_SL_UseHealingPotion"; }
    void Execute(const AnimNotifyContext& context) override;

private:
    Shared<class SL_Maelle> Resolve_Maelle(GameObject* notifyOwner) const;
};

NS_END
