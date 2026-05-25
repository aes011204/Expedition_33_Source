#include "pch.h"
#include "ScreenFx_Instance.h"
#include "GameInstance.h"

ScreenFx_Instance::ScreenFx_Instance(ScreenFxDesc* desc)
{
    if (desc != nullptr)
    {
        _type = desc->type;
        _blendMode = desc->blendMode;
        _endCondition = desc->endCondition;
    }
}

bool ScreenFx_Instance::Update_ScreenFx(float timeDelta)
{
    bool shouldEnd = false;
    if (_endCondition.IsBound())
    {
        _endCondition.Broadcast(shouldEnd);

        if (shouldEnd)
            return true;
    }
    
    return shouldEnd;
}
