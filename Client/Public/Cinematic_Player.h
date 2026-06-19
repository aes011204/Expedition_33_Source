#pragma once
#include "Base.h"
#include "Cinematic_Types.h"

NS_BEGIN(Engine)
class Camera;
NS_END

NS_BEGIN(Client)

class CinematicCamera;
class Cinematic_Player :  public Base
{
public:
    Cinematic_Player();
    virtual ~Cinematic_Player();
    
public:
    void  Update(float timeDelta);
    void  Play();
    void  Pause();
    void  Resume();
    void  Stop();
    void  Set_CurrentFrame(int frame, bool applyCamera = true);

public:
    void  Ready_CinematicPlayer(CINEMATIC_SEQUENCE* cinematicSequence, CINEMATIC_TRACK* cinematicTrack, const Shared<CinematicCamera>& camera, bool playerOnOff);
    bool  Is_CinematicPlaying() {return _isCinematicPlaying;}
    bool  Is_CinematicPaused() {return _isCinematicPaused;}
    int   Get_CurrentFrame() {return _currentFrame;}


private:
    CINEMATIC_SEQUENCE*          _sequence = nullptr;
    CINEMATIC_KEYFRAME*          _leftKey = nullptr;
    CINEMATIC_KEYFRAME*          _rightKey = nullptr;
    Shared<CinematicCamera>      _cinematicCamera; //추후 별도 카메라 구현 예정
    CINEMATIC_TRACK*             _cinematicTrack = nullptr;
    int                          _currentFrame = 0;
    bool                         _isCinematicPlaying = false;
    bool                         _isCinematicPaused = false;

public:
    static Shared<Cinematic_Player> Create();
};
NS_END
