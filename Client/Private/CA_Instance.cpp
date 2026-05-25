#include "pch.h"
#include "CA_Instance.h"
#include "Shader_CBuffer_Define.h"
#include <algorithm>

NS_BEGIN(Client)

CA_Instance::CA_Instance(const CADesc& desc)
    : ScreenFx_Instance((ScreenFxDesc*)&desc)
    , _maxIntensity(desc.maxIntensity)
    , _duration(desc.duration)
    , _timeAcc(0.f)
    , _currentIntensity(desc.maxIntensity)
{
}

bool CA_Instance::Update_ScreenFx(float timeDelta)
{
    _timeAcc += timeDelta;

    // 강렬하게 나타났다가 빠르게 사라지도록 (비선형 감쇠 가능, 여기선 일단 선형)
    float ratio = 1.0f - std::clamp(_timeAcc / _duration, 0.0f, 1.0f);
    _currentIntensity = _maxIntensity * ratio;

    if (_timeAcc >= _duration)
    {
        return true; // 종료 조건
    }

    return ScreenFx_Instance::Update_ScreenFx(timeDelta);
}

void CA_Instance::Apply_To(Engine::ScreenFxCB& outParameters)
{
    outParameters.chromaticAberrationIntensity = _currentIntensity;
}

Shared<CA_Instance> CA_Instance::Create(const CADesc& desc)
{
    return Shared<CA_Instance>(new CA_Instance(desc), [](CA_Instance* p) { delete p; });
}

NS_END
