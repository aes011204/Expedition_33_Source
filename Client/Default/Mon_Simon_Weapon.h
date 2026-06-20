#pragma once

#include "Client_Defines.h"
#include "Monster_Part.h"

NS_BEGIN(Client)

class Mon_Simon_Weapon final : public Monster_Part
{
    GENERATED_GAMEOBJECT(Mon_Simon_Weapon, ObjectType::Weapon)

public:
    struct  WEAPON_DESC : Monster_Part::MONSTER_PART_DESC
    {
        const  Matrix* socketMatrix = nullptr;
    };

public:
    Mon_Simon_Weapon(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Mon_Simon_Weapon(const Mon_Simon_Weapon& prototype);
    virtual ~Mon_Simon_Weapon() override = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    virtual void Priority_Update(float timeDelta) override;
    virtual void Update(float timeDelta) override;
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;
    virtual HRESULT Render_Shadow() override;
    void Set_UseSocketBinding(bool useSocketBinding) { _useSocketBinding = useSocketBinding; }
    void Detach_PreserveWorld();
    void Attach_ToBindings();

private:
    virtual uint32 Decide_PassIndex(Shared<Material> mat) override;

private:
    const Matrix* _socketMatrix = nullptr;
    Matrix _offsetMatrix = {};
    Matrix _detachedWorldMatrix = Matrix::Identity;
    bool _useSocketBinding = true;
    bool _isDetached = false;

public:
    static Shared<Mon_Simon_Weapon> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
