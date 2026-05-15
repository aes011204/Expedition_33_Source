#pragma once

#include "Component.h"
#include "Collision_Define.h"

NS_BEGIN(Engine)

class Bounding;

class ENGINE_DLL ColliderCom : public Component
{
    GENERATED_COMPONENT(ColliderCom)

public:
    ColliderCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ColliderCom(const ColliderCom& Prototype);
    virtual ~ColliderCom() = default;

public:
    HRESULT                 Initialize_Prototype() override;
    HRESULT                 Initialize_Prototype(CollisionShape shape);
    HRESULT                 Initialize(void* arg) override;

public:
    void                    Update_Collider(const Matrix& worldMatrix);
    bool                    Intersect(Shared<ColliderCom> target);

public:
    void                    Add_Overlap(Shared<ColliderCom> other);
    void                    Remove_Overlap(Shared<ColliderCom> other);
    bool                    Is_Overlapping(Shared<ColliderCom> other) const;
    void                    Clear_Overlap();

public:
    Shared<Bounding>        Get_Bounding() const { return _bounding; }
    CollisionShape          Get_Shape() const { return _shape; }
    Collision_Channel       Get_Channel() const { return _channel; }
    uint32                  Get_OverlapMask() const { return _overlapMask; }
    uint32                  Get_BlockMask() const { return _blockMask; }
    Collision_Preset        Get_CollisionPreset() const { return _preset; }
    bool                    Get_IsActive() const { return _isActive; }
    bool                    Get_IsColl() const { return _isColl; }

public:
    void                    Set_CollisionPreset(Collision_Preset preset);
    void                    Set_IsActive(bool isActive) { _isActive = isActive; }
    void                    Set_IsColl(bool isColl) { _isColl = isColl; }
    void                    Set_ShapeDesc(const ColliderShapeDesc& shapeDesc);


#ifdef _DEBUG
public:
    HRESULT Render_Debug();

private:
    HRESULT Ready_DebugRenderResources();
#endif


public: /* PhysX */
    const ColliderShapeDesc&    Get_ShapeDesc() const { return _shapeDesc; }
    PxRigidActor*               Get_PxActor() const { return _pxActor; }
    PxShape*                    Get_PxShape() const { return _pxShape; }

    bool                        Is_PhysXRegistered() const { return _physxRegistered; }
    bool                        Is_Trigger() const  { return _isTrigger; }

    void                        Release_PhysX();

public:
    // 피직스 월드에 들어갈 실제 충돌 객체를 만들기
    HRESULT                     Build_PhysXActor(PxPhysics* physics);
    // 매 프레임 게임 Transform 값을 피직스쪽 actor 위치/회전에 복사해서 맞춰주기
    void                        Sync_ToPhysX(const Matrix& worldMatrix);
    // 현재 preset 정보를 shape의 filter data로 넣는다.
    void                        Apply_PhysX_FilterData();

    void                        Set_PhysXRegistered(bool registered) { _physxRegistered = registered; }

    PxTransform                 Build_QueryTransform(const Matrix& worldMatrix) const;
    PxGeometryHolder            Build_QueryGeometry(const Matrix& worldMatrix) const;

private:
    // DirectX Matirx 를 PhysX PxTransform으로 변환한다.
    PxTransform                 Build_PXTransform(const Matrix& worldMatrix) const;
    // DirectXCollision debug shape와 PhysX query shape의 local offset을 맞춘다.
    PxTransform                 Build_PxLocalPose(const Matrix& worldMatrix) const;
    // _shapeDesc를 PxBoxGeomtry 또는 PxSpheregemetry 로 바꾼다.
    PxGeometryHolder            Build_PxGeometry() const;
    void                        Update_PhysXGeometry(const Matrix& worldMatrix);

private:
    CollisionShape _shape = CollisionShape::AABB;

    Collision_Channel _channel = Collision_Channel::CHANNEL_NONE;
    uint32 _overlapMask = 0;
    uint32 _blockMask   = 0;

    Collision_Preset _preset = Collision_Preset::Custom;

    bool _isActive = true;
    bool _isColl = false;

    Shared<Bounding> _bounding = nullptr;
    set<Weak<ColliderCom>, owner_less<Weak<ColliderCom>>> _overlapSet;

#ifdef _DEBUG
    Shared<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> _batch = nullptr;
    Shared<DirectX::BasicEffect> _effect = nullptr;
    ComPtr<ID3D11InputLayout> _inputLayout = nullptr;
#endif

private:
    ColliderShapeDesc _shapeDesc{};

    // 이 collider가 PhysX world에서 사용되는 실제 Actor
    PxRigidActor*   _pxActor = nullptr;
    // Actor에 붙는 실제 Collision Shape 모양
    PxShape*        _pxShape = nullptr;
    // Scene에 들어갔는지 판단
    bool _physxRegistered = false;
    // overlap-only collider인지?
    bool _isTrigger = false;


public:
    static Shared<ColliderCom> Create(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        CollisionShape shape);

    Shared<Component> Clone(void* arg) override;
    void Free() override;
};

NS_END
