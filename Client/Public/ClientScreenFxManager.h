#pragma once
#include "Base.h"

NS_BEGIN(Client)

class RadialBlur_ScreenFx;

class ClientScreenFxManager final : public Base
{
public:
    ClientScreenFxManager() = default;
    ~ClientScreenFxManager() override = default;
public:
    enum class ScreenFx_Type
    {
        Vignette = 0,
        RadialBlur = 1,
        ChromaticAberration = 2,
        Bloom,
        Ghosting,
        Distortion,
        BattleTransition,
        Flash,
        FlashExclude,
        WorldDesaturate,
        WorldDesaturateExclude,
    };

private:
    HRESULT Initialize();

public:
    void Play_BattleInCounterFx();
    bool Is_BattleInCounterFxFinished() const;
    void Transition_To_BattleField();
    void Request_NextBattleTransitionBlackSource();

private:
    Shared<RadialBlur_ScreenFx> _cameraBlur{ nullptr };
    bool _useBlackSourceForNextBattleTransition{ false };

public:
    static Shared<ClientScreenFxManager> Create();
    void Free() override;
};

NS_END
