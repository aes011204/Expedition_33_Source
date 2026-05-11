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
        float           mouseSensor;
        float           mouseSensitivity;
	}CAMERA_DESC;

public:
	Camera(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Camera(const Camera& prototype);
	virtual ~Camera() = default;

public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* arg);
	virtual void    Priority_Update(float timeDelta);
	virtual void    Update(float timeDelta);
	virtual void    Late_Update(float timeDelta);
	virtual HRESULT Render();

public:
    float Get_FovY() { return _fovY; }
    void  Set_FovY(float fovY) { _fovY = fovY;}

    float Get_Near() { return _near; }
    void  Set_Near(float fNear) { _near = fNear;};

    float Get_Far() { return _far; }
    void  Set_Far(float fFar) {_far = fFar;}

    float Get_Yaw() { return _yaw; }
    void  Set_Yaw(float yaw) { _yaw = yaw; }

    float Get_Pitch() {return _pitch; }
    void  Set_Pitch(float pitch) { _pitch = pitch; }

    float Get_MouseSensor() {return _mouseSensor; }
    void  Set_MouseSensor(float mouseSensor) { _mouseSensor = mouseSensor; }

public:
    virtual bool Try_GetFlatMoveBasis(Vec3& outForward, Vec3& outRight) const;

protected:
    float	_fovY{}, _near{}, _far{}, _aspect{};
    float	_mouseSensor = {};
    float   _mouseSensitivity = {};
    float   _yaw {}, _pitch{};

protected:
	void Update_TransformMatrices();

public:
	virtual Shared<GameObject> Clone(void* arg) = 0;
	virtual void Free() override;
};

NS_END

