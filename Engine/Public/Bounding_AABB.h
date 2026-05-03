#pragma once

#include "Bounding.h"

NS_BEGIN(Engine)

class Bounding_AABB : public Bounding
{
public:
    struct BoundingAABBDesc : public BoundingDesc
    {
        Vec3 extents = Vec3(0.5f);
    };

public:
    Bounding_AABB(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual ~Bounding_AABB() = default;

public:
    HRESULT Initialize(const BoundingDesc& desc) override;
    void    Update(const Matrix& worldMatrix) override;
    bool    Intersect(Bounding* otherBounding) override;

    CollisionShape      Get_Shape() const override { return CollisionShape::AABB; }
    BoundingBox&        Get_OriginAABB() { return _originAABB;}
    const BoundingBox&  Get_AABB() const { return _aabb; }

#ifdef _DEBUG
    HRESULT Render_Debug(DirectX::PrimitiveBatch<DirectX::VertexPositionColor>* batch, Color color) override;
#endif


private:
    BoundingBox _originAABB;
    BoundingBox _aabb;

public:
    static Shared<Bounding_AABB> Create(
        ComPtr<Device> device,
        ComPtr<Context> context,
        const BoundingAABBDesc& desc);

    virtual void Free() override;

};

NS_END
