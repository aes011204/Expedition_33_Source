#pragma once

#include "Monster_BattleBase.h"

NS_BEGIN(Client)

class Mon_Portier_Body;

class MB_Portier final : public Monster_BattleBase
{
    GENERATED_GAMEOBJECT(MB_Portier, ObjectType::Monster)

public:
public:
    MB_Portier(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MB_Portier(const MB_Portier& prototype);
    ~MB_Portier() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;

protected:
    HRESULT Ready_PartObjects();

private:
    Shared<Mon_Portier_Body> _body = {};

public:
    static Shared<MB_Portier> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
