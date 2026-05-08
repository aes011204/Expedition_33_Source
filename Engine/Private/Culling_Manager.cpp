#include "Culling_Manager.h"

Culling_Manager::Culling_Manager()
{
}

Culling_Manager::~Culling_Manager()
{
}

//void Culling_Manager::UpdateFrustum(const Matrix& view, const Matrix& proj)
//{
//    BoundingFrustum::CreateFromMatrix(_frustum, proj);
//    _frustum.Transform(_frustum, view.Invert());
//}

void Culling_Manager::UpdateFrustum(const Matrix& view, const Matrix& proj)
{
    BoundingFrustum localFrustum;
    BoundingFrustum::CreateFromMatrix(localFrustum, proj);

    XMVECTOR det;
    XMMATRIX invView = XMMatrixInverse(&det, view);
    localFrustum.Transform(_frustum, invView);
}

bool Culling_Manager::IsVisible(const BoundingBox& worldAABB) const
{
    return _frustum.Intersects(worldAABB) != DirectX::DISJOINT;
}

Unique<Culling_Manager> Culling_Manager::Create()
{
    return make_unique<Culling_Manager>();
}

void Culling_Manager::Free()
{
    __super::Free();
}
