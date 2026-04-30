#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)

class ENGINE_DLL Camera abstract : public GameObject
{
public:
	typedef struct tagCameraDesc : public GameObject::GAMEOBJECT_DESC
	{
		XMFLOAT4		vEye, vAt;
		float		    fFovy, fNear, fFar;
	}CAMERA_DESC;

protected:
	Camera(ComPtr<Device> device, ComPtr<Context> context);
	Camera(const Camera& Prototype);
	virtual ~Camera() = default;

public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* pArg);
	virtual void Priority_Update(float fTimeDelta);
	virtual void Update(float fTimeDelta);
	virtual void Late_Update(float fTimeDelta);
	virtual HRESULT Render();

protected:
    float	_fovy{}, _near{}, _far{}, _aspect{};

protected:
	void Update_TransformMatrices();

public:
	virtual Shared<GameObject> Clone(void* pArg) = 0;
	virtual void Free() override;
};

NS_END

