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

HRESULT ClientScreenFxManager::Initialize()
{
    {
        RadialBlur_ScreenFx::RadialBlurDesc desc{};
        desc.center = Vec2(0.5f, 0.5f);
        desc.intensity = 0.05f;
        desc.radius = 1.25f;             // 화면 외곽 쪽에 집중되도록 반경 설정
        desc.duration = 0.15f;
        desc.fadeOutTime = 0.05f;

        GAME->Play_ScreenFx(RadialBlur_ScreenFx::Create(&desc));
    }

    return S_OK;
}

void ClientScreenFxManager::Play_BattleInCounterFx()
{
    auto killCondition = [&](bool& outShouldEnd) {
        if (CLIENT->Is_BattleActive()) {
            outShouldEnd = true;
        }
    };

    Distortion_ScreenFx::DistortionDesc distDesc;
    distDesc.passType = ETOI(ScreenFx_Type::Distortion);
    distDesc.tag = L"BattleInCounterFx";
    distDesc.duration = 2.f;
    distDesc.endPredicate = killCondition;
    distDesc.bulgeIntensity = 0.575f;
    distDesc.pullDirection = Vec2{ 0.f, 0.f };
    distDesc.pullIntensity = 0.25f;
    distDesc.isReverse = true;

    GAME->Play_ScreenFx(Distortion_ScreenFx::Create(&distDesc));
}

bool ClientScreenFxManager::Is_BattleInCounterFxFinished() const
{
    return GAME->Is_ScreenFxFinished(L"BattleInCounterFx");
}

void ClientScreenFxManager::Transition_To_BattleField()
{
    BattleTransition_ScreenFx::BattleTransitionDesc wipeDesc;
    wipeDesc.passType = ETOI(ScreenFx_Type::BattleTransition);
    wipeDesc.duration = 2.f;
    wipeDesc.edgeSoftness = 0.1f;
    wipeDesc.maxRadius = 2.f;
    wipeDesc.useBlackSource = _useBlackSourceForNextBattleTransition;
    _useBlackSourceForNextBattleTransition = false;

    wipeDesc.endPredicate = nullptr;

    GAME->Play_ScreenFx(BattleTransition_ScreenFx::Create(&wipeDesc));
}

void ClientScreenFxManager::Request_NextBattleTransitionBlackSource()
{
    _useBlackSourceForNextBattleTransition = true;
}

Shared<ClientScreenFxManager> ClientScreenFxManager::Create()
{
    auto instance = make_shared<ClientScreenFxManager>();

    CHECK_FAILED(instance->Initialize(), nullptr);

    return instance;
}

void ClientScreenFxManager::Free()
{
    _cameraBlur.reset();

    Base::Free();
}

NS_END

