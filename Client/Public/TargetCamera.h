#pragma once
#include "Camera.h"
#include "CameraPresetTypes.h"

NS_BEGIN(Client)

class TargetCamera final : public Camera
{
    GENERATED_GAMEOBJECT(TargetCamera, ObjectType::TargetCamera)

public:
    typedef struct tagTargetCamera final : public Camera::CAMERA_DESC
    {
        float        followSpeed;       // 이건 지금 카메라 세팅값 보간 속도
        float        targetFollowSpeed; // 플레이어 위치 추적 속도
        float        verticalFollowSpeed = 10.f; // 플레이어 높이 추적 속도
        float        distance;

        float        sideOffset;
        float        heightOffset;
        float        aimOffset;

        float        pitchLookScale;

        float        pitchMin;
        float        pitchMax;

        float        yawOffset = 0.f;
        float        pitchOffset = 0.f;
        float        rollOffset = 0.f;

        float        lookHeight;
        float        pivotHeight;

        bool         allowMouseRotation = true;

        /* Camera Collision Test */
        bool         doCollisionTest = true;
        float        collisionProbeRadius = 0.2f;
        float        collisionSkin = 0.03f;


    }TARGETCAMERA_DESC;

public:
    TargetCamera(const ComPtr<Device>& device, const ComPtr<Context>& context);
    TargetCamera(const TargetCamera& prototype);
    virtual ~TargetCamera() { Free(); }

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    void            BeginPlay() override;
    void            OnActivated() override;
    virtual void    Priority_Update(float timeDelta) override;
    virtual void    Update(float timeDelta) override;
    virtual void    Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;

private:
    void Interpolate_CurrentSettings(float timeDelta);
    bool Try_GetTargetTransform(Shared<TransformCom>& outTargetTransformCom) const;
    void Update_FromCurrentSettings(float timeDelta);
private:
    void Update_SmoothedTargetPosition(const Vec3& targetPos, float timeDelta);
    void Update_NormalState(float timeDelta);
    void Update_AimState(float timeDelta);
    void Update_BattleEnterState(float timeDelta);

    /* Camera Collision Test */
    Vec3 Resolve_Collision(const Vec3& sweepStart, const Vec3& desiredPos, float timeDelta);

public:
    void Change_Mode(TargetCameraMode mode);
    TargetCameraMode Get_CurrentMode() const { return _currentState; }
    void Change_Target();
    bool Try_GetFlatMoveBasis(Vec3& outForward, Vec3& outRight) const override;
    void Refresh_TargetTransform();

    Vec3 Get_FreeAimQueryOrigin() const;
    Vec3 Get_FreeAimQueryDirection() const;

public:
    //TODO imgui      desc 하나로 보여주는 걸로 바꾸기?
    void            SetTargetTransform(Shared<TransformCom> targetTransform);

    float           Get_SideOffset() { return _sideOffset; }
    void            Set_SideOffset(float sideOffset) { _sideOffset = sideOffset; }

    float           Get_HeightOffset() { return _heightOffset; }
    void            Set_HeightOffset(float heightOffset) { _heightOffset = heightOffset; }

    float           Get_FollowSpeed() { return _followSpeed; }
    void            Set_FollowSpeed(float followSpeed) { _followSpeed = followSpeed; }

    float           Get_AimOffset() { return _aimOffset; }
    void            Set_AimOffset(float aimOffset) { _aimOffset = aimOffset; }

    float           Get_Distance() { return _distance; }
    void            Set_Distance(float distance) { _distance = distance; }

    float           Get_PitchLookScale() { return _pitchLookScale; }
    void            Set_PitchLookScale(float pitchLookScale) { _pitchLookScale = pitchLookScale; }

    float           Get_PitchMin() { return _pitchMin; }
    void            Set_PitchMin(float pitchMin) { _pitchMin = pitchMin; }

    float           Get_PitchMax() { return _pitchMax; }
    void            Set_PitchMax(float pitchMax) { _pitchMax = pitchMax; }

    float           Get_LookHeight() { return _lookHeight; }
    void            Set_LookHeight(float lookHeight) { _lookHeight = lookHeight; }

    float           Get_PivotHeight() { return _pivotHeight; }
    void            Set_PivotHeight(float pivotHeight) { _pivotHeight = pivotHeight; }

    const           TargetCameraPreset& Get_Preset(TargetCameraMode mode) const;
    void            Set_Preset(TargetCameraMode mode, const TargetCameraPreset& preset);
    void            Apply_PreviewPreset(const TargetCameraPreset& preset);

private:
    TargetCameraPreset  _currentSettings;
    TargetCameraPreset  _targetSettings;

    TargetCameraPreset  _normalPreset;
    TargetCameraPreset  _aimPreset;
    TargetCameraPreset  _battleEnterPreset;
    TargetCameraPreset  _soulLikePreset;

    float               _battleEnterDuration = 3.f;
    float               _battleEnterAcc = { 0.f };

    Weak<TransformCom> _targetTransformCom;
    float              _followSpeed;
    float              _distance;
    float              _sideOffset;
    float              _heightOffset;
    float              _lookHeight;
    float              _aimOffset;

    float              _pitchLookScale;
    float              _pitchMin;
    float              _pitchMax;

    float              _pivotHeight;

    Vec3               _smoothedTargetPos;
    Vec3               _prevTargetPos;
    TargetCameraMode   _currentState;

    //float               _speed = 5.f;

    // Camera Collision Test
    float _collisionDistance = -1.f;     // 현재 보간중인 카메라 거리
    float _collisionPullSpeed = 16.f;    // 벽에 막혀서 카메라 당길 때
    float _collisionReturnSpeed = 3.5f;  // 벽이 없어져 원래 거리로 돌아갈 때
    float _collisionDeadZone = 0.04f;    // sweep 거리의 작은 흔들림 무시 범위

    float _collisionQueryInterval = 0.05f;
    float _collisionQueryAccum = 0.f; // 다음 collision sweep까지 누적 시간

    bool _hasCollisionQueryCache = false;    // 이전 sweep 결과를 재사용할 수 있는지
    Vec3 _lastCollisionSweepStart = Vec3::Zero;
    Vec3 _lastCollisionDesiredPos = Vec3::Zero;
    Vec3 _lastCollisionResolvedPos = Vec3::Zero;


    // 매 프레임 계산된 화면 중앙 look target을 저장해 두고
    // world crosshair hit query가 같은 기준을 쓰게 한다.
    Vec3               _currentLookTarget = Vec3::Zero;

public:
    static Shared<TargetCamera> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* arg) override;
    virtual void Free() override;
};
NS_END
