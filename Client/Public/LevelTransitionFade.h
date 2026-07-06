#pragma once

#include "Base.h"

NS_BEGIN(Engine)
class Level;
NS_END

NS_BEGIN(Client)

class UI_ScreenFade;

class LevelTransitionFade final : public Base
{
public:
    LevelTransitionFade() = default;
    ~LevelTransitionFade() override;

public:
    HRESULT Initialize(const ComPtr<Device>& Device, const ComPtr<Context>& Context);
    void    Update(float TimeDelta);

    bool    Request_LevelTransition(LevelType TargetLevel, LevelType GameEntryRevealLevel = LevelType::END);
    bool    Request_LoadedLevelTransition(LevelType TargetLevel, Shared<Level> TargetLevelInstance);
    bool    Is_Playing() const { return _phase != Phase::Idle; }

private:
    enum class Phase
    {
        Idle,
        FadeOutToLoading,
        CoveredBeforeLoading,
        FadeInLoading,
        FadeOutToLoadedLevel,
        CoveredBeforeLoadedLevel,
        FadeInLoadedLevel,
    };

private:
    void    Begin_Fade(Phase NextPhase, float TargetAlpha, float Duration);
    void    Update_Fade(float TimeDelta);
    void    Complete_CurrentPhase();

    bool    Change_ToLoadingLevel();
    bool    Change_ToLoadedLevel();

private:
    ComPtr<Device>  _device{};
    ComPtr<Context> _context{};

    Unique<UI_ScreenFade> _screenFade{};
    Shared<Level>         _targetLevelInstance{};
    LevelType             _targetLevel = LevelType::END;

    Phase _phase = Phase::Idle;
    float _elapsed = 0.f;
    float _duration = 0.f;
    float _startAlpha = 0.f;
    float _targetAlpha = 0.f;

    float _fadeOutDuration = 0.35f;
    float _fadeInDuration = 0.35f;

public:
    static Unique<LevelTransitionFade> Create(const ComPtr<Device>& Device, const ComPtr<Context>& Context);
    void Free() override;
};

NS_END
