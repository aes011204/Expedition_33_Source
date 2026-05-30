#include "LightingPipeline.h"

NS_BEGIN(Engine)

LightingPipeline::LightingPipeline(ComPtr<Device> device, ComPtr<Context> context)
    : _device{ device }
    , _context{ context }
{
}

HRESULT LightingPipeline::Initialize()
{
    _ssaoCB = SSAOCB{};
    _deferredLightingCB = DeferredLightingCB{};
    return S_OK;
}

void LightingPipeline::Set_Params(const SSAOCB& params)
{
    _ssaoCB = params;
}

const SSAOCB& LightingPipeline::Get_Params() const
{
    return _ssaoCB;
}

SSAOCB& LightingPipeline::Get_Params()
{
    return _ssaoCB;
}

void LightingPipeline::Set_Enabled(bool enabled)
{
    _ssaoCB.SSAOEnabled = enabled ? 1u : 0u;
}

bool LightingPipeline::Is_Enabled() const
{
    return _ssaoCB.SSAOEnabled != 0u;
}

void LightingPipeline::Set_DeferredLightingParams(const DeferredLightingCB& params)
{
    _deferredLightingCB = params;
}

const DeferredLightingCB& LightingPipeline::Get_DeferredLightingParams() const
{
    return _deferredLightingCB;
}

DeferredLightingCB& LightingPipeline::Get_DeferredLightingParams()
{
    return _deferredLightingCB;
}

void LightingPipeline::Set_IBLEnabled(bool enabled)
{
    _deferredLightingCB.iblEnabled = enabled ? 1u : 0u;
}

bool LightingPipeline::Is_IBLEnabled() const
{
    return _deferredLightingCB.iblEnabled != 0u;
}

Shared<LightingPipeline> LightingPipeline::Create(ComPtr<Device> device, ComPtr<Context> context)
{
    Shared<LightingPipeline> instance = make_shared<LightingPipeline>(device, context);
    if (FAILED(instance->Initialize()))
    {
        MSG_BOX("Failed to Create : LightingPipeline");
        return nullptr;
    }

    return instance;
}

void LightingPipeline::Free()
{
}

NS_END

