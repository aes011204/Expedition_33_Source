#pragma once
#include "Camera.h"

NS_BEGIN(Client)
class TargetCamera final : public Camera
{
    GENERATED_GAMEOBJECT(TargetCamera, ObjectType::TargetCamera)

public:
    typedef struct tagTargetCamera final : public Camera::CAMERA_DESC
    {
        float followSpeed;
        float distance;

        float sideOffset;
        float heightOffset;
        float aimOffset;

        float pitchLookScale;

        float pitchMin;
        float pitchMax;

        float lookHeight;

        float pivotHeight;
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
    
    float           Get_AimOffset() {return _aimOffset;}
    void            Set_AimOffset(float aimOffset) {_aimOffset = aimOffset;}

    float           Get_Distance() {return _distance;}
    void            Set_Distance(float distance) {_distance = distance;}

    float           Get_PitchLookScale() {return _pitchLookScale;}
    void            Set_PitchLookScale(float pitchLookScale) {_pitchLookScale = pitchLookScale;}

    float           Get_PitchMin() {return _pitchMin;}
    void            Set_PitchMin(float pitchMin) {_pitchMin = pitchMin;}
    
    float           Get_PitchMax() {return _pitchMax;}
    void            Set_PitchMax(float pitchMax) {_pitchMax = pitchMax;}
       
    float           Get_LookHeight() {return _lookHeight;}
    void            Set_LookHeight(float lookHeight) {_lookHeight = lookHeight;}

    float           Get_PivotHeight() {return _pivotHeight;}
    void            Set_PivotHeight(float pivotHeight) {_pivotHeight = pivotHeight;}
    
private:
   Weak<TransformCom> _targetTransformCom;
    float             _followSpeed;
    float             _distance;
    float             _sideOffset;
    float             _heightOffset;
    float             _lookHeight;
    float             _aimOffset;

    float             _pitchLookScale;
    float             _pitchMin;
    float             _pitchMax;

    float             _pivotHeight;

    Vec3             _prevTargetPos;
public:
    static Shared<TargetCamera> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* arg) override;
    virtual void Free() override;
};
NS_END
