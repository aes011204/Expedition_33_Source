#pragma once
#include "Camera.h"

NS_BEGIN(Client)
class TargetCamera final : public Camera
{
    GENERATED_GAMEOBJECT(TargetCamera, ObjectType::TargetCamera)

public:
    typedef struct tagTargetCamera final : public Camera::CAMERA_DESC
    {
        Vec3 offset;
        float followSpeed;
        float distance;
        float sideOffset;
        float heightOffset;
    }TARGETCAMERA_DESC;

public:
    TargetCamera(const ComPtr<Device>& device, const ComPtr<Context>& context);
    TargetCamera(const TargetCamera& prototype);
    virtual ~TargetCamera() { Free(); }

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    void            BeginPlay() override;
    virtual void    Priority_Update(float timeDelta) override;
    virtual void    Update(float timeDelta) override;
    virtual void    Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;

public:
    void            SetTargetTransform(Shared<TransformCom> targetTransform);

    float           Get_SideOffset() {return _sideOffset;}   
    void            Set_SideOffset(float sideOffset) {_sideOffset = sideOffset;}

    float           Get_HeightOffset() {return _heightOffset;}   
    void            Set_HeightOffset(float heightOffset) {_heightOffset = heightOffset;}

    float           Get_FollowSpeed() {return _followSpeed;}
    void            Set_FollowSpeed(float followSpeed) {_followSpeed = followSpeed;}

private:
   Weak<TransformCom> _targetTransformCom;
    Vec3              _offset;
    float             _followSpeed;
    float             _distance;
    float             _sideOffset;
    float             _heightOffset;

public:
    static Shared<TargetCamera> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* arg) override;
    virtual void Free() override;
};
NS_END
