#pragma once

#include "Component.h"

NS_BEGIN(Engine)

class ShaderCom;

class ENGINE_DLL TransformCom final : public Component
{
    GENERATED_COMPONENT(TransformCom)

public:
    using TRANSFORM_DESC = struct tagTransformDesc
    {
        float speedPerSec{ 0.f };
        float degreePerSec{ 0.f };
        Vec3 position{ Vec3::Zero };
        Vec3 scale{ Vec3::One };
        float pitch{ 0.f };
        float yaw{ 0.f };
        float roll{ 0.f };
    };

private: //## Types::DirtyTracking
    enum class TRANSFORM_DIRTY_FLAG : uint8
    {
        NONE = 0,
        ROTATION = 1 << 0,
        WORLD = 1 << 1
    };

    enum class LOCAL_CHANGE_KIND : uint8
    {
        ROTATION_UNCHANGED,
        ROTATION_CHANGED
    };

public:
    TransformCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    TransformCom(const TransformCom& prototype);
    ~TransformCom() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update_WorldMatrix();
    HRESULT Bind_ShaderResource(ShaderCom* shader, const char* resourceName);

    json To_Json() const override;
    void From_Json(const json& data) override;

public: //## Accessors
    const Matrix& Get_WorldMatrix();
    Matrix Get_LocalMatrix() const;

    // World 기준 축 또는 위치를 조회한다.
    Vec3 Get_State(State state);

    //## Translation
    // Local 위치를 조회한다. parent가 있으면 World 위치와 다를 수 있다.
    Vec3 Get_Position() const;
    // Local 위치를 절대 설정한다.
    void Set_Position(const Vec3& position);
    // Local 위치에 local delta를 더한다.
    void Add_Position(const Vec3& deltaPosition);
    // parent를 반영한 최종 World 위치를 조회한다.
    Vec3 Get_WorldPosition();
    // 최종 World 위치를 절대 설정한다. parent가 있으면 local 위치로 역변환한다.
    void Set_WorldPosition(const Vec3& position);
    // 최종 World 위치에 world delta를 더한다. parent가 있으면 local 위치로 역변환한다.
    void Add_WorldPosition(const Vec3& deltaPosition);

    //## Scale
    // Local signed scale을 조회한다. parent scale이 합성된 World scale은 아니다.
    Vec3 Get_Scale() const;
    // Local signed scale을 절대 설정한다.
    void Set_Scale(const Vec3& scale);
    // Local X scale을 절대 설정한다.
    void Set_ScaleX(float scaleX);
    // Local Y scale을 절대 설정한다.
    void Set_ScaleY(float scaleY);
    // Local Z scale을 절대 설정한다.
    void Set_ScaleZ(float scaleZ);

    //## Rotation
    // Local Euler 회전을 degree 단위로 조회한다.
    Vec3 Get_RotationEuler();
    // Local Euler 회전을 degree 단위로 절대 설정한다.
    void Set_RotationEuler(const Vec3& eulerDegree);
    // Local Euler 회전에 degree delta를 누적한다.
    void Add_LocalRotationEuler(const Vec3& deltaEulerDegree);
    // parent를 반영한 World Euler 회전을 degree 단위로 조회한다.
    Vec3 Get_WorldRotationEuler();
    // 최종 World Euler 회전을 degree 단위로 절대 설정한다. parent가 있으면 local 회전으로 역변환한다.
    void Set_WorldRotationEuler(const Vec3& eulerDegree);
    // 최종 World Euler 회전에 degree delta를 누적한다. parent가 있으면 local 회전으로 역변환한다.
    void Add_WorldRotationEuler(const Vec3& deltaEulerDegree);

    // Local 회전 quaternion을 조회한다.
    const Quat& Get_RotationQuaternion();
    // Local 회전 quaternion을 절대 설정한다.
    void Set_RotationQuaternion(const Quat& rotation);
    // Local 회전 quaternion에 delta를 누적한다.
    void Add_LocalRotationQuaternion(const Quat& deltaRotation);
    // parent를 반영한 World 회전 quaternion을 조회한다.
    Quat Get_WorldRotationQuaternion();
    // 최종 World 회전 quaternion을 절대 설정한다. parent가 있으면 local 회전으로 역변환한다.
    void Set_WorldRotationQuaternion(const Quat& rotation);
    // 최종 World 회전 quaternion에 delta를 누적한다. parent가 있으면 local 회전으로 역변환한다.
    void Add_WorldRotationQuaternion(const Quat& deltaRotation);

    //## Speed
    float Get_SpeedPerSec() const;
    void Set_SpeedPerSec(float speedPerSec);
    float Get_DegreePerSec() const;
    void Set_DegreePerSec(float degreePerSec);

public: //## Hierarchy
    void Set_Parent(const Shared<TransformCom>& parent);

public: //## Direction
    // World forward 축을 정규화해서 조회한다.
    Vec3 Get_WorldForward();
    // World right 축을 정규화해서 조회한다.
    Vec3 Get_WorldRight();
    // World up 축을 정규화해서 조회한다.
    Vec3 Get_WorldUp();
    // Local forward 축을 정규화해서 조회한다.
    Vec3 Get_LocalForward();
    // Local right 축을 정규화해서 조회한다.
    Vec3 Get_LocalRight();
    // Local up 축을 정규화해서 조회한다.
    Vec3 Get_LocalUp();

public: //## Commands::Scale Multiply
    // 현재 Local scale에 각 축 multiplier를 곱한다.
    void Scaling(float scaleX, float scaleY, float scaleZ);
    // 현재 Local X scale에 multiplier를 곱한다.
    void ScaleX(float scaleX);
    // 현재 Local Y scale에 multiplier를 곱한다.
    void ScaleY(float scaleY);
    // 현재 Local Z scale에 multiplier를 곱한다.
    void ScaleZ(float scaleZ);

public: //## Commands::Movement
    // World forward 축 방향으로 speedPerSec * timeDelta만큼 이동한다.
    void Go_Straight(float timeDelta);
    // World forward 축 반대 방향으로 speedPerSec * timeDelta만큼 이동한다.
    void Go_Backward(float timeDelta);
    // World right 축 방향으로 speedPerSec * timeDelta만큼 이동한다.
    void Go_Right(float timeDelta);
    // World right 축 반대 방향으로 speedPerSec * timeDelta만큼 이동한다.
    void Go_Left(float timeDelta);
    // World Y+ 방향으로 speedPerSec * timeDelta만큼 이동한다.
    void Go_Up(float timeDelta);
    // World Y- 방향으로 speedPerSec * timeDelta만큼 이동한다.
    void Go_Down(float timeDelta);

public: //## Commands::Rotate
    // Legacy: Local 회전을 axis-angle(degree)로 절대 설정한다. 누적 회전이 아니다.
    void Rotate(const Vec3& axis, float degree);
    // World 위치 at을 바라보도록 회전한다. parent는 보정하지만 roll은 보존하지 않는다.
    void LookAt(const Vec3& at);

private: //## Data
    Matrix _localMatrix{ Matrix::Identity };
    Vec3 _localSignedScale{ Vec3::One };

    Weak<TransformCom> _parent;
    bool _hasParent{ false };

    Quat _localRotationCache{ Quat::Identity };
    Vec3 _localEulerDegreeCache{ Vec3::Zero };
    Matrix _worldMatrix{ Matrix::Identity };
    uint64 _worldRevision{ 0 };
    uint64 _observedParentWorldRevision{ 0 };

    float _speedPerSec{ 0.f };
    float _radianPerSec{ 0.f };

    uint8 _dirtyMask = static_cast<uint8>(TRANSFORM_DIRTY_FLAG::ROTATION) |
                       static_cast<uint8>(TRANSFORM_DIRTY_FLAG::WORLD);

private: //## Internal::WorldSync
    void Ensure_WorldMatrixUpToDate();

private: //## Internal::DirtyTracking
    void Mark_LocalChanged(LOCAL_CHANGE_KIND changeKind);
    bool Has_Dirty(TRANSFORM_DIRTY_FLAG dirty) const;
    void Set_Dirty(TRANSFORM_DIRTY_FLAG dirty);
    void Clear_Dirty(TRANSFORM_DIRTY_FLAG dirty);

private: //## Internal::Scale
    static float Clamp_EditableScale(float scale);
    static Vec3 Clamp_EditableScale(const Vec3& scale);
    float Get_SignedScale(State state) const;
    void Set_SignedScale(State state, float signedScale);
    void Sync_SignedScaleFromLocalMatrix(Vec3 fallbackScale);

private: //## Internal::LocalState
    Vec3 Get_LocalState(State state) const;
    void Set_LocalState(State state, const Vec3& value);
    Vec3 Resolve_NormalizedAxis(State state) const;
    void Sanitize_LocalBasis();
    void Rebuild_LocalBasis_FromRotationCache();
    void Sync_RotationCache_FromLocalMatrix();
    static Vec3 Extract_EulerDegreeFromRotationMatrix(const Matrix& rotationMatrix);

public:
    static Shared<TransformCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
};

NS_END
