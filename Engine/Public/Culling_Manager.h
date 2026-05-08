#pragma once
#include "Base.h"

NS_BEGIN(Engine)

class ENGINE_DLL Culling_Manager : public Base
{
public:
    Culling_Manager();
    virtual ~Culling_Manager();

private:
    BoundingFrustum _frustum{};

public:
    void UpdateFrustum(const Matrix& view, const Matrix& proj);
    bool IsVisible(const BoundingBox& worldAABB) const;

public:
    static Unique<Culling_Manager> Create();
    void Free() override;
};

NS_END
