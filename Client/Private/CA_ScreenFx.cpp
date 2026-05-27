#include "pch.h"
#include "CA_ScreenFx.h"

#include "Shader_CBuffer_Define.h"
#include <algorithm>

NS_BEGIN(Client)

CA_ScreenFx::CA_ScreenFx(const CADesc& desc)
    : ScreenFx_Instance()
    , _maxIntensity(desc.maxIntensity)
    , _duration(desc.duration)
    , _timeAcc(0.f)
    , _currentIntensity(desc.maxIntensity)
{
}

Shared<CA_ScreenFx> CA_ScreenFx::Create(const CADesc& desc)
{
    return Shared<CA_ScreenFx>(new CA_ScreenFx(desc), [](CA_ScreenFx* p) { delete p; });
}

NS_END
