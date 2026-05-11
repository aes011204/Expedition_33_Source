#pragma once
#include "Base.h"

NS_BEGIN(Engine)

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

    void BuildOctree(const vector<BoundingBox>& worldAABBs);
    void QueryVisible(vector<uint32>& outIndices) const;
    void ClearOctree();

public:
    static Unique<Culling_Manager> Create();
    void Free() override;
};

NS_END
