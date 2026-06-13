#pragma once

#include "Monster_WorldBase.h"

NS_BEGIN(Client)

class Mon_PotatoBag_Body;
class Mon_PotatoBag_Weapon;
class Mon_PotatoBag_Shield;
class Mon_WeakPoint;

class MW_PotatoBag final : public Monster_WorldBase
{
    GENERATED_GAMEOBJECT(MW_PotatoBag, ObjectType::Monster)

public:
    MW_PotatoBag(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MW_PotatoBag(const MW_PotatoBag& prototype);
    virtual ~MW_PotatoBag() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Priority_Update(float timeDelta) override;
    void Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;

public:
    void OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;

protected:
    HRESULT Ready_Components() override;
    HRESULT Ready_PartObjects();

private:
    Shared<Mon_PotatoBag_Body> _body = {};
    Shared<Mon_PotatoBag_Weapon> _weapon = {};
    Shared<Mon_PotatoBag_Shield> _shield = {};
    Shared<Mon_WeakPoint> _weak = {};

public:
    static Shared<MW_PotatoBag> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
