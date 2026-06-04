#pragma once

#include "Monster_BattleBase.h"

NS_BEGIN(Client)

class Mon_WeakPoint;

class Mon_Lancelier_Body;
class Mon_Lancelier_Weapon;

class MB_Lancelier final : public Monster_BattleBase
{
    GENERATED_GAMEOBJECT(MB_Lancelier, ObjectType::Monster)

public:
    MB_Lancelier(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MB_Lancelier(const MB_Lancelier& prototype);
    ~MB_Lancelier() override = default;

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
    Shared<Mon_Lancelier_Body> _body = {};
    Shared<Mon_Lancelier_Weapon> _weapon = {};
    Shared<Mon_WeakPoint> _weak = {};

public:
    static Shared<MB_Lancelier> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
