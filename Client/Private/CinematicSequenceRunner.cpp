#include "CinematicSequenceRunner.h"
#include "ClientInstance.h"
#include "Cinematic_Player.h"
#include "Cinematic_SequenceIO.h"
#include "CinematicCamera.h"

bool CinematicSequenceRunner::Play_Sequence(const wstring& filePath, int trackIndex)
{
    if(_player == nullptr)
        _player = Cinematic_Player::Create();

    if(_player == nullptr)
        return false;

    string stringPath = String::ToString(filePath);

    if(!Cinematic_SequenceIO::LoadSequenceFromFile(stringPath, _sequence))
        return false;

    if(trackIndex < 0 || trackIndex >= _sequence.cinematicTracks.size())
        return false;

    Shared<CinematicCamera> cinematicCamera = CLIENT->Request_Cinematic();

    if(cinematicCamera == nullptr)
        return false;

    _player->Ready_CinematicPlayer(&_sequence, &_sequence.cinematicTracks[trackIndex], cinematicCamera, true);

    _player->Play();

    return true;
}

void CinematicSequenceRunner::Stop_Sequence()
{
    if(_player == nullptr)
        return;

    bool wasPlaying = _player->Is_CinematicPlaying();

    if(wasPlaying)
    {
        _player->Stop();
        CLIENT->Restore_PreviousCamera();
    }
}

void CinematicSequenceRunner::Update(float timeDelta)
{
    if(_player == nullptr)
        return;

    bool wasPlaying = _player->Is_CinematicPlaying();

    if(!wasPlaying)
        return;

        _player->Update(timeDelta);

    if(wasPlaying && !_player->Is_CinematicPlaying())
    {
        _player->Stop();
        CLIENT->Restore_PreviousCamera();
    }
}

Unique<CinematicSequenceRunner> CinematicSequenceRunner::Create()
{
    return make_unique<CinematicSequenceRunner>();
}

void CinematicSequenceRunner::Free()
{
    __super::Free();
}
