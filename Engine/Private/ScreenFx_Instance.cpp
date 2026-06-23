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
        _tag = desc->tag;
        _duration = desc->duration;
        _elapsedTime = 0.f;

        if (desc->endPredicate)
            _endCondition.Add(desc->endPredicate);

        return S_OK;
    }

    return S_OK;
}

bool ScreenFx_Instance::Check_EndCondition()
{
    //if (_endCondition.GetBindingCount() == 0) return false;

    bool endDuration = _duration > 0.f && _elapsedTime >= _duration;

    if (_endCondition.IsBound())
    {
        bool shouldEnd = false;
        _endCondition.Broadcast(shouldEnd);
        return endDuration || shouldEnd;
    }

    return endDuration;
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
