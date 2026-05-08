#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class ENGINE_DLL Picking : public Base
{
public:
    Picking(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual ~Picking() = default;

public:
    HRESULT Initialize();
    HRESULT Update();
    bool    Pick_WorldPosition(const POINT& pixelPos, Vec3& outWorldPos);

private:
    HRESULT Ensure_DepthReadbackBuffer();
    bool Read_DepthPixel(const POINT& mousePos, Vec4& outDepthPixel);
    bool Reconstruct_WorldPosition(const POINT& mousePos, const Vec4& depthPixel, Vec3& outWorldPos) const;

private:
    ComPtr<Device>      _device{};
    ComPtr<Context>     _context{};
    ComPtr<Texture2D>   _depthTexture{};
    vector<Vec4>        _depthPixels{};

public:
    static Unique<Picking> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual void Free() override;

};

NS_END
