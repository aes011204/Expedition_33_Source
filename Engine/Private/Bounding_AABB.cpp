#include "Bounding_AABB.h"

#include "Bounding_OBB.h"
#include "Bounding_Sphere.h"

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



}

bool Bounding_AABB::Intersect(Bounding* otherBounding)
{
}

Shared<Bounding_AABB> Bounding_AABB::Create(ComPtr<Device> device, ComPtr<Context> context,
    const BoundingAABBDesc& desc)
{
}

void Bounding_AABB::Free()
{
    Bounding::Free();
}
