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

#ifdef _DEBUG
public:
    HRESULT Render_Debug();

private:
    HRESULT Ready_DebugRenderResources();
#endif

private:
    CollisionShape _shape = CollisionShape::AABB;

    Collision_Channel _channel = Collision_Channel::CHANNEL_NONE;
    uint32 _overlapMask = 0;
    uint32 _blockMask = 0;

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

public:
    static Shared<ColliderCom> Create(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        CollisionShape shape);

    Shared<Component> Clone(void* arg) override;
    void Free() override;
};

NS_END
