#pragma once

#include "Monster_BattleBase.h"

NS_BEGIN(Client)

class Mon_WeakPoint;
class Mon_Abbest_Body;

class MB_Abbest final : public Monster_BattleBase
{
    GENERATED_GAMEOBJECT(MB_Abbest, ObjectType::Monster)

public:
    MB_Abbest(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MB_Abbest(const MB_Abbest& prototype);
    ~MB_Abbest() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;
    Shared<Mon_WeakPoint> Get_BattleWeakPoint() const override { return _weak; }

protected:
    HRESULT Ready_PartObjects();

private:
    Shared<Mon_Abbest_Body> _body = {};
    Shared<Mon_WeakPoint> _weak = {};
public:
    static Shared<MB_Abbest> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
