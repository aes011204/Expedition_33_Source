#pragma once
#include "Camera.h"
class TargetCamera final : public Camera
{
public:
    typedef struct tagTargetCamera final : public Camera::CAMERA_DESC
    {
    }TARGETCAMERA_DESC;

public:
    TargetCamera(const ComPtr<Device>& device, const ComPtr<Context>& context);
    TargetCamera(const TargetCamera& prototype);
    virtual ~TargetCamera() { Free(); }

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    virtual void    Priority_Update(float timeDelta) override;
    virtual void    Update(float timeDelta) override;
    virtual void    Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;


private:
   Weak<Transform> _targetTransform;

public:
    static Shared<TargetCamera> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* arg) override;
    virtual void Free() override;

};

