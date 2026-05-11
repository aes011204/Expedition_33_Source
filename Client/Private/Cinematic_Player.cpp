#include "Cinematic_Player.h"
#include "Cinematic_Utils.h"
#include "FreeCamera.h"

Cinematic_Player::Cinematic_Player()
{
}

Cinematic_Player::~Cinematic_Player()
{
}

void Cinematic_Player::Update(float timeDelta)
{
    if(!_isCinematicPlaying)
        return;

    _currentFrame += 1;

    if(_currentFrame >= _sequence->endFrame)
    {
        _currentFrame = _sequence->endFrame;
        Stop();
    }

    Play();
}

void Cinematic_Player::Play()
{
    CAMERAPOSE camPose =Evaluate_CinematicPose(_cinematicTrack, _currentFrame, _leftKey, _rightKey);

    Apply_CameraPose(camPose);
}

void Cinematic_Player::Pause()
{
}

void Cinematic_Player::Stop()
{
    _isCinematicPlaying = false;
    _currentFrame = 0;
}

void Cinematic_Player::Ready_CinematicPlayer(CINEMATIC_SEQUENCE* cinematicSequence, CINEMATIC_TRACK* cinematicTrack, const Shared<Camera>& camera, bool playerOnOff)
{
    _sequence = cinematicSequence;
    _cinematicTrack = cinematicTrack;
    _cinematicCamera = camera;
    _isCinematicPlaying = playerOnOff;
}

void Cinematic_Player::Apply_CameraPose(const CAMERAPOSE& camPose)
{
  if(_cinematicCamera == nullptr)
      return;

    _cinematicCamera->Get_Transform()->Set_Position(camPose.position);
    _cinematicCamera->Get_Transform()->Set_RotationEuler(camPose.rotation);
    _cinematicCamera->Set_FovY(camPose.fovY);
}

Shared<Cinematic_Player> Cinematic_Player::Create()
{
    return make_shared<Cinematic_Player>();
}
