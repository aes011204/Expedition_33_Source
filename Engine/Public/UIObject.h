#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)

class ENGINE_DLL UIObject abstract : public GameObject
{
public:
	typedef struct tagUIObjectDesc  : public GameObject::GAMEOBJECT_DESC
	{
		float x, y, sizeX, sizeY;
	}UI_DESC;

public:
	UIObject(const ComPtr<Device>& device, const ComPtr<Context>& context);
	UIObject(const UIObject& prototype);
	virtual ~UIObject() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* arg) override;
	virtual void Priority_Update(float timeDelta) override;
	virtual void Update(float timeDelta) override;
	virtual void Late_Update(float timeDelta) override;
	virtual HRESULT Render() override;

protected:
	float _positionX = {}, _positionY = {}, _sizeX = {}, _sizeY = {};
	float _viewPortWidth = {}, _viewPortHeight = {};
	XMFLOAT4X4 _transformMatrices[ETOI(D3DTS::END)] = {};

protected:
	void Update_Transform();
	HRESULT Bind_ShaderResource(class Shader* shader, const char* constantName, D3DTS transformState);

public:
	virtual Shared<GameObject> Clone(void* arg) = 0;
	virtual void Free() override;
};

NS_END

