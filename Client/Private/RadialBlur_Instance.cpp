#include "pch.h"
#include "RadialBlur_Instance.h"
#include "Shader_CBuffer_Define.h"
#include <algorithm>

NS_BEGIN(Client)

RadialBlur_Instance::RadialBlur_Instance(const RadialBlurDesc& desc)
    : ScreenFx_Instance((ScreenFxDesc*)&desc)
    , _center(desc.center)
    , _radius(desc.radius)
    , _maxIntensity(desc.maxIntensity)
    , _duration(desc.duration)
    , _timeAcc(0.f)
    , _currentIntensity(desc.maxIntensity)
{
}

bool RadialBlur_Instance::Update_ScreenFx(float timeDelta)
{
    _timeAcc += timeDelta;

    // 시간에 따라 강도를 선형으로 감소시킴 (Fade Out)
    float ratio = 1.0f - std::clamp(_timeAcc / _duration, 0.0f, 1.0f);
    _currentIntensity = _maxIntensity * ratio;

    // 지속시간이 끝나면 종료 처리
    if (_timeAcc >= _duration)
    {
        return true; // true 반환 시 삭제됨
    }

    // EndCondition 델리게이트 체크 로직은 부모에 위임
    return ScreenFx_Instance::Update_ScreenFx(timeDelta);
}

void RadialBlur_Instance::Apply_To(Engine::ScreenFxCB& outParameters)
{
    outParameters.radialBlurCenter = _center;
    outParameters.radialBlurRadius = _radius;
    outParameters.radialBlurIntensity = _currentIntensity;
}

Shared<RadialBlur_Instance> RadialBlur_Instance::Create(const RadialBlurDesc& desc)
{
    return Shared<RadialBlur_Instance>(new RadialBlur_Instance(desc), [](RadialBlur_Instance* p) { delete p; });
}

NS_END
