#include "Culling_Manager.h"
#include "Shadow.h"
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

    // camera space -> world space
    XMVECTOR det;
    XMMATRIX invView = XMMatrixInverse(&det, view);
    localFrustum.Transform(_frustum, invView);
}

bool Culling_Manager::IsVisible(const BoundingBox& worldAABB) const
{
    return _frustum.Intersects(worldAABB) != DirectX::DISJOINT;
}

bool Culling_Manager::IsVisible(const BoundingBox& worldAABB, const BoundingFrustum& frustum) const
{
    return frustum.Intersects(worldAABB) != DirectX::DISJOINT;
}

bool Culling_Manager::IsVisible(const BoundingBox& worldAABB, const ShadowCascadeInfluenceVolume& volume) const
{
    if (volume.planeCount == 0u)
        return true;

    if (!volume.bounds.Intersects(worldAABB))
        return false;

    Vec3 corners[8]{};
    worldAABB.GetCorners(corners);

    constexpr float kPlaneEpsilon = 0.001f;
    for (uint32 planeIndex = 0; planeIndex < volume.planeCount; ++planeIndex)
    {
        const Vec4& plane = volume.planes[planeIndex];
        bool allOutside = true;

        for (const Vec3& corner : corners)
        {
            const float distance =
                (plane.x * corner.x) +
                (plane.y * corner.y) +
                (plane.z * corner.z) +
                plane.w;

            if (distance <= kPlaneEpsilon)
            {
                allOutside = false;
                break;
            }
        }

        if (allOutside)
            return false;
    }

    return true;
}

bool Culling_Manager::IsVisible(const BoundingBox& worldAABB, const Matrix& view, const Matrix& proj) const
{
    BoundingFrustum localFrustum;
    BoundingFrustum::CreateFromMatrix(localFrustum, proj);

    XMVECTOR det;
    XMMATRIX invView = XMMatrixInverse(&det, view);
    localFrustum.Transform(localFrustum, invView);

    return localFrustum.Intersects(worldAABB) != DirectX::DISJOINT;
}

bool Culling_Manager::IsVisibleInPerspectiveRange(
    const BoundingBox& worldAABB,
    const Matrix& view,
    float fovY,
    float aspect,
    float nearZ,
    float farZ) const
{
    const Matrix proj = XMMatrixPerspectiveFovLH(
        max(0.001f, fovY),
        max(0.001f, aspect),
        max(0.001f, nearZ),
        max(max(0.001f, nearZ) + 1.f, farZ));

    return IsVisible(worldAABB, view, proj);
}

bool Culling_Manager::IsVisible(
    const BoundingBox& worldAABB,
    const Matrix* viewMatrices,
    const Matrix* projMatrices,
    uint32 frustumCount) const
{
    if (viewMatrices == nullptr || projMatrices == nullptr || frustumCount == 0u)
        return true;

    for (uint32 frustumIndex = 0; frustumIndex < frustumCount; ++frustumIndex)
    {
        if (IsVisible(worldAABB, viewMatrices[frustumIndex], projMatrices[frustumIndex]))
            return true;
    }

    return false;
}

bool Culling_Manager::Is_Visible_In_OrthographicShadowSpace(const BoundingBox& worldAABB, const Matrix& viewProj) const
{
    Vec3 corners[8]{};
    worldAABB.GetCorners(corners);

    Vec3 minNDC(FLT_MAX, FLT_MAX, FLT_MAX);
    Vec3 maxNDC(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (const Vec3& corner : corners)
    {
        const Vec3 ndc = Vec3::Transform(corner, viewProj);
        minNDC = Vec3::Min(minNDC, ndc);
        maxNDC = Vec3::Max(maxNDC, ndc);
    }

    if (maxNDC.x < -1.f || minNDC.x > 1.f)
        return false;

    if (maxNDC.y < -1.f || minNDC.y > 1.f)
        return false;

    if (maxNDC.z < 0.f || minNDC.z > 1.f)
        return false;

    return true;
}

void Culling_Manager::BuildOctree(const vector<BoundingBox>& worldAABBs)
{
    ClearOctree();

    if (worldAABBs.empty())
    {
        return;
    }

    Vec3 minPt = Vec3(worldAABBs[0].Center) - Vec3(worldAABBs[0].Extents);
    Vec3 maxPt = Vec3(worldAABBs[0].Center) + Vec3(worldAABBs[0].Extents);

    for (const auto& aabb : worldAABBs)
    {
        Vec3 lo = Vec3(aabb.Center) - Vec3(aabb.Extents);
        Vec3 hi = Vec3(aabb.Center) + Vec3(aabb.Extents);
        minPt = Vec3::Min(minPt, lo);
        maxPt = Vec3::Max(maxPt, hi);
    }

    _octreeRoot = make_unique<OctreeNode>();
    _octreeRoot->bounds.Center = (minPt + maxPt) * 0.5f;
    _octreeRoot->bounds.Extents = (maxPt - minPt) * 0.5f;

    for (uint32 i = 0; i < static_cast<uint32>(worldAABBs.size()); ++i)
    {
        _octreeRoot->indices.push_back(i);
    }


    function<void(OctreeNode*, int)> subdivide = [&](OctreeNode* node, int depth)
        {
            if (depth >= MAX_DEPTH || static_cast<int>(node->indices.size()) <= MAX_OBJECTS)
            {
                return;
            }

            Vec3 c = node->bounds.Center;
            Vec3 e = Vec3(node->bounds.Extents) * 0.5f;

            Vec3 offsets[8] = {
            {-e.x,-e.y,-e.z}, { e.x,-e.y,-e.z},
            {-e.x, e.y,-e.z}, { e.x, e.y,-e.z},
            {-e.x,-e.y, e.z}, { e.x,-e.y, e.z},
            {-e.x, e.y, e.z}, { e.x, e.y, e.z},
            };

            for (int i = 0; i < 8; ++i)
            {
                node->children[i] = make_unique<OctreeNode>();
                node->children[i]->bounds.Center = c + offsets[i];
                node->children[i]->bounds.Extents = e;
            }

            for (uint32 idx : node->indices)
            {
                for (int i = 0; i < 8; ++i)
                {
                    if (node->children[i]->bounds.Intersects(worldAABBs[idx]))
                    {
                        node->children[i]->indices.push_back(idx);
                    }
                }
            }

            node->indices.clear();

            for (int i = 0; i < 8; ++i)
            {
                subdivide(node->children[i].get(), depth + 1);
            }
        };
    subdivide(_octreeRoot.get(), 0);
}

void Culling_Manager::QueryVisible(vector<uint32>& outIndices) const
{
    if (!_octreeRoot)
    {
        return;
    }

    function<void(const OctreeNode*)> traverse = [&](const OctreeNode* node)
        {
            if (_frustum.Intersects(node->bounds) == DirectX::DISJOINT)
            {
                return;
            }

            if (node->IsLeaf())
            {
                for (uint32 idx : node->indices)
                {
                    outIndices.push_back(idx);
                }
                return;
            }

            for (int i = 0; i < 8; ++i)
            {
                if (node->children[i])
                {
                    traverse(node->children[i].get());
                }
            }
        };

    traverse(_octreeRoot.get());
    sort(outIndices.begin(), outIndices.end());
    outIndices.erase(unique(outIndices.begin(), outIndices.end()), outIndices.end());
}

void Culling_Manager::ClearOctree()
{
    _octreeRoot.reset();
}

Unique<Culling_Manager> Culling_Manager::Create()
{
    return make_unique<Culling_Manager>();
}

void Culling_Manager::Free()
{
    __super::Free();
}
