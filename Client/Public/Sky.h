#pragma once

#include "Client_Defines.h"
#include "GameObject.h"

NS_BEGIN(Engine)
class Shader;
class Texture;
class VIBuffer_Cube;
NS_END

NS_BEGIN(Client)

class Sky final : public GameObject
{
public:
	typedef struct tagTerrainDesc final : public GameObject::GAMEOBJECT_DESC
	{
		
	}TERRAIN_DESC;
public:
	Sky(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Sky(const Sky& prototype);
	virtual ~Sky() { Free(); }

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Priority_Update(float fTimeDelta) override;
	virtual void Update(float fTimeDelta) override;
	virtual void Late_Update(float fTimeDelta) override;
	virtual HRESULT Render() override;

protected:
	Shared<Shader> _shader = { nullptr };
	Shared<VIBuffer_Cube> _vibuffer = { nullptr };
	Shared<Texture> _texture = { nullptr };

protected:
	HRESULT Ready_Components();
	HRESULT Bind_ShaderResources();


public:
	static Shared<Sky> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* pArg) override;
	virtual void Free() override;
};

NS_END

