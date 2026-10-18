#pragma once
#include "Weapon.h"

NS_BEGIN(Engine)
class ColliderCom;
NS_END

NS_BEGIN(Client)

class SL_Weapon final : public Weapon
{
    GENERATED_GAMEOBJECT(SL_Weapon, ObjectType::Weapon)

public:
    SL_Weapon(const ComPtr<Device>& device, const ComPtr<Context>& context);
    SL_Weapon(const SL_Weapon& prototype);
    ~SL_Weapon() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Update(float timeDelta) override;
    void    OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;

public:
    Shared<ColliderCom> Get_AttackCollider() const { return _attackColliderCom; }

private:
    HRESULT Ready_AttackCollider();
    void    Sync_AttackCollider();

private:
    Shared<ColliderCom> _attackColliderCom{ nullptr };

public:
    static Shared<SL_Weapon> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END

