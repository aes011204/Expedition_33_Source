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
class BattleTurnCamera;
class CinematicCamera;
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
    void Render_BattleClipTrackEditor(const char* label, CameraTrack& track);
    void Render_SelectedBattleClipKeyframe();
    void Render_BattleClipInsertKeyframePopup();
    void Render_BattleClipRemoveKeyframePopup();
    void Render_SequenceSettings();
    void Render_TrackList();
    void Render_KeyframeList();
    void Render_SelectedKeyframe();
    void Render_SelectedKeyframeEditor(CINEMATIC_KEYFRAME& keyframe);
    void Render_SelectedActorRelativeCameraKeyframeEditor(CINEMATIC_ACTOR_RELATIVE_CAMERA_KEYFRAME& keyframe);
    void Render_SelectedModifierKeyframeEditor(CameraKeyframe& keyframe);
    void Render_RightPanel();
    void Render_EventTrack();
    void Render_ActorRelativeCameraTracks();
    void Render_ActorTransformTracks();
    void Render_PartRotationTracks();
    void Render_SelectedKeyframePopup();
    void Render_InsertKeyframePopup();
    void Render_RemoveKeyframePopup();
    void Render_PreviewState();
    void Toggle_PreviewPlayback();
    void Start_PreviewPlayback();
    void Restart_PreviewPlayback();
    void Render_LoadSequencePopup();
    void Render_SequenceOverwriteWarningPopups();
    void Render_ImSequencer();
    void Render_CameraShots();

    void Ensure_DefaultTrack();
    void Clamp_Selection();
    void Step_PreviewFrame(int frameDelta);
    void Step_SelectedTrackKeyframe(int direction);
    void Sort_SelectedTrackKeyframes();
    void Sort_SelectedModifierTrackKeyframes();
    void Capture_KeyframeFromFreeCamera();
    void Insert_KeyframeAtFrame(int trackIndex, int frame);
    void Clear_AllSequenceKeyframes();
    void Reset_ToNewSequence();
    void Refresh_SequenceFileList();
    void Load_SelectedSequenceFile();
    void Apply_CurrentFramePose();
    bool Sync_PreviewSequenceToRunner();
    void Request_CinematicPreviewRefresh(bool rebuildContext = false);
    void Sort_AllActorTransformTrackKeyframes();
    void Sort_AllPartRotationTrackKeyframes();
    void Sync_FreeCameraToCinematicPreviewPose();
    void Enter_FreeCameraEditPreview(bool requestViewportFocus = true);
    void Clear_PendingInsertRequest();
    void Sort_BattleClipTrackKeyframes(CameraTrack& track);
    void Insert_BattleClipKeyAtFrame(int trackIndex, int frame);
    void Apply_CurrentBattleClipPreview();
    bool Try_CaptureSelectedActorTransform(Vec3& outPosition, Vec3& outRotation) const;
    bool Try_CaptureSelectedPartRotation(Vec3& outRotation) const;
    bool Try_CaptureSelectedActorRelativeCameraValues(
        float& outDistance,
        float& outHeight,
        float& outSideOffset,
        float& outYawOffset,
        float& outPitch,
        float& outLookHeight) const;
    void Ensure_BattleClipSkillListLoaded();
    string Get_SelectedBattleClipSkillId() const;

  void Clip_BattleClipHotInput();

    CINEMATIC_TRACK* Get_SelectedTrack();
    CINEMATIC_KEYFRAME* Get_SelectedKeyframe();
    CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK* Get_SelectedActorRelativeCameraTrack();
    CINEMATIC_ACTOR_RELATIVE_CAMERA_KEYFRAME* Get_SelectedActorRelativeCameraKeyframe();
    CINEMATIC_PART_ROTATION_TRACK* Get_SelectedPartRotationTrack();
    CINEMATIC_PART_ROTATION_KEYFRAME* Get_SelectedPartRotationKeyframe();
    CameraTrack* Get_SelectedModifierTrack();
    CameraKeyframe* Get_SelectedModifierKeyframe();
    bool Is_SelectedActorRelativeCameraTrack() const;
    int Get_SelectedActorRelativeCameraTrackIndex() const;
    bool Is_SelectedModifierTrack() const;
    int Get_SelectedModifierTrackIndex() const;
    bool Is_SelectedPartRotationTrack() const;
    int Get_SelectedPartRotationTrackIndex() const;
    CameraTrack* Get_SelectedBattleClipTrack();
    CameraKeyframe* Get_SelectedBattleClipKeyframe();
    Shared<Client::BattleTurnCamera> Get_BattleTurnCamera() const;
    Shared<Character> Resolve_BattleClipPreviewSubject() const;
    Shared<Character> Resolve_BattleClipPreviewOpponent() const;
    Shared<ModelCom> Resolve_BattleClipPreviewModel() const;
    void Apply_BattleClipPreviewAnimation(float previewTimeSec) const;
    void Clear_BattleClipPreviewState() const;
    Shared<Client::FreeCamera> Get_ActiveFreeCamera() const;
    bool Try_CaptureActiveFreeCameraPose(CAMERAPOSE& outPose) const;

private:
    bool                                 _wasCinematicPlaying = true;
    bool                                 _isPreviewDirty  = true;
    int                                  _lastPreviewFrame = -1;
    int                                  _lastPreviewTrackIndex = -1;
    int                                  _lastPreviewKeyIndex = -1;
    CINEMATIC_SEQUENCE                   _sequence{};
    vector<std::filesystem::path>        _sequenceFilePaths{};
    int                                  _selectedTrackIndex = 0;
    int                                  _selectedKeyIndex = -1;
    int                                  _selectedSequenceFileIndex = -1;
    int                                  _selectedEventIndex = -1;
    int                                  _selectedCameraShotIndex = -1;
    int                                  _selectedActorRelativeCameraTrackIndex = -1;
    int                                  _selectedActorRelativeCameraKeyIndex = -1;
    int                                  _selectedActorTransformTrackIndex = -1;
    int                                  _selectedActorTransformKeyIndex = -1;
    int                                  _selectedPartRotationTrackIndex = -1;
    int                                  _selectedPartRotationKeyIndex = -1;
    int                                  _currentFrame = 0;
    int                                  _sequencerFirstFrame = 0;
    int                                  _sequencerVisibleFrameCount = 240;
    float                                _currentFrameExact = 0.f;
    bool                                 _useExactFramePreview = false;
    bool                                 _requestOpenSelectedKeyPopup = false;
    bool                                 _requestOpenInsertKeyPopup = false;
    bool                                 _requestRemoveKeyPopup = false;
    bool                                 _requestOpenOverwriteKeyframePopup = false;
    bool                                 _requestOpenLoadSequencePopup = false;
    bool                                 _requestOpenSaveSequenceConfirmPopup = false;
    bool                                 _requestOpenLoadSequenceConfirmPopup = false;
    bool                                 _requestOpenNewEmptySequenceConfirmPopup = false;
    bool                                 _requestOpenClearAllKeysConfirmPopup = false;
    int                                  _contextMenuTrackIndex = -1;
    int                                  _contextMenuFrame = 0;
    bool                                 _hasPendingInsertRequest = false;
    bool                                 _hasPendingInsertPose = false;
    int                                  _pendingInsertTrackIndex = -1;
    int                                  _pendingInsertFrame = 0;
    CAMERAPOSE                           _pendingInsertPose{};
    BattleCameraMode                     _selectedBattleClipMode = BattleCameraMode::PLAYERATTACK;
    BattleCameraMode                     _loadedBattleClipMode = BattleCameraMode::PLAYERATTACK;
    string                               _loadedBattleClipSkillId{};
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
    bool                                 _isCinematicTabActive = false;
    bool                                 _isPreviewStoppedForEditing = false;
    bool                                 _previewApplyCameraTrack = true;
    bool                                 _previewApplyEventTrack = true;
    bool                                 _previewApplyActorTransformTrack = true;
    bool                                 _previewApplyAnimNotifies = true;
    bool                                 _previewApplyTrailNotify = true;
    bool                                 _previewApplyWeaponEchoNotify = true;
    vector<string>                       _battleClipSkillIds{};
    int                                  _selectedBattleClipSkillIndex = -1;
    bool                                 _useBattleClipSkillOverride = false;

public:
    static Shared<Cinematic_Tool> Create();
};

NS_END
