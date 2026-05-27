#include "pch.h"
#include "ScreenFx_Instance.h"
#include "GameInstance.h"

ScreenFx_Instance::ScreenFx_Instance()
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
    }

    LOG_ERROR("[SCREEN FX INSTANCE] : Failed to Initialize ScreenFx");
    return E_FAIL;
}

bool ScreenFx_Instance::Check_EndCondition()
{
    bool shouldEnd = false;
    if (_endCondition.IsBound())
    {
        _endCondition.Broadcast(shouldEnd);
    }

    return shouldEnd;
}
