#pragma once
#include "Camera.h"
class TargetCamera final : public Camera
{
    GENERATED_GAMEOBJECT(TargetCamera, ObjectType::TargetCamera)

public:
    typedef struct tagTargetCamera final : public Camera::CAMERA_DESC
    {
        Vec3 offset;
        float followSpeed;
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

public:
    void            SetTargetTransform(Shared<TransformCom> targetTransform);

private:
   Weak<TransformCom> _targetTransformCom;
    bool           _firstUpdate = true;
    Vec3           _offset;
    float          _followSpeed;

public:
    static Shared<TargetCamera> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* arg) override;
    virtual void Free() override;

};

