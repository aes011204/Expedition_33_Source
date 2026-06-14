#pragma once

#include "Monster_WorldBase.h"

NS_BEGIN(Client)

class Mon_Abbest_Body;
class Mon_WeakPoint;

class MW_Abbest final : public Monster_WorldBase
{
    GENERATED_GAMEOBJECT(MW_Abbest, ObjectType::Monster)

public:
    MW_Abbest(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MW_Abbest(const MW_Abbest& prototype);
    virtual ~MW_Abbest() override = default;

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
    Shared<Mon_Abbest_Body> _body = {};
    Shared<Mon_WeakPoint> _weak = {};
public:
    static Shared<MW_Abbest> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
