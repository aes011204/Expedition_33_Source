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

public: //## Accessors
    const Matrix& Get_WorldMatrix();
    Matrix Get_LocalMatrix() const;

    // enum 기반으로 World 축 또는 World 위치를 조회한다.
    Vec3 Get_State(State state);
    [[deprecated("회전&스케일 오염을 방지하고자 사용 금지! | Use Set_Position/Set_WorldPosition/Set_Scale/Set_RotationEuler/Set_RotationQuaternion instead.")]]
    void Set_State(State state, const Vec3& value);

    //## Translation
    // Local 정본 위치를 조회한다.
    Vec3 Get_Position() const;
    // Local 정본 위치를 설정한다.
    void Set_Position(const Vec3& position);
    // 현재 local/parent 관계를 반영한 계산된 World 위치를 조회한다. Get_State(State::Position)과 동일하다.
    Vec3 Get_WorldPosition();
    // 부모 유무와 관계없이 최종 World 위치가 position이 되도록 설정한다. 부모가 있을 경우에 사용시 local 위치로 역변환하여 저장한다.
    void Set_WorldPosition(const Vec3& position);

    //## Scale
    Vec3 Get_Scale() const;
    void Set_Scale(const Vec3& scale);
    void Set_ScaleX(float scaleX);
    void Set_ScaleY(float scaleY);
    void Set_ScaleZ(float scaleZ);

    //## Rotation
    Vec3 Get_RotationEuler();
    void Set_RotationEuler(const Vec3& eulerDegree);
    const Quat& Get_RotationQuaternion();
    void Set_RotationQuaternion(const Quat& rotation);

    //## Speed
    float Get_SpeedPerSec() const;
    void Set_SpeedPerSec(float speedPerSec);
    float Get_DegreePerSec() const;
    void Set_DegreePerSec(float degreePerSec);

public: //## Hierarchy
    void Set_Parent(const Shared<TransformCom>& parent);

public: //## Compatibility
    void Add_LocalRotation(const Quat& deltaRotation);

    Vec3 Get_WorldForward();
    Vec3 Get_WorldRight();
    Vec3 Get_WorldUp();
    Vec3 Get_LocalForward();
    Vec3 Get_LocalRight();
    Vec3 Get_LocalUp();

public: //## Commands::Scale Multiply
    void Scaling(float scaleX, float scaleY, float scaleZ);
    void ScaleX(float scaleX);
    void ScaleY(float scaleY);
    void ScaleZ(float scaleZ);

public: //## Commands::Movement
    void Go_Straight(float timeDelta);
    void Go_Backward(float timeDelta);
    void Go_Right(float timeDelta);
    void Go_Left(float timeDelta);
    void Go_Up(float timeDelta);
    void Go_Down(float timeDelta);

public: //## Commands::Rotate
    void Rotate(const Vec3& axis, float degree);
    void Turn(const Vec3& axis, float timeDelta);
    // at는 World 위치를 받는다. parent유무와 상관 없이 최종 World forward가 타겟을 향하도록 자동 보정한다
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
