#pragma once
#include "Base.h"
#include "Cinematic_Types.h"

NS_BEGIN(Client)

class CinematicCamera;
class Cinematic_Player;
class CinematicSequenceRunner : public Base
{
    public:
        bool Play_Sequence(const wstring& filePath, int trackIndex);
        void Stop_Sequence();
        void Update(float timeDelta);

    private:
        CINEMATIC_SEQUENCE       _sequence {};
        Shared<Cinematic_Player> _player = nullptr;

    public:
    static Unique<CinematicSequenceRunner> Create();
    virtual void Free() override;
};
NS_END
