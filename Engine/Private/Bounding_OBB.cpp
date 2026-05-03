#include "Bounding_OBB.h"
#include "Bounding_AABB.h"
#include "Bounding_Sphere.h"
#include "DebugDraw.h"

Bounding_OBB::Bounding_OBB(ComPtr<Device> device, ComPtr<Context> context)
    : Bounding(device, context)
{
}

HRESULT Bounding_OBB::Initialize(const BoundingDesc& desc)
{
    CHECK_FAILED(Bounding::Initialize(desc), E_FAIL);

    const BoundingOBBDesc& obbDesc = static_cast<const BoundingOBBDesc&>(desc);

    Quat rotation = Quat::CreateFromYawPitchRoll(
        obbDesc.radians.y,
        obbDesc.radians.x,
        obbDesc.radians.z);

    _originalOBB = BoundingOrientedBox(obbDesc.center, obbDesc.extents, rotation);
    _obb = _originalOBB;

    return S_OK;
}

void Bounding_OBB::Update(const Matrix& worldMatrix)
{
    _originalOBB.Transform(_obb, worldMatrix);
}

bool Bounding_OBB::Intersect(Bounding* otherBounding)
{
    CollisionShape otherShape = otherBounding->Get_Shape();

    switch (otherShape)
    {
    case CollisionShape::AABB:
        return _obb.Intersects(static_cast<Bounding_AABB*>(otherBounding)->Get_AABB());
    case CollisionShape::OBB:
        return _obb.Intersects(static_cast<Bounding_OBB*>(otherBounding)->Get_OBB());
    case CollisionShape::Sphere:
        return _obb.Intersects(static_cast<Bounding_Sphere*>(otherBounding)->Get_Sphere());
    //case CollisionShape::Capsule:
    //    return _obb.Intersects(static_cast<Bounding_Capsule*>(otherBounding)->Get_ProxyOBB());
    }

    return false;
}

HRESULT Bounding_OBB::Render_Debug(DirectX::PrimitiveBatch<DirectX::VertexPositionColor>* batch, Color color)
{
    DX::Draw(batch, _obb, color);

    return S_OK;
}

Shared<Bounding_OBB> Bounding_OBB::Create(ComPtr<Device> device, ComPtr<Context> context, const BoundingOBBDesc& desc)
{
    auto instance = make_shared<Bounding_OBB>(device, context);

    if (FAILED(instance->Initialize(desc)))
    {
        MSG_BOX("Failed to Created : Bounding_OBB");
        return nullptr;
    }

    return instance;
}

void Bounding_OBB::Free()
{
    Bounding::Free();
}
