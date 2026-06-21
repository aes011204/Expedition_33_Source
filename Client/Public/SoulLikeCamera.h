#pragma once
#include "Camera.h"
#include "CameraPresetTypes.h"

NS_BEGIN(Client)

class SoulLikeCamera final : public Camera
{
    GENERATED_GAMEOBJECT(SoulLikeCamera, ObjectType::SoulLikeCamera)

public:
    struct SOULLIKE_CAMERA_DESC final : public Camera::CAMERA_DESC
    {
        float followSpeed = 8.f;
        float targetFollowSpeed = 4.f;
        float verticalFollowSpeed = 10.f;
        float distance = 2.51f;

        float sideOffset = 0.f;
        float heightOffset = 0.65f;
        float aimOffset = 0.5f;

        float pitchLookScale = 0.5f;
        float pitchMin = -60.f;
        float pitchMax = 60.f;

        float yawOffset = 0.f;
        float pitchOffset = 0.f;
        float rollOffset = 0.f;

        float lookHeight = 1.13f;
        float pivotHeight = 0.4f;

        bool allowMouseRotation = true;

        bool doCollisionTest = true;
        float collisionProbeRadius = 0.2f;
        float collisionSkin = 0.03f;
    };

public:
    SoulLikeCamera(const ComPtr<Device>& device, const ComPtr<Context>& context);
    SoulLikeCamera(const SoulLikeCamera& prototype);
    ~SoulLikeCamera() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;
    void    OnActivated() override;
    void    Priority_Update(float timeDelta) override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;
    HRESULT Render() override;

public:
    void SetTargetTransform(Shared<TransformCom> targetTransform);
    bool Try_GetFlatMoveBasis(Vec3& outForward, Vec3& outRight) const override;

private:
    void Build_DefaultPreset(const SOULLIKE_CAMERA_DESC& desc);
    void Interpolate_CurrentSettings(float timeDelta);
    bool Try_GetTargetTransform(Shared<TransformCom>& outTargetTransformCom) const;
    void Update_SmoothedTargetPosition(const Vec3& targetPos, float timeDelta);
    void Update_FromCurrentSettings(float timeDelta);
    Vec3 Resolve_Collision(const Vec3& sweepStart, const Vec3& desiredPos, float timeDelta);

private:
    TargetCameraPreset _currentSettings{};
    TargetCameraPreset _targetSettings{};

    Weak<TransformCom> _targetTransformCom{};
    Vec3 _smoothedTargetPos = Vec3::Zero;
    Vec3 _prevTargetPos = Vec3::Zero;

    float _collisionDistance = -1.f;
    float _collisionPullSpeed = 16.f;
    float _collisionReturnSpeed = 3.5f;
    float _collisionDeadZone = 0.04f;

    float _collisionQueryInterval = 0.05f;
    float _collisionQueryAccum = 0.f;

    bool _hasCollisionQueryCache = false;
    Vec3 _lastCollisionSweepStart = Vec3::Zero;
    Vec3 _lastCollisionDesiredPos = Vec3::Zero;
    Vec3 _lastCollisionResolvedPos = Vec3::Zero;

public:
    static Shared<SoulLikeCamera> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
