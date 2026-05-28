#include "pch.h"
#include "ClientScreenFxManager.h"

#include "BattleTransition_ScreenFx.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "RadialBlur_ScreenFx.h"
#include "CA_ScreenFx.h"
#include "Distortion_ScreenFx.h"

NS_BEGIN(Client)

void ClientScreenFxManager::Play_BattleInCounterFx()
{
    auto killCondition = [](bool& outShouldEnd, Engine::ScreenFx_Instance* fx) {
        if (CLIENT->Is_BattleActive()) {
            outShouldEnd = true;
        }
    };

    Distortion_ScreenFx::DistortionDesc distDesc;
    distDesc.passType = ETOI(ScreenFx_Type::Distortion);
    distDesc.duration = 2.f;
    distDesc.endPredicate = killCondition;
    distDesc.bulgeIntensity = 0.1f;
    distDesc.pullDirection = Vec2{ 1.f, 1.f };
    distDesc.pullIntensity = 0.5f;
    distDesc.isReverse = true;

    GAME->Play_ScreenFx(Distortion_ScreenFx::Create(&distDesc));
}

void ClientScreenFxManager::Transition_To_BattleField()
{
    // 기존에 존재하는 BackgroundMask RT를 재활용하여 현재 화면(디스토션 씬)을 캡처합니다.
    GAME->Copy_CurrentRTV_To(RT::BackgroundMask);

    BattleTransition_ScreenFx::BattleTransitionDesc wipeDesc;
    wipeDesc.passType = ETOI(ClientScreenFxManager::ScreenFx_Type::BattleTransition);
    wipeDesc.duration = 1.5f;       // 1.5초 동안 서서히 원이 커짐
    wipeDesc.edgeSoftness = 0.05f;  // 원형 테두리 부드러움
    wipeDesc.maxRadius = 1.5f;      // 16:9 비율 기준 화면을 다 덮는 대각선 길이 이상

    // 전투 종료 시 또는 연출이 완전히 화면을 가렸을 때(마스크 텍스처가 완전히 덮였을 때) 파괴
    wipeDesc.endPredicate = [](bool& outShouldEnd, Engine::ScreenFx_Instance* fx) {
        // 1. 전투가 비활성화 되었거나
        // 2. 이펙트가 지정한 duration(1.5초)를 넘겨 마스크가 완전히 덮였을 때
        if (!CLIENT->Is_BattleActive() || fx->Get_ElapsedTime() >= fx->Get_Duration()) {
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

