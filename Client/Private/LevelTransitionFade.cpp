#include "pch.h"
#include "LevelTransitionFade.h"

#include "GameInstance.h"
#include "Level_Loading.h"
#include "UI_ScreenFade.h"

LevelTransitionFade::~LevelTransitionFade() = default;

HRESULT LevelTransitionFade::Initialize(const ComPtr<Device>& Device, const ComPtr<Context>& Context)
{
    _device = Device;
    _context = Context;

    _screenFade = UI_ScreenFade::Create();
    CHECK_NULL(_screenFade, E_FAIL);

    return S_OK;
}

void LevelTransitionFade::Update(float TimeDelta)
{
    if (_screenFade)
        _screenFade->Update(TimeDelta);

    Update_Fade(TimeDelta);
}

bool LevelTransitionFade::Request_LevelTransition(LevelType TargetLevel)
{
    if (Is_Playing())
        return false;

    _targetLevel = TargetLevel;
    _targetLevelInstance.reset();

    if (_screenFade == nullptr)
        return Change_ToLoadingLevel();

    Begin_Fade(Phase::FadeOutToLoading, 1.f, _fadeOutDuration);
    return true;
}

bool LevelTransitionFade::Request_LoadedLevelTransition(LevelType TargetLevel, Shared<Level> TargetLevelInstance)
{
    if (Is_Playing())
        return false;

    _targetLevel = TargetLevel;
    _targetLevelInstance = TargetLevelInstance;

    if (_targetLevelInstance == nullptr)
        return false;

    if (_screenFade == nullptr)
        return Change_ToLoadedLevel();

    Begin_Fade(Phase::FadeOutToLoadedLevel, 1.f, _fadeOutDuration);
    return true;
}

void LevelTransitionFade::Begin_Fade(Phase NextPhase, float TargetAlpha, float Duration)
{
    _phase = NextPhase;
    _elapsed = 0.f;
    _duration = max(0.f, Duration);
    _startAlpha = _screenFade ? _screenFade->Get_FadeAlpha() : 0.f;
    _targetAlpha = clamp(TargetAlpha, 0.f, 1.f);

    if (_duration <= 0.f)
    {
        if (_screenFade)
            _screenFade->Set_FadeAlpha(_targetAlpha);

        Complete_CurrentPhase();
    }
}

void LevelTransitionFade::Update_Fade(float TimeDelta)
{
    if (_phase == Phase::Idle || _screenFade == nullptr)
        return;

    if (_duration <= 0.f)
        return;

    _elapsed += TimeDelta;
    const float ratio = clamp(_elapsed / _duration, 0.f, 1.f);
    const float alpha = std::lerp(_startAlpha, _targetAlpha, ratio);
    _screenFade->Set_FadeAlpha(alpha);

    if (ratio >= 1.f)
        Complete_CurrentPhase();
}

void LevelTransitionFade::Complete_CurrentPhase()
{
    switch (_phase)
    {
    case Phase::FadeOutToLoading:
        if (_screenFade)
            _screenFade->Set_FadeAlpha(1.f);

        if (Change_ToLoadingLevel())
        {
            Begin_Fade(Phase::FadeInLoading, 0.f, _fadeInDuration);
            return;
        }

        Begin_Fade(Phase::FadeInLoadedLevel, 0.f, _fadeInDuration);
        return;

    case Phase::FadeInLoading:
        if (_screenFade)
            _screenFade->Set_FadeAlpha(0.f);
        break;

    case Phase::FadeOutToLoadedLevel:
        if (_screenFade)
            _screenFade->Set_FadeAlpha(1.f);

        if (Change_ToLoadedLevel())
        {
            Begin_Fade(Phase::FadeInLoadedLevel, 0.f, _fadeInDuration);
            return;
        }

        Begin_Fade(Phase::FadeInLoadedLevel, 0.f, _fadeInDuration);
        return;

    case Phase::FadeInLoadedLevel:
        if (_screenFade)
            _screenFade->Set_FadeAlpha(0.f);
        break;

    case Phase::Idle:
    default:
        break;
    }

    _phase = Phase::Idle;
    _elapsed = 0.f;
    _duration = 0.f;
    _startAlpha = 0.f;
    _targetAlpha = 0.f;
    _targetLevel = LevelType::END;
    _targetLevelInstance.reset();
}

bool LevelTransitionFade::Change_ToLoadingLevel()
{
    Shared<Level_Loading> loadingLevel = Level_Loading::Create(_device.Get(), _context.Get(), _targetLevel);
    if (loadingLevel == nullptr)
    {
        MSG_BOX("Failed to Created : Level_Loading");
        return false;
    }

    if (FAILED(GAME->Change_Level(ETOI(LevelType::Loading), loadingLevel)))
    {
        MSG_BOX("Failed to Changed : Level_Loading");
        return false;
    }

    return true;
}

bool LevelTransitionFade::Change_ToLoadedLevel()
{
    if (_targetLevelInstance == nullptr)
        return false;

    if (FAILED(GAME->Change_Level(ETOI(_targetLevel), _targetLevelInstance)))
    {
        MSG_BOX("Failed to Changed : NextLevel");
        return false;
    }

    _targetLevelInstance.reset();
    return true;
}

Unique<LevelTransitionFade> LevelTransitionFade::Create(const ComPtr<Device>& Device, const ComPtr<Context>& Context)
{
    auto instance = make_unique<LevelTransitionFade>();

    if (FAILED(instance->Initialize(Device, Context)))
    {
        instance->Free();
        return nullptr;
    }

    return instance;
}

void LevelTransitionFade::Free()
{
    if (_screenFade)
    {
        _screenFade->Free();
        _screenFade.reset();
    }

    _targetLevelInstance.reset();
    __super::Free();
}
