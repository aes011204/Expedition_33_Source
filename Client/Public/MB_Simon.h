#pragma once
#include "Monster_BattleBase.h"

NS_BEGIN(Client)

class Mon_Simon_Body;
class Mon_Simon_Weapon;

class MB_Simon :
    public Monster_BattleBase
{
    GENERATED_GAMEOBJECT(MB_Simon, ObjectType::Monster)

public:
public:
    MB_Simon(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MB_Simon(const MB_Simon& prototype);
    ~MB_Simon() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;

protected:
    HRESULT Ready_PartObjects();

private:
    Shared<Mon_Simon_Body> _body = {};
    Shared<Mon_Simon_Weapon> _weapon = {};

public:
    static Shared<MB_Simon> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
