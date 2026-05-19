#include "SSAOPipeline.h"

NS_BEGIN(Engine)

SSAOPipeline::SSAOPipeline(ComPtr<Device> device, ComPtr<Context> context)
    : _device{ device }
    , _context{ context }
{
}

HRESULT SSAOPipeline::Initialize()
{
    _ssaoCB = SSAOCB{};
    return S_OK;
}

void SSAOPipeline::Set_Params(const SSAOCB& params)
{
    _ssaoCB = params;
}

const SSAOCB& SSAOPipeline::Get_Params() const
{
    return _ssaoCB;
}

SSAOCB& SSAOPipeline::Get_Params()
{
    return _ssaoCB;
}

void SSAOPipeline::Set_Enabled(bool enabled)
{
    _ssaoCB.SSAOEnabled = enabled ? 1u : 0u;
}

bool SSAOPipeline::Is_Enabled() const
{
    return _ssaoCB.SSAOEnabled != 0u;
}

Shared<SSAOPipeline> SSAOPipeline::Create(ComPtr<Device> device, ComPtr<Context> context)
{
    Shared<SSAOPipeline> instance = make_shared<SSAOPipeline>(device, context);
    if (FAILED(instance->Initialize()))
    {
        MSG_BOX("Failed to Create : SSAOPipeline");
        return nullptr;
    }

    return instance;
}

void SSAOPipeline::Free()
{
}

NS_END

