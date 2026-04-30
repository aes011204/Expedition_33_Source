#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)

class ENGINE_DLL Camera abstract : public GameObject
{
public:
	typedef struct tagCameraDesc : public GameObject::GAMEOBJECT_DESC
	{
		XMFLOAT4		eye, at;
		float		    fovy, nearPlane, farPlane;
	}CAMERA_DESC;

public:
	Camera(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Camera(const Camera& prototype);
	virtual ~Camera() = default;

public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* arg);
	virtual void Priority_Update(float timeDelta);
	virtual void Update(float timeDelta);
	virtual void Late_Update(float timeDelta);
	virtual HRESULT Render();

protected:
    float	_fovy{}, _near{}, _far{}, _aspect{};

protected:
	void Update_TransformMatrices();

public:
	virtual Shared<GameObject> Clone(void* arg) = 0;
	virtual void Free() override;
};

NS_END

