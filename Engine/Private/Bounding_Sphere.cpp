#include "pch.h"
#include "Bounding_Sphere.h"

#include "Bounding_AABB.h"
#include "Bounding_OBB.h"
#include "DebugDraw.h"

Bounding_Sphere::Bounding_Sphere(ComPtr<Device> device, ComPtr<Context> context)
    : Bounding(device, context)
{
}

HRESULT Bounding_Sphere::Initialize(const BoundingDesc& desc)
{
    CHECK_FAILED(Bounding::Initialize(desc), E_FAIL);

    const FBoundingSphereDesc& sphereDesc = static_cast<const FBoundingSphereDesc&>(desc);

    _originSphere = BoundingSphere(sphereDesc.center, sphereDesc.radius);
    _sphere = _originSphere;

    return S_OK;
}

void Bounding_Sphere::Update(const Matrix& worldMatrix)
{
    _originSphere.Transform(_sphere, worldMatrix);
}

bool Bounding_Sphere::Intersect(Bounding* otherBounding)
{
    CollisionShape otherShape = otherBounding->Get_Shape();

    switch (otherShape)
    {
    case CollisionShape::AABB:
        return _sphere.Intersects(static_cast<Bounding_AABB*>(otherBounding)->Get_AABB());
    case CollisionShape::OBB:
        return _sphere.Intersects(static_cast<Bounding_OBB*>(otherBounding)->Get_OBB());
    case CollisionShape::Sphere:
        return _sphere.Intersects(static_cast<Bounding_Sphere*>(otherBounding)->Get_Sphere());
    //case CollisionShape::Capsule:
    //    return _sphere.Intersects(static_cast<Bounding_Capsule*>(otherBounding)->Get_ProxyOBB());
    }

    return false;
}

HRESULT Bounding_Sphere::Render_Debug(DirectX::PrimitiveBatch<DirectX::VertexPositionColor>* batch, Color color)
{
    DX::Draw(batch, _sphere, color);
    return S_OK;
}

Shared<Bounding_Sphere> Bounding_Sphere::Create(ComPtr<Device> device, ComPtr<Context> context, const FBoundingSphereDesc& desc)
{
    auto instance = make_shared<Bounding_Sphere>(device, context);

    if (FAILED(instance->Initialize(desc)))
    {
        MSG_BOX("Failed to Create : Bounding_Sphere");
        return nullptr;
    }

    return instance;
}

void Bounding_Sphere::Free()
{
    Bounding::Free();
}
