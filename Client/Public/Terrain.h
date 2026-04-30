#pragma once

#include "Client_Defines.h"
#include "GameObject.h"

NS_BEGIN(Engine)
class Shader;
class Texture;
class VIBuffer_Terrain;
NS_END

NS_BEGIN(Client)

class Terrain final : public GameObject
{
public:
	enum TEXTURE { Diffuse, Mask, Brush, END };
	typedef struct tagTerrainDesc final : public GameObject::GAMEOBJECT_DESC
	{
		
	}TERRAIN_DESC;
public:
	Terrain(ComPtr<Device> device, ComPtr<Context> context);
	Terrain(const Terrain& Prototype);
	virtual ~Terrain() { Free(); }

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Priority_Update(float fTimeDelta) override;
	virtual void Update(float fTimeDelta) override;
	virtual void Late_Update(float fTimeDelta) override;
	virtual HRESULT Render() override;

protected:
	Shared<Shader> _shader = { nullptr };
	Shared<VIBuffer_Terrain> _vibuffer = { nullptr };
	Shared<Texture> _textures[TEXTURE::END] = { nullptr };

protected:
	HRESULT Ready_Components();
	HRESULT Bind_ShaderResources();


public:
	static Shared<Terrain> Create(ComPtr<Device> device, ComPtr<Context> context);
	virtual Shared<GameObject> Clone(void* pArg) override;
	virtual void Free() override;
};

NS_END

