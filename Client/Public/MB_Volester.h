#pragma once

#include "Monster_BattleBase.h"

NS_BEGIN(Client)

class Mon_Volester_Body;


class MB_Volester :
    public Monster_BattleBase
{
    GENERATED_GAMEOBJECT(MB_Volester, ObjectType::Monster)

public:
    MB_Volester(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MB_Volester(const MB_Volester& prototype);
    ~MB_Volester() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;

protected:
    HRESULT Ready_PartObjects();

private:
    Shared<Mon_Volester_Body> _body = {};

public:
    static Shared<MB_Volester> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
