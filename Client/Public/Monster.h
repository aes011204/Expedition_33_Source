#pragma once

#include "Client_Defines.h"
#include "Character.h"

NS_BEGIN(Engine)
class Shader;
class Model;
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

protected:
	Shared<Shader>	_shader = { nullptr };
	Shared<Model>	_model = { nullptr };
	

protected:
	HRESULT Ready_Components();
	HRESULT Bind_ShaderResources();


public:
	static Shared<Monster> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;
};

NS_END
