#pragma once

#include "Editor_Window.h"
#include "BattleCameraClipTypes.h"
#include "CameraPresetTypes.h"
#include "Cinematic_Types.h"
#include <filesystem>

NS_BEGIN(Engine)
class Character;
class ModelCom;
NS_END

NS_BEGIN(Client)
class FreeCamera;
class Cinematic_Player;
class BattleTurnCamera;
NS_END

NS_BEGIN(MainEditor)

class Cinematic_Tool : public Editor_Window
{
public:
    Cinematic_Tool();
    virtual ~Cinematic_Tool() = default;

public:
    HRESULT      Initialize() override;
    void         Update(float timeDelta) override;
    virtual void Render() override;

private:
    void Render_CinematicTab();
    void Render_BattleClipTab();
    void Render_BattleClipHeader();
    void Render_BattleClipSequencer();
    void Render_BattleClipTrackEditor(const char* label, BattleCameraTrack& track);
    void Render_SelectedBattleClipKeyframe();
    void Render_BattleClipInsertKeyframePopup();
    void Render_BattleClipRemoveKeyframePopup();
    void Render_SequenceSettings();
    void Render_TrackList();
    void Render_KeyframeList();
    void Render_SelectedKeyframe();
    void Render_SelectedKeyframeEditor(CINEMATIC_KEYFRAME& keyframe);
    void Render_RightPanel();
    void Render_SelectedKeyframePopup();
    void Render_InsertKeyframePopup();
    void Render_RemoveKeyframePopup();
    void Render_PreviewState();
    void Render_LoadSequencePopup();
    void Render_ImSequencer();

    void Ensure_DefaultTrack();
    void Clamp_Selection();
    void Sort_SelectedTrackKeyframes();
    void Capture_KeyframeFromFreeCamera();
    void Insert_KeyframeAtFrame(int trackIndex, int frame);
    void Refresh_SequenceFileList();
    void Load_SelectedSequenceFile();
    void Apply_CurrentFramePose();
    void Clear_PendingInsertRequest();
    void Sort_BattleClipTrackKeyframes(BattleCameraTrack& track);
    void Insert_BattleClipKeyAtFrame(int trackIndex, int frame);
    void Apply_CurrentBattleClipPreview();

  void Clip_BattleClipHotInput();

    CINEMATIC_TRACK* Get_SelectedTrack();
    CINEMATIC_KEYFRAME* Get_SelectedKeyframe();
    BattleCameraTrack* Get_SelectedBattleClipTrack();
    BattleCameraKeyframe* Get_SelectedBattleClipKeyframe();
    Shared<Client::BattleTurnCamera> Get_BattleTurnCamera() const;
    Shared<Character> Resolve_BattleClipPreviewSubject() const;
    Shared<Character> Resolve_BattleClipPreviewOpponent() const;
    Shared<ModelCom> Resolve_BattleClipPreviewModel() const;
    void Apply_BattleClipPreviewAnimation(float previewTimeSec) const;
    void Clear_BattleClipPreviewState() const;
    Shared<Client::FreeCamera> Get_ActiveFreeCamera() const;
    bool Try_CaptureActiveFreeCameraPose(CAMERAPOSE& outPose) const;

private:
    bool                                 _isPreviewDirty  = true;
    int                                  _lastPreviewFrame = -1;
    int                                  _lastPreviewTrackIndex = -1;
    int                                  _lastPreviewKeyIndex = -1;
    Shared<Client::Cinematic_Player>     _cinematicPlayer;
    CINEMATIC_SEQUENCE                   _sequence{};
    vector<std::filesystem::path>        _sequenceFilePaths{};
    int                                  _selectedTrackIndex = 0;
    int                                  _selectedKeyIndex = -1;
    int                                  _selectedSequenceFileIndex = -1;
    int                                  _currentFrame = 0;
    bool                                 _requestOpenSelectedKeyPopup = false;
    bool                                 _requestOpenInsertKeyPopup = false;
    bool                                 _requestRemoveKeyPopup = false;
    int                                  _contextMenuTrackIndex = -1;
    int                                  _contextMenuFrame = 0;
    bool                                 _hasPendingInsertRequest = false;
    bool                                 _hasPendingInsertPose = false;
    int                                  _pendingInsertTrackIndex = -1;
    int                                  _pendingInsertFrame = 0;
    CAMERAPOSE                           _pendingInsertPose{};
    BattleCameraMode                     _selectedBattleClipMode = BattleCameraMode::PLAYERATTACK;
    BattleCameraMode                     _loadedBattleClipMode = BattleCameraMode::PLAYERATTACK;
    BattleCameraClip                     _editingBattleClip{ 1.f };
    bool                                 _hasLoadedBattleClip = false;
    int                                  _battleClipSelectedTrackIndex = 0;
    int                                  _battleClipSelectedKeyIndex = -1;
    int                                  _battleClipCurrentFrame = 0;
    bool                                 _requestOpenBattleClipInsertPopup = false;
    bool                                 _requestRemoveBattleClipKeyPopup = false;
    int                                  _battleClipContextTrackIndex = -1;
    int                                  _battleClipContextFrame = 0;
    bool                                 _isBattleClipPreviewDirty = false;

public:
    static Shared<Cinematic_Tool> Create();
};

NS_END
