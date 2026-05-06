#pragma once
#include "Base.h"
#include "Cinematic_Types.h"

NS_BEGIN(Engine)
class Camera;
NS_END

NS_BEGIN(Client)
class FreeCamera;
class Cinematic_Player :  public Base
{
public:
    Cinematic_Player();
    virtual ~Cinematic_Player();
    
public:
    void  Update(float timeDelta);
    void  Play();
    void  Pause();
    void  Stop();
    void  Ready_CinematicPlayer(CINEMATIC_SEQUENCE* cinematicSequence, CINEMATIC_TRACK* cinematicTrack, const Shared<Camera>& camera);
    bool  Is_CinematicPlaying() {return _isCinematicPlaying;}
    int   Get_CurrentFrame() {return _currentFrame;}

public:
    void    Apply_CameraPose(const CAMERAPOSE& camPose);

private:
    CINEMATIC_SEQUENCE* _sequence;
    CINEMATIC_KEYFRAME* _leftKey = nullptr;
    CINEMATIC_KEYFRAME* _rightKey = nullptr;
    Shared<Camera>      _cinematicCamera; //추후 별도 카메라 구현 예정
    CINEMATIC_TRACK*    _cinematicTrack;
    int                _currentFrame = 0;
    bool               _isCinematicPlaying = false;

public:
    static Shared<Cinematic_Player> Create();
};
NS_END
