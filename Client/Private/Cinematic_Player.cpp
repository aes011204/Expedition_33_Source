#include "Cinematic_Player.h"

#include "CinematicCamera.h"
#include "Cinematic_Utils.h"
#include "FreeCamera.h"
#include <algorithm>

namespace
{
void Apply_CinematicModifier(
    const CINEMATIC_SEQUENCE* sequence,
    int currentFrame,
    const Shared<CinematicCamera>& camera,
    CAMERAPOSE& inOutPose)
{
    if (sequence == nullptr || camera == nullptr)
        return;

    const Client::CameraModifierSample modifierSample = Evaluate_CinematicModifierSample(*sequence, currentFrame);
    const float sequenceTime = Evaluate_CinematicSequenceTime(*sequence, currentFrame);

    camera->Apply_CinematicPose(inOutPose);

    if (camera->Get_Transform() == nullptr)
        return;

    const Vec3 shakeOffset = Sample_CameraModifierShakeOffset(
        modifierSample,
        sequenceTime,
        camera->Get_Transform()->Get_WorldRight(),
        camera->Get_Transform()->Get_WorldUp());

    inOutPose.position += shakeOffset;
}
}

Cinematic_Player::Cinematic_Player()
{
}

Cinematic_Player::~Cinematic_Player()
{
}

void Cinematic_Player::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if(!_isCinematicPlaying)
        return;

    if (!_isCinematicPaused)
        _currentFrame += 1;

    if(_sequence != nullptr && _currentFrame >= _sequence->endFrame)
    {
        _currentFrame = _sequence->endFrame;
        Stop();
    }

    Play();
}

void Cinematic_Player::Play()
{
    if (_sequence == nullptr || _cinematicTrack == nullptr || _cinematicCamera == nullptr)
        return;

    CAMERAPOSE camPose = Evaluate_CinematicPose(_cinematicTrack, _currentFrame, _leftKey, _rightKey);
    Apply_CinematicModifier(_sequence, _currentFrame, _cinematicCamera, camPose);
    _cinematicCamera->Apply_CinematicPose(camPose);
} 

void Cinematic_Player::Pause()
{
    if (!_isCinematicPlaying)
        return;

    _isCinematicPaused = true;
}

void Cinematic_Player::Resume()
{
    if (!_isCinematicPlaying)
        return;

    _isCinematicPaused = false;
}

void Cinematic_Player::Stop()
{
    _isCinematicPlaying = false;
    _isCinematicPaused = false;
    _currentFrame = _sequence ? _sequence->startFrame : 0;
}

void Cinematic_Player::Set_CurrentFrame(int frame)
{
    if (_sequence == nullptr)
        return;

    _currentFrame = std::clamp(frame, _sequence->startFrame, _sequence->endFrame);
    Play();
}

void Cinematic_Player::Ready_CinematicPlayer(CINEMATIC_SEQUENCE* cinematicSequence, CINEMATIC_TRACK* cinematicTrack, const Shared<CinematicCamera>& camera, bool playerOnOff)
{
    _sequence = cinematicSequence;
    _cinematicTrack = cinematicTrack;
    _cinematicCamera = camera;
    _isCinematicPlaying = playerOnOff;
    _isCinematicPaused = false;
    _currentFrame = _sequence ? _sequence->startFrame : 0;
}

Shared<Cinematic_Player> Cinematic_Player::Create()
{
    return make_shared<Cinematic_Player>();
}
