#pragma once

#include "Client_Defines.h"
#include "Character.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
class AIController;
NS_END

NS_BEGIN(Client)

class Monster final : public Character
{
    GENERATED_GAMEOBJECT(Monster, ObjectType::Monster)

public:
	Monster(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Monster(const Monster& prototype);
	virtual ~Monster() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* arg) override;
	virtual void Priority_Update(float timeDelta) override;
	virtual void Update(float timeDelta) override;
	virtual void Late_Update(float timeDelta) override;
	virtual HRESULT Render() override;
    virtual HRESULT Render_Shadow() override;

protected:
	Shared<ShaderCom>	    _shader = { nullptr };
    Shared<ModelCom>	    _model = { nullptr };
    Shared<AIController> _aiCtrl = { nullptr };

    Shared<ColliderCom>     _collider;

protected:
	HRESULT Ready_Components();
	HRESULT Bind_ShaderResources();

    HRESULT Ready_Collider();


public:
	static Shared<Monster> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;
};

NS_END
