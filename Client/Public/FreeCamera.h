#pragma once

#include "Client_Defines.h"
#include "Camera.h"

NS_BEGIN(Client)

class FreeCamera final : public Camera
{
public:
	typedef struct tagFreeCamera final : public Camera::CAMERA_DESC
	{
		float	mouseSensor;
	}FREECAMERA_DESC;

public:
	FreeCamera(const ComPtr<Device>& device, const ComPtr<Context>& context);
	FreeCamera(const FreeCamera& prototype);
	virtual ~FreeCamera() { Free(); }

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void    Priority_Update(float fTimeDelta) override;
	virtual void    Update(float fTimeDelta) override;
	virtual void    Late_Update(float fTimeDelta) override;
	virtual HRESULT Render() override;

private:
	float				_mouseSensor = {};

public:
	static Shared<FreeCamera> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;

};

NS_END
