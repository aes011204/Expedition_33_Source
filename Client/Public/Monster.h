#pragma once

#include "Client_Defines.h"
#include "GameObject.h"

NS_BEGIN(Engine)
class Shader;
class Model;
NS_END

NS_BEGIN(Client)

class Monster final : public GameObject
{
public:
	Monster(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Monster(const Monster& prototype);
	virtual ~Monster() { Free(); }

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Priority_Update(float fTimeDelta) override;
	virtual void Update(float fTimeDelta) override;
	virtual void Late_Update(float fTimeDelta) override;
	virtual HRESULT Render() override;

protected:
	Shared<Shader>	_shader = { nullptr };
	Shared<Model>	_model = { nullptr };
	

protected:
	HRESULT Ready_Components();
	HRESULT Bind_ShaderResources();


public:
	static Shared<Monster> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* pArg) override;
	virtual void Free() override;
};

NS_END
