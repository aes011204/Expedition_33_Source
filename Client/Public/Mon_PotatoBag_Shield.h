#pragma once

#include "Client_Defines.h"
#include "Monster_Part.h"

NS_BEGIN(Engine)
class ColliderCom;
NS_END

NS_BEGIN(Client)

class Mon_PotatoBag_Shield final : public Monster_Part
{
    GENERATED_GAMEOBJECT(Mon_PotatoBag_Shield, ObjectType::Weapon)

public:
    struct SHIELD_DESC : Monster_Part::MONSTER_PART_DESC
    {
        const Matrix* socketMatrix = nullptr;
    };

public:
    Mon_PotatoBag_Shield(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Mon_PotatoBag_Shield(const Mon_PotatoBag_Shield& prototype);
    virtual ~Mon_PotatoBag_Shield() override = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    virtual void Priority_Update(float timeDelta) override;
    virtual void Update(float timeDelta) override;
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;
    virtual HRESULT Render_Shadow() override;
    HRESULT Ready_Components(const MONSTER_PART_DESC& desc) override;

private:
    virtual uint32 Decide_PassIndex(Shared<Material> mat) override;

private:
    const Matrix* _socketMatrix = nullptr;
    Matrix _offsetMatrix = {};
    Shared<ColliderCom> _shieldColliderCom{};

public:
    static Shared<Mon_PotatoBag_Shield> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
