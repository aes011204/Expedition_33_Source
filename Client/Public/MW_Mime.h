#pragma once

#include "Monster_WorldBase.h"

NS_BEGIN(Client)

class Mon_Mime_Body;
class Mon_Mime_Bagutte;
class Mon_WeakPoint;

class MW_Mime final : public Monster_WorldBase
{
    GENERATED_GAMEOBJECT(MW_Mime, ObjectType::Monster)

public:
    MW_Mime(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MW_Mime(const MW_Mime& prototype);
    virtual ~MW_Mime() override = default;

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
    Shared<Mon_Mime_Body> _body = {};
    Shared<Mon_Mime_Bagutte> _bagutte = {};

public:
    static Shared<MW_Mime> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
