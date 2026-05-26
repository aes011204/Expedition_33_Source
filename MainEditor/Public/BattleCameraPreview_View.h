#pragma once

#include "Editor_Window.h"
#include "BattleCameraClipTypes.h"
#include "CameraPresetTypes.h"
#include "Client_Enum.h"
#include "BattleTurnCamera.h"
#include <array>
#include <BattleTurnCameraPoseUtils.h>

NS_BEGIN(Engine)
class GameObject;
class ModelCom;
class RenderTarget;
NS_END

NS_BEGIN(MainEditor)
class Editor_Camera_Free;

class BattleCameraPreview_View final : public Editor_Window
{
public:
    BattleCameraPreview_View();
    ~BattleCameraPreview_View() override = default;

public:
    void Update(float timeDelta) override;
    void Pre_Render() override;
    void Render() override;

private:
    enum class PreviewSlotSide : uint8
    {
        Hero,
        Enemy
    };

    struct PreviewSlot
    {
        string label;
        Vec3 offset = Vec3::Zero;
        Vec3 rotation = Vec3::Zero;
    };

    struct PreviewFormation
    {
        string name = "Default Battle Layout";
        Vec3 origin = Vec3(22.f, 2.5f, -10.24f);
        array<PreviewSlot, 3> heroes{};
        array<PreviewSlot, 3> enemies{};
    };

    struct PreviewContext
    {
        BattleCameraMode mode = BattleCameraMode::PLAYERATTACK;
        string skillId;
        PreviewSlotSide subjectSide = PreviewSlotSide::Hero;
        int subjectSlot = 1;
        PreviewSlotSide opponentSide = PreviewSlotSide::Enemy;
        int opponentSlot = 1;
        float timeSec = 0.f;
    };

    struct PreviewTransitionStep
    {
        BattleCameraMode mode = BattleCameraMode::PLAYERFOCUS;
        string skillId{};
        string animationName{};
        float durationSec = 0.75f;
    };

private:
    void Refresh_SkillList();
    void Ensure_PreviewRenderTarget(uint32 width, uint32 height);
    void Ensure_PreviewCamera();
    void Reset_DefaultFormation();
    void Rebuild_PreviewObjects();
    void Clear_PreviewObjects();
    void Position_PreviewObjects() const;
    void Render_Header();
    void Render_ContextPanel();
    void Render_PreviewActorSelector();
    void Render_SlotSelector(const char* label, PreviewSlotSide& side, int& slotIndex);
    void Render_FormationEditor();
    void Render_PreviewCanvas();
    void Render_PreviewPanel();
    void Render_TransitionPreviewPanel();
    void Render_BattlePresetEditor();
    void Render_TimelinePanel();
    void Render_BattleClipSequencer();
    void Render_SelectedBattleClipKeyframe();
    void Render_BattleClipInsertKeyframePopup();
    void Render_BattleClipRemoveKeyframePopup();
    void Render_LoadAuthoringFilePopup();
    void Apply_PreviewContext();
    void Apply_PreviewAnimation(float previewTimeSec) const;
    void Apply_BattleCameraPreviewPose(float previewTimeSec);
    float Get_CurrentPreviewAnimationDurationSec() const;
    int  Get_BattleClipPreviewFrameMax() const;
    void Clear_PreviewContext();
    void Refresh_PreviewAnimationList();
    void Try_AutoSelectPreviewActorAndAnimation();
    void Sort_BattleClipTrackKeyframes(BattleCameraTrack& track);
    void Insert_BattleClipKeyAtFrame(int trackIndex, int frame);
    void Sync_PreviewTimeFromTimeline();
    void Ensure_BattlePresetLoaded();
    void Load_BattlePresetForMode(BattleCameraMode mode);
    void Load_BattlePresetForCurrentMode();
    void Load_BattleClipForCurrentMode();
    void Refresh_AuthoringFileList();
    void Load_SelectedAuthoringFile();
    bool Apply_AuthoringFileSelection(const fs::path& filePath);
    void Apply_AuthoringFileContext(BattleCameraMode mode, const string& skillId);
    void Update_BattlePresetAuthoringInput(float timeDelta);
    void Refresh_SelectedBattleClipKey();
    void Reset_PreviewPoseState();
    void Apply_DefaultPreviewSlotsForMode(BattleCameraMode mode, bool resetPoseState = true);
    void Apply_PreviewTransitionMode(BattleCameraMode mode);
    void Apply_PreviewSkillContext(const string& skillId);
    void Apply_TransitionStepContext(const PreviewTransitionStep& step);
    void Capture_CurrentPreviewIntoTransitionStep(PreviewTransitionStep& step);
    void Set_TransitionPreviewLocalTime(float timeSec);
    Client::BattleTurnCameraEvaluatedPose Capture_PreviewCurrentPose() const;
    bool Evaluate_PreviewModePose(BattleCameraMode mode, float previewTimeSec, bool includeClip, Client::BattleTurnCameraEvaluatedPose& outPose) const;
    void Apply_PreviewEvaluatedPose(const Client::BattleTurnCameraEvaluatedPose& pose);
    void Rebuild_TransitionPreviewStepAtTime(int stepIndex, float timeSec);
    void Rebuild_TransitionPreviewSequenceAtTime(float timeSec);
    void Start_TransitionPreviewPlayback();
    void Stop_TransitionPreviewPlayback();
    void Update_TransitionPreviewPlayback(float timeDelta);
    void Set_TransitionEditTargetStep(int stepIndex);
    void Update_PreviewAttackActorMotion(float previewTimeSec);
    Shared<GameObject> Resolve_PreviewBattleActor(PreviewSlotSide side);

    Shared<Client::BattleTurnCamera> Get_BattleTurnCamera() const;
    Shared<ModelCom> Resolve_PreviewSubjectModel() const;
    string Get_SelectedSkillId() const;
    const char* Get_SlotSideLabel(PreviewSlotSide side) const;
    const PreviewSlot* Get_PreviewSlot(PreviewSlotSide side, int slotIndex) const;
    Vec3 Get_PreviewSlotWorldPosition(PreviewSlotSide side, int slotIndex) const;
    string Get_SelectedHeroPrefabName() const;
    string Get_SelectedEnemyPrefabName() const;
    BattleCameraMode Get_BattlePresetEditorMode() const;
    BattleCameraTrack* Get_SelectedBattleClipTrack();
    BattleCameraKeyframe* Get_SelectedBattleClipKeyframe();
    float Get_TransitionSequenceDuration() const;
    void Resolve_TransitionSequenceTime(float time, int& outStepIndex, float& outStepLocalTime);
    void Scrub_TransitionSequence(float timeSec);

private:
    PreviewContext _previewContext{};
    PreviewFormation _previewFormation{};
    vector<string> _skillIds{};
    vector<string> _enemyPrefabNames{};
    Client::HeroId _selectedHeroId = Client::HeroId::Gustave;
    int _selectedEnemyPrefabIndex = 0;
    int _selectedSkillIndex = -1;
    bool _useSkillOverride = false;
    bool _autoPreview = false;
    bool _previewDistanceOffsetEnabled = true;
    bool _previewFovOffsetEnabled = true;
    bool _previewShakeEnabled = true;
    bool _previewSideOffsetEnabled = true;
    bool _previewPivotOffsetEnabled = true;
    bool _previewYawOffsetEnabled = true;
    bool _previewEnabled = true;
    bool _previewDirty = false;
    bool _previewObjectsDirty = true;
    bool _previewObjectsBegunPlay = false;
    bool _animationListDirty = true;
    bool _timelinePlaying = false;
    float _topSectionHeight = 420.f;
    uint32 _previewWidth = 1;
    uint32 _previewHeight = 1;
    bool _isPreviewHovered = false;
    bool _isPreviewActive = false;
    Shared<RenderTarget> _previewRenderTarget{};
    Shared<Editor_Camera_Free> _previewCamera{};
    Shared<GameObject> _previewHeroObject{};
    Shared<GameObject> _previewEnemyObject{};
    vector<string> _previewAnimationNames{};
    int _selectedPreviewAnimationIndex = -1;
    BattleCameraPreset _editingBattlePreset{};
    BattleCameraMode _loadedBattlePresetMode = BattleCameraMode::PLAYERATTACK;
    string _loadedBattlePresetSkillId{};
    bool _hasLoadedBattlePreset = false;
    BattleCameraMode _loadedBattleClipMode = BattleCameraMode::PLAYERATTACK;
    string _loadedBattleClipSkillId{};
    BattleCameraClip _editingBattleClip{ 1.f };
    bool _hasLoadedBattleClip = false;
    int _battleClipSelectedTrackIndex = 0;
    int _battleClipSelectedKeyIndex = -1;
    int _battleClipCurrentFrame = 0;
    bool _requestOpenBattleClipInsertPopup = false;
    bool _requestRemoveBattleClipKeyPopup = false;
    int _battleClipContextTrackIndex = -1;
    int _battleClipContextFrame = 0;
    vector<fs::path> _authoringFilePaths{};
    int _selectedAuthoringFileIndex = -1;
    Vec3 _previewSmoothedTargetPos = Vec3::Zero;
    bool _previewHasSmoothedTarget = false;
    BattleCameraMode _previewPoseMode = BattleCameraMode::PLAYERATTACK;
    PreviewSlotSide _previewPoseSubjectSide = PreviewSlotSide::Hero;
    int _previewPoseSubjectSlot = 1;
    float _previewPoseLastTimeSec = 0.f;
    vector<PreviewTransitionStep> _transitionPreviewSteps
    {
        { BattleCameraMode::PLAYERFOCUS, {}, {}, 0.75f },
        { BattleCameraMode::ENEMYFOCUS, {}, {}, 0.75f },
    };
    bool _transitionPreviewPlaying = false;
    bool _transitionPreviewLoop = true;
    int _transitionPreviewActiveStepIndex = -1;
    int _transitionPreviewEditTargetStepIndex = 0;
    bool _transitionPreviewEditTargetPinned = false;
    bool _transitionPreviewAutoFollowActiveStep = true;
    float _transitionPreviewElapsed = 0.f;
    float _transitionPreviewSequenceTimeSec = 0.f;

public:
    static Shared<BattleCameraPreview_View> Create();
};

NS_END
