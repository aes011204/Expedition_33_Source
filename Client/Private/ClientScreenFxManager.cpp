#include "pch.h"
#include "ClientScreenFxManager.h"

#include "BattleTransition_ScreenFx.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "RadialBlur_ScreenFx.h"
#include "CA_ScreenFx.h"
#include "Distortion_ScreenFx.h"
#include "Renderer_Define.h"

NS_BEGIN(Client)

void ClientScreenFxManager::Play_BattleInCounterFx()
{
    auto killCondition = [&](bool& outShouldEnd) {
        if (CLIENT->Is_BattleActive()) {
            outShouldEnd = true;
            Transition_To_BattleField();
        }
    };

    Distortion_ScreenFx::DistortionDesc distDesc;
    distDesc.passType = ETOI(ScreenFx_Type::Distortion);
    distDesc.duration = 2.f;
    distDesc.endPredicate = killCondition;
    distDesc.bulgeIntensity = 0.075f;
    distDesc.pullDirection = Vec2{ 1.f, 1.f };
    distDesc.pullIntensity = 0.35f;
    distDesc.isReverse = true;

    GAME->Play_ScreenFx(Distortion_ScreenFx::Create(&distDesc));
}

void ClientScreenFxManager::Transition_To_BattleField()
{

    BattleTransition_ScreenFx::BattleTransitionDesc wipeDesc;
    wipeDesc.passType = ETOI(ClientScreenFxManager::ScreenFx_Type::BattleTransition);
    wipeDesc.duration = 1.5f;       // 1.5초 동안 서서히 원이 커짐
    wipeDesc.edgeSoftness = 0.05f;  // 원형 테두리 부드러움
    wipeDesc.maxRadius = 1.5f;      // 16:9 비율 기준 화면을 다 덮는 대각선 길이 이상

    wipeDesc.endPredicate = [](bool& outShouldEnd) {
        if (!CLIENT->Is_BattleEnterTransitionPlaying()) {
            outShouldEnd = true;
        }
    };

    GAME->Play_ScreenFx(BattleTransition_ScreenFx::Create(&wipeDesc));
}


Shared<ClientScreenFxManager> ClientScreenFxManager::Create()
{
    return make_shared<ClientScreenFxManager>();
}

NS_END

