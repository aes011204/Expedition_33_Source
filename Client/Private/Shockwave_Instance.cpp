#include "pch.h"
#include "Shockwave_Instance.h"
#include "Shader_CBuffer_Define.h"
#include "ShaderCom.h"
#include <algorithm>

NS_BEGIN(Client)

Shockwave_Instance::Shockwave_Instance(const ShockwaveDesc& desc)
    : ScreenFx_Instance((ScreenFxDesc*)&desc)
    , _desc(desc)
{
}

void Shockwave_Instance::Update(float timeDelta)
{
    // 베이스의 시간/진행률 갱신
    __super::Update(timeDelta);

    // 1. 반경(Radius) 팽창: 시간이 지남에 따라 중심에서 밖으로 퍼져나감
    // 초반에 빠르게 퍼지고 뒤로 갈수록 느려지는 Ease-Out 효과
    float easeOutProgress = 1.0f - pow(1.0f - _progress, 3.0f); 
    _currentRadius = _desc.maxRadius * easeOutProgress;

    // 2. 블러 및 색수차 강도 조절: 중간쯤에 최대치를 찍고 사라짐 (Parabola)
    float parabola = 4.0f * _progress * (1.0f - _progress);
    _currentBlur = _desc.maxBlurIntensity * parabola;
    _currentCA = _desc.maxCAIntensity * parabola;

    // 3. Bulge(공간 왜곡) 조절: 터지는 순간(progress 0~0.2) 가장 강했다가 빠르게 감소
    float impactFade = std::clamp(1.0f - (_progress * 3.0f), 0.0f, 1.0f);
    _currentBulge = _desc.maxBulgeIntensity * impactFade;
}

HRESULT Shockwave_Instance::Bind_FxConstantBuffer(ShaderCom* shader)
{
    Engine::ScreenFxCB cbData{};
    
    cbData.radialBlurCenter = _desc.center;
    cbData.radialBlurRadius = _currentRadius;
    cbData.radialBlurIntensity = _currentBlur;
    
    cbData.chromaticAberrationIntensity = _currentCA;
    cbData.bulgeIntensity = _currentBulge;

    return shader->Bind_CBufferData(cbData);
}

Shared<Shockwave_Instance> Shockwave_Instance::Create(const ShockwaveDesc& desc)
{
    Shared<Shockwave_Instance> pInstance = Shared<Shockwave_Instance>(new Shockwave_Instance(desc), [](Shockwave_Instance* p) { delete p; });
    return pInstance;
}

NS_END
