#pragma once

#include "Character.h"

NS_BEGIN(Engine)
class ColliderCom;
NS_END

NS_BEGIN(Client)

enum class AttackType;
struct BattleChar;
struct DamageAppliedInfo;

class Character_BattleBase abstract : public Character
{
public:
    Character_BattleBase(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Character_BattleBase(const Character_BattleBase& prototype);
    virtual ~Character_BattleBase() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;
    void    Late_Update(float timeDelta) override;

public:
    virtual void Adjust_IncomingBattleDamage(
        const BattleChar& attacker,
        float& damage,
        AttackType attackType) const;

    virtual bool Try_ApplyBattleBreakOnDamage(const DamageAppliedInfo& info);

    void    Set_BattleColliderActive(bool isActive);
    bool    Is_BattleColliderActive() const;

    bool    Try_GetDamageFeedbackWorldPosition(Vec3& outWorldPos) const;

protected:
    virtual bool Try_GetDamageFeedbackSocketWorldPosition(Vec3& outWorldPos) const;
    virtual Vec3 Get_DamageFeedbackFallbackOffset() const;

protected:
    HRESULT Ready_Collider(const wstring& prototypeTag, void* colliderDesc, Collision_Preset preset);
    void    Update_Collider();



protected:
    Shared<ColliderCom> _colliderCom;

public:
    void Free() override;

};

NS_END
