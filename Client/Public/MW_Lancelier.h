#pragma once

#include "Monster_WorldBase.h"

NS_BEGIN(Client)

class Mon_Lancelier_Body;
class Mon_Lancelier_Weapon;

class MW_Lancelier final : public Monster_WorldBase
{
    GENERATED_GAMEOBJECT(MW_Lancelier, ObjectType::Monster)
public:
    MW_Lancelier(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MW_Lancelier(const MW_Lancelier& prototype);
    virtual ~MW_Lancelier() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Priority_Update(float timeDelta) override;
    void    Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;

public:
    void    OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;


protected:
    HRESULT Ready_Components() override;
    HRESULT Ready_PartObjects();

private:
    Shared<Mon_Lancelier_Body>      _body = {};
    Shared<Mon_Lancelier_Weapon>    _weapon = {};

public:
    static Shared<MW_Lancelier> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
