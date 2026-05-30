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
    distDesc.bulgeIntensity = 0.575f;
    distDesc.pullDirection = Vec2{ 0.f, 0.f };
    distDesc.pullIntensity = 0.25f;
    distDesc.isReverse = true;

    GAME->Play_ScreenFx(Distortion_ScreenFx::Create(&distDesc));
}

void ClientScreenFxManager::Transition_To_BattleField()
{
    BattleTransition_ScreenFx::BattleTransitionDesc wipeDesc;
    wipeDesc.passType = ETOI(ScreenFx_Type::BattleTransition);
    wipeDesc.duration = 2.5f;       
    wipeDesc.edgeSoftness = 0.1f;  
    wipeDesc.maxRadius = 2.f;      
    
    wipeDesc.endPredicate = nullptr;
    
    GAME->Play_ScreenFx(BattleTransition_ScreenFx::Create(&wipeDesc));
}

void ClientScreenFxManager::Play_ActionCameraBlur()
{
    RadialBlur_ScreenFx::RadialBlurDesc desc{};
    desc.center = Vec2(0.5f, 0.5f); 
    desc.intensity = 0.75f;     
    desc.radius = 0.85f;             // 화면 외곽 쪽에 집중되도록 반경 설정
    desc.duration = 0.3f;       
    desc.fadeOutTime = 0.2f;

    GAME->Play_ScreenFx(RadialBlur_ScreenFx::Create(&desc));
}


Shared<ClientScreenFxManager> ClientScreenFxManager::Create()
{
    return make_shared<ClientScreenFxManager>();
}

NS_END

