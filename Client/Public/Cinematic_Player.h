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
    void  Set_CurrentFrameExact(float frameExact, bool applyCamera = true);
    void  Rebind_Sequence(CINEMATIC_SEQUENCE* cinematicSequence, CINEMATIC_TRACK* cinematicTrack);

public:
    void  Ready_CinematicPlayer(CINEMATIC_SEQUENCE* cinematicSequence, CINEMATIC_TRACK* cinematicTrack, const Shared<CinematicCamera>& camera, bool playerOnOff, bool applyCameraTrack = true);
    bool  Is_CinematicPlaying() const { return _isCinematicPlaying; }
    bool  Is_CinematicPaused() const { return _isCinematicPaused; }

    int   Get_CurrentFrame() const { return _currentFrame; }
    float Get_CurrentFrameExact() const { return _currentFrameExact; }

    void  Set_ApplyCameraTrack(bool applyCameraTrack) { _applyCameraTrack = applyCameraTrack; }
    bool  Get_ApplyCameraTrack() const { return _applyCameraTrack; }


private:
    CINEMATIC_SEQUENCE*          _sequence = nullptr;
    CINEMATIC_KEYFRAME*          _leftKey = nullptr;
    CINEMATIC_KEYFRAME*          _rightKey = nullptr;
    Shared<CinematicCamera>      _cinematicCamera; //추후 별도 카메라 구현 예정
    CINEMATIC_TRACK*             _cinematicTrack = nullptr;

    int                          _currentFrame = 0;
    float                        _currentFrameExact = 0.f;

    bool                         _isCinematicPlaying = false;
    bool                         _isCinematicPaused = false;
    bool                         _applyCameraTrack = true;

public:
    static Shared<Cinematic_Player> Create();
};
NS_END
