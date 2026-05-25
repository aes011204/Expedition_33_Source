#include "pch.h"
#include "ClientScreenFxManager.h"
#include "GameInstance.h"
#include "RadialBlur_Instance.h"
#include "CA_Instance.h"

NS_BEGIN(Client)

void ClientScreenFxManager::Play_BattleInCounterFx()
{
    // Radial Blur 연출: 중심(0.5, 0.5)에서 최대 강도 1.5로 반경 1.0 안에서 0.5초 동안 진행
    RadialBlur_Instance::RadialBlurDesc rbDesc{};
    rbDesc.type = Engine::ScreenFx_Instance::ScreenFx_Type::RadialBlur;
    rbDesc.blendMode = Engine::ScreenFx_Instance::ScreenFx_BlendMode::Add;
    rbDesc.center = Vec2(0.5f, 0.5f);
    rbDesc.radius = 1.0f;
    rbDesc.maxIntensity = 1.5f;
    rbDesc.duration = 0.5f;
    Shared<RadialBlur_Instance> radialBlur = RadialBlur_Instance::Create(rbDesc);
    
    // Chromatic Aberration 연출: 최대 강도 0.3으로 0.2초 동안 진행
    CA_Instance::CADesc caDesc{};
    caDesc.type = Engine::ScreenFx_Instance::ScreenFx_Type::ChromaticAberration;
    caDesc.blendMode = Engine::ScreenFx_Instance::ScreenFx_BlendMode::Add;
    caDesc.maxIntensity = 0.3f;
    caDesc.duration = 0.2f;
    Shared<CA_Instance> chromaticAberration = CA_Instance::Create(caDesc);
    
    // GameInstance 퍼사드를 통해 이펙트 등록
    GAME->Play_ScreenFx(radialBlur);
    GAME->Play_ScreenFx(chromaticAberration);
}

Shared<ClientScreenFxManager> ClientScreenFxManager::Create()
{
    return make_shared<ClientScreenFxManager>();
}

NS_END
