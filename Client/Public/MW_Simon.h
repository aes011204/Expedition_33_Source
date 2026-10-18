#pragma once
#include "Monster_WorldBase.h"
NS_BEGIN(Client)

class Mon_Simon_Body;
class Mon_Simon_Hair;
class Mon_Simon_Weapon;

class MW_Simon :
    public Monster_WorldBase
{
    GENERATED_GAMEOBJECT(MW_Simon, ObjectType::Monster)

public:
    MW_Simon(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MW_Simon(const MW_Simon& prototype);
    virtual ~MW_Simon() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Priority_Update(float timeDelta) override;
    void    Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;

public:
    void    OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    bool    Should_ApplySerializedPart(const wstring& partTag, const json& partObjectJson) const override;

protected:
    HRESULT Ready_Components() override;
    HRESULT Ready_PartObjects();

private:
    Shared<Mon_Simon_Body>      _body = {};
    Shared<Mon_Simon_Hair>      _hair = {};
    Shared<Mon_Simon_Weapon>      _weapon = {};



public:
    static Shared<MW_Simon> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
