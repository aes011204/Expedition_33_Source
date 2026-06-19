#pragma once

#include "Monster_WorldBase.h"

NS_BEGIN(Client)

class Mon_Dualliste_Body;
class Mon_Dualliste_Sword_L;
class Mon_Dualliste_Sword_R;

class MW_Dualliste final : public Monster_WorldBase
{
    GENERATED_GAMEOBJECT(MW_Dualliste, ObjectType::Monster)

public:
    MW_Dualliste(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MW_Dualliste(const MW_Dualliste& prototype);
    virtual ~MW_Dualliste() override = default;

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
    Shared<Mon_Dualliste_Body> _body = {};
    Shared<Mon_Dualliste_Sword_L> _swordL = {};
    Shared<Mon_Dualliste_Sword_R> _swordR = {};

public:
    static Shared<MW_Dualliste> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
