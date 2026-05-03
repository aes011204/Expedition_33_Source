#include "Bounding_AABB.h"

#include "Bounding_OBB.h"
#include "Bounding_Sphere.h"
#include "DebugDraw.h"

Bounding_AABB::Bounding_AABB(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Bounding(device, context)
{
}

HRESULT Bounding_AABB::Initialize(const BoundingDesc& desc)
{
    CHECK_FAILED(Bounding::Initialize(desc), E_FAIL);

    const BoundingAABBDesc& aabbDesc = static_cast<const BoundingAABBDesc&>(desc);

    _originAABB = BoundingBox(aabbDesc.center, aabbDesc.extents);
    _aabb = _originAABB;

    return S_OK;
}

void Bounding_AABB::Update(const Matrix& worldMatrix)
{
    Matrix tempMatrix = worldMatrix;

    Vec3 scale, translation;
    Quat rotation;

    tempMatrix.Decompose(scale, rotation, translation);

    // AABB는 회전만 버리고 다시 재구성
    Matrix transform = Matrix::CreateScale(scale) * Matrix::CreateTranslation(translation);
    _originAABB.Transform(_aabb, transform);
}

bool Bounding_AABB::Intersect(Bounding* otherBounding)
{
    CollisionShape otherShape = otherBounding->Get_Shape();

    switch (otherShape)
    {
    case CollisionShape::AABB:
        return _aabb.Intersects(static_cast<Bounding_AABB*>(otherBounding)->Get_AABB());
    case CollisionShape::OBB:
        return _aabb.Intersects(static_cast<Bounding_OBB*>(otherBounding)->Get_OBB());
    case CollisionShape::Sphere:
        return _aabb.Intersects(static_cast<Bounding_Sphere*>(otherBounding)->Get_Sphere());
    //case CollisionShape::Capsule:
    //    return _aabb.Intersects(static_cast<Bounding_Capsule*>(otherBounding)->Get_ProxyOBB());
    }

    return false;
}

#ifdef _DEBUG
HRESULT Bounding_AABB::Render_Debug(DirectX::PrimitiveBatch<DirectX::VertexPositionColor>* batch, Color color)
{
    DX::Draw(batch, _aabb, color);
    return S_OK;
}
#endif

Shared<Bounding_AABB> Bounding_AABB::Create(ComPtr<Device> device, ComPtr<Context> context,
    const BoundingAABBDesc& desc)
{
    auto instance = make_shared<Bounding_AABB>(device, context);

    if (FAILED(instance->Initialize(desc)))
    {
        MSG_BOX("Failed to Created : Bounding_AABB");
        return nullptr;
    }

    return instance;
}

void Bounding_AABB::Free()
{
    Bounding::Free();
}
