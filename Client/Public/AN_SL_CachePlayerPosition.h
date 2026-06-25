#pragma once

#include "AnimNotify.h"

NS_BEGIN(Engine)
class ContainerObject;
NS_END

NS_BEGIN(Client)

class SL_Dualliste;

class AN_SL_CachePlayerPosition final : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_SL_CachePlayerPosition)

public:
    AN_SL_CachePlayerPosition() = default;
    ~AN_SL_CachePlayerPosition() override = default;

public:
    string Get_TypeName() const override { return "AN_SL_CachePlayerPosition"; }
    void Execute(const AnimNotifyContext& context) override;

private:
    Shared<SL_Dualliste> Resolve_OwnerDualliste(GameObject* notifyOwner) const;
};

NS_END
