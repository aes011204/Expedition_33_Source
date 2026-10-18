#pragma once

#include "Bounding.h"

NS_BEGIN(Engine)

class ENGINE_DLL Bounding_Sphere final : public Bounding
{
public:
    struct FBoundingSphereDesc : public BoundingDesc
    {
        float radius = 0.5f;
    };

public:
    explicit Bounding_Sphere(ComPtr<Device> device, ComPtr<Context> context);
    virtual ~Bounding_Sphere() = default;

public:
    HRESULT Initialize(const BoundingDesc& desc) override;
    void    Update(const Matrix& worldMatrix) override;
    bool    Intersect(Bounding* otherBounding) override;

    CollisionShape          Get_Shape() const override { return CollisionShape::Sphere; }
    BoundingSphere&         Get_OriginSphere() { return _originSphere; }
    const BoundingSphere&   Get_Sphere() const { return _sphere; }

    HRESULT Render_Debug(DirectX::PrimitiveBatch<DirectX::VertexPositionColor>* batch, Color color) override;

private:
    BoundingSphere _originSphere;
    BoundingSphere _sphere;

public:
    static Shared<Bounding_Sphere> Create(
        ComPtr<Device> device,
        ComPtr<Context> context,
        const FBoundingSphereDesc& desc);

    virtual void Free() override;
};

NS_END
