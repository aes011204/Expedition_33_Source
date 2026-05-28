#pragma once

#include "Base.h"
#include "Shader_CBuffer_Define.h"

NS_BEGIN(Engine)

class ENGINE_DLL LightingPipeline : public Base
{
public:
    explicit LightingPipeline(ComPtr<Device> device, ComPtr<Context> context);
    virtual ~LightingPipeline() = default;

public:
    HRESULT Initialize();

    void Set_Params(const SSAOCB& params);
    const SSAOCB& Get_Params() const;
    SSAOCB& Get_Params();

    void Set_Enabled(bool enabled);
    bool Is_Enabled() const;
    
    void Set_DeferredLightingParams(const DeferredLightingCB& params);
    const DeferredLightingCB& Get_DeferredLightingParams() const;
    DeferredLightingCB& Get_DeferredLightingParams();

    void Set_IBLEnabled(bool enabled);
    bool Is_IBLEnabled() const;

private:
    ComPtr<Device> _device;
    ComPtr<Context> _context;

    SSAOCB _ssaoCB{};
    DeferredLightingCB _deferredLightingCB{};

public:
    static Shared<LightingPipeline> Create(ComPtr<Device> device, ComPtr<Context> context);
    virtual void Free() override;
};

NS_END

