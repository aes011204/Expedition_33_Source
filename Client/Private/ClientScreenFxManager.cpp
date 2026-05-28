#include "pch.h"
#include "ClientScreenFxManager.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "RadialBlur_ScreenFx.h"
#include "CA_ScreenFx.h"
#include "Distortion_ScreenFx.h"

NS_BEGIN(Client)

void ClientScreenFxManager::Play_BattleInCounterFx()
{
    float shockwaveDuration = 1.2f;
    Vec2 centerPos = Vec2(0.5f, 0.5f);

    auto killCondition = [](bool& outShouldEnd) {
        if (CLIENT->Is_BattleActive()) {
            outShouldEnd = true;
        }
    };

    Distortion_ScreenFx::DistortionDesc distDesc;
    distDesc.passType = ETOI(ScreenFx_Type::Distortion);
    distDesc.duration = shockwaveDuration;
    distDesc.endPredicate = killCondition;
    distDesc.bulgeIntensity = 0.6f;
    distDesc.pullDirection = Vec2{ -1.f, 1.f };
    distDesc.pullIntensity = 0.0f;

    RadialBlur_ScreenFx::RadialBlurDesc blurDesc;
    blurDesc.passType = ETOI(ScreenFx_Type::RadialBlur);
    blurDesc.duration = shockwaveDuration;
    blurDesc.endPredicate = killCondition;
    blurDesc.center = centerPos;
    blurDesc.radius = 1.2f;
    blurDesc.maxIntensity = 0.8f;

    CA_ScreenFx::CADesc caDesc;
    caDesc.passType = ETOI(ScreenFx_Type::ChromaticAberration);
    caDesc.duration = shockwaveDuration;
    caDesc.endPredicate = killCondition;
    caDesc.maxIntensity = 0.25f;

    GAME->Play_ScreenFx(Distortion_ScreenFx::Create(&distDesc));
    //GAME->Play_ScreenFx(RadialBlur_ScreenFx::Create(&blurDesc));
    //GAME->Play_ScreenFx(CA_ScreenFx::Create(&caDesc));
}

Shared<ClientScreenFxManager> ClientScreenFxManager::Create()
{
    return make_shared<ClientScreenFxManager>();
}

NS_END

