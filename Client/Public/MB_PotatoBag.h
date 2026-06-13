#pragma once

#include "Monster_BattleBase.h"

NS_BEGIN(Client)

class Mon_WeakPoint;
class Mon_PotatoBag_Body;
class Mon_PotatoBag_Weapon;
class Mon_PotatoBag_Shield;

class MB_PotatoBag final : public Monster_BattleBase
{
    GENERATED_GAMEOBJECT(MB_PotatoBag, ObjectType::Monster)

public:
    MB_PotatoBag(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MB_PotatoBag(const MB_PotatoBag& prototype);
    ~MB_PotatoBag() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;

public:
    Shared<Mon_WeakPoint> Get_BattleWeakPoint() const override { return _weak; }

protected:
    bool Try_GetDamageFeedbackSocketWorldPosition(Vec3& outWorldPos) const override;
    HRESULT Ready_PartObjects();

private:
    Shared<Mon_PotatoBag_Body> _body = {};
    Shared<Mon_PotatoBag_Weapon> _weapon = {};
    Shared<Mon_PotatoBag_Shield> _shield = {};
    Shared<Mon_WeakPoint> _weak = {};

public:
    static Shared<MB_PotatoBag> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
