#pragma once
#include "VIBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL VIBuffer_Landscape : public VIBuffer
{
public:
    struct LAND_DESC
    {
        uint32 countX = 64;
        uint32 countZ = 64;
        float sizeX = 1.f;
        float sizeZ = 1.f;
    };

public:
    VIBuffer_Landscape(const ComPtr<Device>& device, const ComPtr<Context>& context, const LAND_DESC& desc);
    VIBuffer_Landscape(const VIBuffer_Landscape& prototype);
    virtual ~VIBuffer_Landscape() = default;

public:
    vector<float>& Get_Heights() { return _heights; }
    uint32 Get_CountX() const { return _desc.countX; }
    uint32 Get_CountZ() const { return _desc.countZ; }
    float Get_SizeX() const { return _desc.sizeX; }
    float Get_SizeZ() const { return _desc.sizeZ; }


public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;

private:
    LAND_DESC _desc{};
    vector<float> _heights;

private:
    void Rebuild_Normals(VTXNORTEX* vertices);

public:
    void Flush();
    float Sample_Height(float worldX, float worldZ) const;

public:
    static Shared<VIBuffer_Landscape> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const LAND_DESC& desc);
    virtual Shared<Component> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
