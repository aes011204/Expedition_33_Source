#include "pch.h"
#include "ScreenFx_Instance.h"
#include "GameInstance.h"

ScreenFx_Instance::ScreenFx_Instance()
    : Base{}
{

}

HRESULT ScreenFx_Instance::Initialize(ScreenFxDesc* desc)
{
    if (desc)
    {
        _passType = desc->passType;
        if (desc->endPredicate)
        {
            _endCondition.Add(desc->endPredicate);
        }
        return S_OK;
    }

    LOG_ERROR("[SCREEN FX INSTANCE] : Failed to Initialize ScreenFx");
    return E_FAIL;
}

bool ScreenFx_Instance::Check_EndCondition()
{
    bool shouldEnd = false;
    if (_endCondition.IsBound())
    {
        _endCondition.Broadcast(shouldEnd, this);
    }

    return shouldEnd;
}

void ScreenFx_Instance::Update_ScreenFx(float timeDelta)
{
    _elapsedTime += timeDelta;
    
    Evaluate_At(_elapsedTime);
}

HRESULT ScreenFx_Instance::Bind_ShaderResources(const Shared<ShaderCom>& shader)
{
    return S_OK;
}
