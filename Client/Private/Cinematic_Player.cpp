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
    //Find_SurroundingKeyFrames(_cinematicTrack, _currentFrame, _leftKey, _rightKey);
    //
    //if(_leftKey == nullptr && _rightKey == nullptr)
    //    return;
    //
    //if(_leftKey == nullptr)
    //{
    //    Apply_CameraPose(_rightKey->camPose);
    //    return;
    //}
    //
    //if(_rightKey == nullptr)
    //{
    //    Apply_CameraPose(_leftKey->camPose);
    //    return;
    //}
    //
    //if(_leftKey->frame == _rightKey->frame)
    //{
    //    Apply_CameraPose(_leftKey->camPose);
    //    return;
    //}
    //
    //CAMERAPOSE camPose {};
    //
    //float t = static_cast<float>(_currentFrame - _leftKey->frame) / static_cast<float>(_rightKey->frame - _leftKey->frame);
    //
    //camPose.position = Vec3::Lerp(_leftKey->camPose.position,_rightKey->camPose.position,t);
    //camPose.rotation = Vec3::Lerp(_leftKey->camPose.rotation, _rightKey->camPose.rotation, t);
    //camPose.fovY    = lerp(_leftKey->camPose.fovY, _rightKey->camPose.fovY, t);
    //
    //Apply_CameraPose(camPose);

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
