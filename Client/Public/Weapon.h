#pragma once

#include "Client_Defines.h"
#include "PartObject.h"

NS_BEGIN(Engine)
class Shader;
class Model;
NS_END

NS_BEGIN(Client)

class Weapon final : public PartObject
{
public:
	typedef struct tagWeaponDesc final : public PartObject::PARTOBJECT_DESC
	{
		const uint32* pParentState = { nullptr };
		const XMFLOAT4X4* pSocketMatrix = { nullptr };
	}WEAPON_DESC;
private:
	Weapon(ComPtr<Device> device, ComPtr<Context> context);
	Weapon(const Weapon& Prototype);
	virtual ~Weapon() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Priority_Update(float fTimeDelta) override;
	virtual void Update(float fTimeDelta) override;
	virtual void Late_Update(float fTimeDelta) override;
	virtual HRESULT Render() override;


private:
	Shader* _shader = { nullptr };
	Model* _model = { nullptr };

private:
	const uint32* _parentState = { nullptr };
	const XMFLOAT4X4* _socketMatrix = { nullptr };



private:
	HRESULT Ready_Components();
	HRESULT Bind_ShaderResources();

public:
	static Shared<Weapon> Create(ComPtr<Device> device, ComPtr<Context> context);
	virtual Shared<GameObject> Clone(void* pArg) override;
	virtual void Free() override;

};

NS_END
