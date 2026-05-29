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
        }
    };

    Distortion_ScreenFx::DistortionDesc distDesc;
    distDesc.passType = ETOI(ScreenFx_Type::Distortion);
    distDesc.duration = 2.f;
    distDesc.endPredicate = killCondition;
    distDesc.bulgeIntensity = 0.075f;
    distDesc.pullDirection = Vec2{ 0.f, 0.f };
    distDesc.pullIntensity = 0.25f;
    distDesc.isReverse = true;

    GAME->Play_ScreenFx(Distortion_ScreenFx::Create(&distDesc));
}

void ClientScreenFxManager::Transition_To_BattleField()
{
    BattleTransition_ScreenFx::BattleTransitionDesc wipeDesc;
    wipeDesc.passType = ETOI(ScreenFx_Type::BattleTransition);
    wipeDesc.duration = 3.f;       
    wipeDesc.edgeSoftness = 0.075f;  
    wipeDesc.maxRadius = 1.5f;      
    
    wipeDesc.endPredicate = nullptr;
    
    GAME->Play_ScreenFx(BattleTransition_ScreenFx::Create(&wipeDesc));
}


Shared<ClientScreenFxManager> ClientScreenFxManager::Create()
{
    return make_shared<ClientScreenFxManager>();
}

NS_END

