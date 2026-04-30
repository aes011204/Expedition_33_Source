#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)

class ENGINE_DLL UIObject abstract : public GameObject
{
public:
	typedef struct tagUIObjectDesc  : public GameObject::GAMEOBJECT_DESC
	{
		float fX, fY, fSizeX, fSizeY;
	}UI_DESC;

public:
	UIObject(ComPtr<Device> device, ComPtr<Context> context);
	UIObject(const UIObject& Prototype);
	virtual ~UIObject() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Priority_Update(float fTimeDelta) override;
	virtual void Update(float fTimeDelta) override;
	virtual void Late_Update(float fTimeDelta) override;
	virtual HRESULT Render() override;

protected:
	float _positionX = {}, _positionY = {}, _sizeX = {}, _sizeY = {};
	float _viewPortWidth = {}, _viewPortHeight = {};
	XMFLOAT4X4 _transformMatrices[ETOI(D3DTS::END)] = {};

protected:
	void Update_Transform();
	HRESULT Bind_ShaderResource(class Shader* pShader, const char* pConstantName, D3DTS eTransformState);

public:
	virtual Shared<GameObject> Clone(void* pArg) = 0;
	virtual void Free() override;
};

NS_END

