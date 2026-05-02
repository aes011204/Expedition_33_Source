#pragma once

#include "Client_Defines.h"
#include "Camera.h"

NS_BEGIN(Client)

class FreeCamera final : public Camera
{
public:
	typedef struct tagFreeCamera final : public Camera::CAMERA_DESC
	{
	}FREECAMERA_DESC;

public:
	FreeCamera(const ComPtr<Device>& device, const ComPtr<Context>& context);
	FreeCamera(const FreeCamera& prototype);
	virtual ~FreeCamera() { Free(); }

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* arg) override;
	virtual void    Priority_Update(float timeDelta) override;
	virtual void    Update(float timeDelta) override;
	virtual void    Late_Update(float timeDelta) override;
	virtual HRESULT Render() override;

private:
    bool    _firstUpdate = true;

public:
	static Shared<FreeCamera> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;

};

NS_END
