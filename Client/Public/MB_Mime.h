#pragma once

#include "Monster_BattleBase.h"

NS_BEGIN(Client)

class Mon_WeakPoint;
class Mon_Mime_Body;
class Mon_Mime_Bagutte;

class MB_Mime final : public Monster_BattleBase
{
    GENERATED_GAMEOBJECT(MB_Mime, ObjectType::Monster)

public:
    MB_Mime(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MB_Mime(const MB_Mime& prototype);
    ~MB_Mime() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;

public:
    void Adjust_IncomingBattleDamage(
        const BattleChar& attacker,
        float& damage,
        AttackType attackType) const override;

    bool Try_ApplyBattleBreakOnDamage(const DamageAppliedInfo& info) override;

protected:
    HRESULT Ready_PartObjects();

private:
    Shared<Mon_Mime_Body> _body = {};
    Shared<Mon_Mime_Bagutte> _bagutte = {};

public:
    static Shared<MB_Mime> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
