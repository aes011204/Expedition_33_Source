#pragma once

#include "Character_WorldBase.h"
#include "SL_Types.h"

NS_BEGIN(Engine)
class ColliderCom;
NS_END

NS_BEGIN(Client)

class CombatStatCom;

class SL_Base abstract : public Character_WorldBase
{
protected:
    explicit SL_Base(ComPtr<Device> device, ComPtr<Context> context);
    explicit SL_Base(const SL_Base& prototype);
    ~SL_Base() override = default;

public:
    // Stat Com
    virtual Shared<CombatStatCom> Get_Stat() const = 0;
    // Collider Com
    virtual Shared<ColliderCom>   Get_AttackCollider() const = 0;

    virtual bool Is_Dead() const = 0;
    virtual void Apply_Damage(const SL_DamageInfo& damageInfo) = 0;

};

NS_END
