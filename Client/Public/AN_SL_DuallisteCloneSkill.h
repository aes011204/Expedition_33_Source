#pragma once

#include "AnimNotify.h"

NS_BEGIN(Engine)
class ContainerObject;
NS_END

NS_BEGIN(Client)

class SL_Base;
class SL_Dualliste;

class AN_SL_DuallisteCloneSkill final : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_SL_DuallisteCloneSkill)

public:
    AN_SL_DuallisteCloneSkill() = default;
    ~AN_SL_DuallisteCloneSkill() override = default;

public:
    string Get_TypeName() const override { return "AN_SL_DuallisteCloneSkill"; }
    void Execute(const AnimNotifyContext& context) override;

private:
    Shared<SL_Dualliste> Resolve_OwnerDualliste(GameObject* notifyOwner) const;
    Shared<SL_Base>      Resolve_Target(const Shared<SL_Dualliste>& owner) const;

private:
    string _prefabName = "";
    string _skillKey = "";
    string _animationName = "";
    Vec3   _spawnOffset = Vec3::Zero;
    float  _goalOffset = 1.8f;
    float  _fallbackDamageMultiplier = 1.f;
    float  _animationSpeed = 1.f;
    float  _moveSpeed = 0.f;
    float  _dissolveDuration = 0.35f;
};

NS_END
