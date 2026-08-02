#pragma once
#include "Base.h"

NS_BEGIN(Engine)

struct ShadowCascadeInfluenceVolume;

class ENGINE_DLL Culling_Manager : public Base
{
private:
    static constexpr int MAX_DEPTH = 6;
    static constexpr int MAX_OBJECTS = 8;

    struct OctreeNode
    {
        BoundingBox bounds;
        vector<uint32> indices;
        Unique<OctreeNode> children[8] = {};

        bool IsLeaf() const { return children[0] == nullptr; }
    };

public:
    Culling_Manager();
    virtual ~Culling_Manager();

private:
    BoundingFrustum _frustum{};

    Unique<OctreeNode> _octreeRoot;

public:
    void UpdateFrustum(const Matrix& view, const Matrix& proj);
    bool IsVisible(const BoundingBox& worldAABB) const;
    bool IsVisible(const BoundingBox& worldAABB, const BoundingFrustum& frustum) const;
    bool IsVisible(const BoundingBox& worldAABB, const ShadowCascadeInfluenceVolume& volume) const;
    bool IsVisible(const BoundingBox& worldAABB, const Matrix& view, const Matrix& proj) const;
    bool IsVisibleInPerspectiveRange(
        const BoundingBox& worldAABB,
        const Matrix& view,
        float fovY,
        float aspect,
        float nearZ,
        float farZ) const;
    bool IsVisible(
        const BoundingBox& worldAABB,
        const Matrix* viewMatrices,
        const Matrix* projMatrices,
        uint32 frustumCount) const;
    bool Is_Visible_In_OrthographicShadowSpace(const BoundingBox& worldAABB, const Matrix& viewProj) const;

    void BuildOctree(const vector<BoundingBox>& worldAABBs);
    void QueryVisible(vector<uint32>& outIndices) const;
    void ClearOctree();
    void CollectOctreeDebugBoxes(vector<BoundingBox>& outBoxes) const;

public:
    static Unique<Culling_Manager> Create();
    void Free() override;
};

NS_END
