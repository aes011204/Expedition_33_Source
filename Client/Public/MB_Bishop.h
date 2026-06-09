#pragma once

#include "Monster_BattleBase.h"

NS_BEGIN(Client)

class Mon_Lancelier_Weapon;
class Mon_Bishop_Body;
class Mon_WeakPoint;


class MB_Bishop :
    public Monster_BattleBase
{
    GENERATED_GAMEOBJECT(MB_Bishop, ObjectType::Monster)

public:
    MB_Bishop(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MB_Bishop(const MB_Bishop& prototype);
    ~MB_Bishop() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;
    void Update(float timeDelta) override;

public:
    Shared<Mon_WeakPoint> Get_BattleWeakPoint() const override { return _weak; }

protected:
    bool Try_GetDamageFeedbackSocketWorldPosition(Vec3& outWorldPos) const override;
    HRESULT Ready_PartObjects();

private:
    Shared<Mon_Bishop_Body> _body = {};
    Shared<Mon_Lancelier_Weapon> _weaponL = {};
    Shared<Mon_Lancelier_Weapon> _weaponR = {};
    Shared<Mon_WeakPoint> _weak = {};

public:
    static Shared<MB_Bishop> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
