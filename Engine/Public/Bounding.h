#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class ENGINE_DLL Bounding : public Base
{
public:
    struct BoundingDesc
    {
        Vec3 center = Vec3::Zero;
    };

protected:
    explicit Bounding(ComPtr<Device> device, ComPtr<Context> context);
    explicit Bounding(const Bounding& Prototype) = default;
    virtual ~Bounding() = default;

public:
    virtual HRESULT Initialize(const BoundingDesc& desc);
    virtual void    Update(const Matrix& worldMatrix) = 0;

    virtual bool    Intersect(Bounding* otherBounding) = 0;

    virtual HRESULT Render_Debug(
        DirectX::PrimitiveBatch<DirectX::VertexPositionColor>* batch,
        Color color) = 0;

public:
    virtual CollisionShape  Get_Shape() const = 0;

protected:
    ComPtr<Device>   _device;
    ComPtr<Context>  _context;

    bool             _isColl = false;


public:
    virtual void Free() override;

};

NS_END
