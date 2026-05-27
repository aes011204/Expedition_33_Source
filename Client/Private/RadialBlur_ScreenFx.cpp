#include "pch.h"
#include "RadialBlur_ScreenFx.h"
#include "Shader_CBuffer_Define.h"
#include <algorithm>

NS_BEGIN(Client)

RadialBlur_ScreenFx::RadialBlur_ScreenFx(const RadialBlurDesc& desc)
    : ScreenFx_Instance()
    , _center(desc.center)
    , _radius(desc.radius)
    , _maxIntensity(desc.maxIntensity)
    , _duration(desc.duration)
    , _timeAcc(0.f)
    , _currentIntensity(desc.maxIntensity)
{
}

Shared<RadialBlur_ScreenFx> RadialBlur_ScreenFx::Create(const RadialBlurDesc& desc)
{
    return Shared<RadialBlur_ScreenFx>(new RadialBlur_ScreenFx(desc), [](RadialBlur_ScreenFx* p) { delete p; });
}

NS_END
