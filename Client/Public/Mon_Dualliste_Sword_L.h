#pragma once

#include "Client_Defines.h"
#include "Monster_Part.h"

NS_BEGIN(Client)

class Mon_Dualliste_Sword_L final : public Monster_Part
{
    GENERATED_GAMEOBJECT(Mon_Dualliste_Sword_L, ObjectType::Weapon)

public:
    struct SWORD_DESC : Monster_Part::MONSTER_PART_DESC
    {
        const Matrix* socketMatrix = nullptr;
    };

public:
    Mon_Dualliste_Sword_L(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Mon_Dualliste_Sword_L(const Mon_Dualliste_Sword_L& prototype);
    virtual ~Mon_Dualliste_Sword_L() override = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    virtual void Priority_Update(float timeDelta) override;
    virtual void Update(float timeDelta) override;
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;
    virtual HRESULT Render_Shadow() override;
    virtual void OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;

    HRESULT Ready_Components(const MONSTER_PART_DESC& desc) override;
    Shared<ColliderCom> Get_AttackCollider() const { return _attackLCom; }
private:
    virtual uint32 Decide_PassIndex(Shared<Material> mat) override;

private:
    const Matrix* _socketMatrix = nullptr;
    Matrix _offsetMatrix = {};

    Shared<ColliderCom>         _attackLCom{};


public:
    static Shared<Mon_Dualliste_Sword_L> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
