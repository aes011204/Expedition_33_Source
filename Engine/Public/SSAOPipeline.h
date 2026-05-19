#pragma once

#include "Base.h"
#include "Shader_CBuffer_Define.h"

NS_BEGIN(Engine)

class ENGINE_DLL SSAOPipeline : public Base
{
public:
    explicit SSAOPipeline(ComPtr<Device> device, ComPtr<Context> context);
    virtual ~SSAOPipeline() = default;

public:
    HRESULT Initialize();

    void Set_Params(const SSAOCB& params);
    const SSAOCB& Get_Params() const;
    SSAOCB& Get_Params();

    void Set_Enabled(bool enabled);
    bool Is_Enabled() const;

private:
    ComPtr<Device> _device;
    ComPtr<Context> _context;

    SSAOCB _ssaoCB{};

public:
    static Shared<SSAOPipeline> Create(ComPtr<Device> device, ComPtr<Context> context);
    virtual void Free() override;
};

NS_END

