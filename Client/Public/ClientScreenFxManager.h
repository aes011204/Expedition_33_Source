#pragma once
#include "Base.h"

NS_BEGIN(Client)

class ClientScreenFxManager final : public Base
{
public:
    enum class ScreenFx_Type
    {
        Vignette = 0,
        RadialBlur = 1,
        ChromaticAberration = 2,
        Bloom,
        Ghosting,
        Distortion,
    };

public:
    void Play_BattleInCounterFx();
private:

public:
    static Shared<ClientScreenFxManager> Create();
};

NS_END
