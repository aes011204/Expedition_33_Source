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

NS_BEGIN(Client)
class SkyBoxFlow;
class SkyCloudCard;
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

        Vec3 subjectOriginPos  = Vec3::Zero;
    };

    struct PreviewTransitionStep
    {
        BattleCameraMode mode = BattleCameraMode::PLAYERFOCUS;
        string skillId{};
        string animationName{};
        float durationSec = 0.75f;
    };

    struct PreviewSkillPhaseBlock
    {
        int phaseIndex = 0;
        string animationKey{};
        float startTimeSec = 0.f;
        float durationSec = 0.f;
    };

    struct PreviewCameraShotBlock
    {
        float startTimeSec = 0.f;
        float endTimeSec = 0.75f;
        BattleCameraMode mode = BattleCameraMode::PLAYERATTACK;
        string presetId{};
        string clipId{};
    };

    enum class TimelineEditorTab : uint8
    {
        ClipTimeline,
        TransitionPreview,
    };

    enum class PreviewShotDragMode : uint8
    {
        None,
        Move,
        ResizeStart,
        ResizeEnd,
    };

private:
    void Refresh_SkillList();
    void Ensure_PreviewRenderTarget(uint32 width, uint32 height);
    void Ensure_PreviewCamera();
    void Ensure_PreviewSkyBackdrop();
    void Ensure_PreviewGridResources();
    void Reset_DefaultFormation();
    void Apply_DefaultHeroFormation();
    void Apply_DefaultEnemyFormation();
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
    void Render_BattleClipDebugPanel();
    void Render_BattleClipInsertKeyframePopup();
    void Render_BattleClipRemoveKeyframePopup();
    void Render_UnsavedSkillChangePopup();
    void Render_SkillShotTimeline();
    void Render_LoadAuthoringFilePopup();
    void Draw_PreviewGrid() const;
    void Apply_PreviewContext();
    void Apply_PreviewAnimation(float previewTimeSec) const;
    void Apply_BattleCameraPreviewPose(float previewTimeSec);
    float Get_CurrentPreviewAnimationDurationSec() const;
    float Get_BattleClipAuthoringDurationSec() const;
    int  Get_BattleClipPreviewFrameMax() const;
    float Get_CurrentBattleClipTimeOriginSec() const;
    float Get_CurrentBattleClipTimeRangeSec() const;
    float Get_CurrentBattleClipPreviewTimeSec() const;
    float Get_BattleClipTimeFromFrame(int frame) const;
    int Get_BattleClipFrameFromTime(float clipTime) const;
    void Sync_BattleClipFrameFromPreviewTime();
    void Clear_PreviewContext();
    void Refresh_PreviewAnimationList();
    void Try_AutoSelectPreviewActorAndAnimation();
    void Sort_BattleClipTrackKeyframes(CameraTrack& track);
    void Insert_BattleClipKeyAtFrame(int trackIndex, int frame);
    void Sync_PreviewTimeFromTimeline();
    void Ensure_BattlePresetLoaded();
    void Load_BattlePresetForMode(BattleCameraMode mode, bool loadBase = false);
    void Load_BattlePresetForCurrentMode(bool loadBase = false);
    void Load_BattleDriftForMode(BattleCameraMode mode);
    void Load_BattleDriftForCurrentMode();
    void Load_BattleClipForMode(BattleCameraMode mode, bool loadBase = false, bool resetTimeline = false);
    void Load_BattleClipForCurrentMode(bool loadBase = false, bool resetTimeline = false);
    void Refresh_AuthoringFileList();
    void Load_SelectedAuthoringFile();
    bool Apply_AuthoringFileSelection(const fs::path& filePath);
    void Apply_AuthoringFileContext(BattleCameraMode mode, bool isPresetFile, const string& authoringId);
    void Update_BattlePresetAuthoringInput(float timeDelta);
    void Refresh_SelectedBattleClipKey();
    void Reset_PreviewPoseState();
    void Apply_DefaultPreviewSlotsForMode(BattleCameraMode mode, bool resetPoseState = true);
    void Apply_PreviewTransitionMode(BattleCameraMode mode);
    void Apply_PreviewSkillContext(const string& skillId);
    bool Has_UnsavedBattleAuthoringChanges() const;
    void Request_SkillOverrideChange(bool useSkillOverride, int selectedSkillIndex);
    void Apply_SkillOverrideChange(bool useSkillOverride, int selectedSkillIndex);
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
    void Refresh_PreviewSkillPhaseBlocks();
    float Resolve_PreviewAnimationDurationByName(const string& animationName) const;
    float Get_PreviewSkillSequenceDuration() const;
    PreviewSkillPhaseBlock* Get_SelectedPreviewSkillPhase();
    const PreviewSkillPhaseBlock* Get_SelectedPreviewSkillPhase() const;
    const PreviewSkillPhaseBlock* Resolve_CurrentPreviewSkillPhase(float previewTimeSec) const;
    const PreviewSkillPhaseBlock* Resolve_ActivePreviewSkillPhase(float previewTimeSec) const;
    PreviewCameraShotBlock* Get_SelectedPreviewShotBlock();
    const PreviewCameraShotBlock* Get_SelectedPreviewShotBlock() const;
    int Find_ActivePreviewShotBlockIndex(float previewTimeSec) const;
    const PreviewCameraShotBlock* Resolve_ActivePreviewShotBlock(float previewTimeSec) const;
    void Apply_ActivePreviewShotBlock(float previewTimeSec);
    bool Does_BattleAuthoringTargetMatchPreview() const;
    void Sync_BattleAuthoringTargetToPreview();
    void RetargetLoadedBattleAuthoringDataToCurrentTarget();
    string Get_CurrentBattlePresetAuthoringOverrideId() const;
    string Get_CurrentBattleClipAuthoringOverrideId() const;
    void Add_PreviewShotBlockFromSelectedPhase();
    void Add_PreviewShotBlockAtCurrentTime();
    void Remove_SelectedPreviewShotBlock();
    void Duplicate_SelectedPreviewShotBlock();
    void Fit_SelectedPreviewShotBlockToActivePhase();
    void Select_PreviewShotBlock(int blockIndex, bool syncPreviewTime = false);
    float Snap_PreviewShotTime(float timeSec, float timelineDurationSec) const;
    Shared<GameObject> Resolve_PreviewBattleActor(PreviewSlotSide side);

    Shared<Client::BattleTurnCamera> Get_BattleTurnCamera() const;
    Shared<ModelCom> Resolve_PreviewSubjectModel() const;
    string Get_SelectedSkillId() const;
    const char* Get_SlotSideLabel(PreviewSlotSide side) const;
    const PreviewSlot* Get_PreviewSlot(PreviewSlotSide side, int slotIndex) const;
    Vec3 Get_PreviewSlotWorldPosition(PreviewSlotSide side, int slotIndex) const;
    string Get_SelectedHeroPrefabName() const;
    string Get_SelectedEnemyPrefabName() const;
    vector<Client::BattleMonsterSpawnDesc> Resolve_SelectedEnemySpawnDescs() const;
    BattleCameraMode Get_CurrentBattleAuthoringMode() const;
    string Get_CurrentPreviewPresetId() const;
    string Get_CurrentPreviewClipId() const;
    BattleCameraMode Get_BattlePresetEditorMode() const;
    CameraTrack* Get_SelectedBattleClipTrack();
    CameraKeyframe* Get_SelectedBattleClipKeyframe();
    float Get_TransitionSequenceDuration() const;
    float Get_DefaultBattleClipValue(int trackIndex);
    void Resolve_TransitionSequenceTime(float time, int& outStepIndex, float& outStepLocalTime);
    void Scrub_TransitionSequence(float timeSec);

    Vec3 Resolve_PhaseGoalPosition(const PreviewContext& context, const Client::BattleActionPhase& phase);
    Vec3 Resolve_AnchorPosition(const PreviewContext& context, Client::BattleTargetAnchor anchor);

private:
    PreviewContext   _previewContext{};
    PreviewFormation _previewFormation{};
    vector<string>   _skillIds{};
    string _skillSearchText{};
    vector<string>   _enemyPrefabNames{};
    Client::HeroId   _selectedHeroId = Client::HeroId::Gustave;

    int _selectedEnemyPrefabIndex = 0;
    int _selectedSkillIndex = -1;
    bool _useSkillOverride = false;
    bool _autoPreview = false;
    bool _previewDistanceOffsetEnabled = true;
    bool _previewFovOffsetEnabled = true;
    bool _previewShakeEnabled = true;
    bool _previewSideOffsetEnabled = true;
    bool _previewPivotOffsetEnabled = true;
    bool _previewHeightOffsetEnabled = true;
    bool _previewAimOffsetEnabled = true;
    bool _previewLookHeightOffsetEnabled = true;
    bool _previewYawOffsetEnabled = true;
    bool _previewOrbitYawEnabled = true;
    bool _previewOrbitRadiusEnabled = true;
    bool _previewOrbitHeightEnabled = true;
    bool _previewOrbitLookHeightEnabled = true;
    bool _previewShakeFrequencyXEnabled = true;
    bool _previewShakeFrequencyYEnabled = true;
    bool _previewShakeDecayRateXEnabled = true;
    bool _previewShakeDecayRateEnabled = true;
    bool _previewShakeVerticalRatioEnabled = true;
    bool _previewShakeStrengthScaleEnabled = true;
    bool _previewEnabled = true;
    bool _previewDirty = false;
    bool _previewObjectsDirty = true;
    bool _previewObjectsBegunPlay = false;
    bool _animationListDirty = true;
    bool _previewSkillPhaseBlocksDirty = true;
    bool _timelinePlaying = false;
    float _topSectionHeight = 420.f;
    uint32 _previewWidth = 1;
    uint32 _previewHeight = 1;
    bool _isPreviewHovered = false;
    bool _isPreviewActive = false;
    bool _showPreviewGrid = true;
    Shared<RenderTarget> _previewRenderTarget{};
    Shared<Editor_Camera_Free> _previewCamera{};
    Shared<Client::SkyBoxFlow> _previewSkyBox{};
    vector<Shared<Client::SkyCloudCard>> _previewSkyCloudCards{};
    Shared<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> _previewGridBatch{};
    Shared<DirectX::BasicEffect> _previewGridEffect{};
    ComPtr<ID3D11InputLayout> _previewGridInputLayout{};
    ComPtr<ID3D11DepthStencilState> _previewGridDepthState{};
    ComPtr<ID3D11BlendState> _previewGridBlendState{};
    Shared<GameObject> _previewHeroObject{};
    Shared<GameObject> _previewEnemyObject{};
    vector<string> _previewAnimationNames{};
    string _previewAnimationSearchText{};
    int _selectedPreviewAnimationIndex = -1;
    vector<PreviewSkillPhaseBlock> _previewSkillPhaseBlocks{};
    int _selectedPreviewSkillPhaseIndex = -1;
    vector<PreviewCameraShotBlock> _previewShotBlocks{};
    int _selectedPreviewShotBlockIndex = -1;
    bool _useManualBattleAuthoringTarget = false;
    string _manualBattleAuthoringPresetId{};
    string _manualBattleAuthoringClipId{};
    bool _useSkillShotTimeline = true;
    bool _previewAttackMovementEnabled = false;
    PreviewShotDragMode _previewShotDragMode = PreviewShotDragMode::None;
    int _previewShotDragBlockIndex = -1;
    float _previewShotDragGrabOffsetSec = 0.f;
    float _previewShotDragOriginalStartSec = 0.f;
    float _previewShotDragOriginalEndSec = 0.f;
    BattleCameraPreset _editingBattlePreset{};
    BattleCameraDriftProfile _editingBattleDrift{};
    bool _battlePresetDirty = false;
    bool _battleDriftDirty = false;
    BattleCameraMode _loadedBattlePresetMode = BattleCameraMode::PLAYERATTACK;
    string _loadedBattlePresetSkillId{};
    bool _hasLoadedBattlePreset = false;
    BattleCameraMode _loadedBattleDriftMode = BattleCameraMode::PLAYERATTACK;
    bool _hasLoadedBattleDrift = false;
    BattleCameraMode _loadedBattleClipMode = BattleCameraMode::PLAYERATTACK;
    string _loadedBattleClipSkillId{};
    BattleCameraClip _editingBattleClip{ 1.f };
    bool _battleClipDirty = false;
    bool _hasLoadedBattleClip = false;
    int _battleClipSelectedTrackIndex = 0;
    int _battleClipSelectedKeyIndex = -1;
    int _battleClipCurrentFrame = 0;
    float _battleClipPlaybackFrameAccumulator = 0.f;
    bool _requestOpenBattleClipInsertPopup = false;
    bool _requestRemoveBattleClipKeyPopup = false;
    int _battleClipContextTrackIndex = -1;
    int _battleClipContextFrame = 0;
    vector<fs::path> _authoringFilePaths{};
    string _authoringFileSearchText{};
    int _selectedAuthoringFileIndex = -1;
    Vec3 _previewSmoothedTargetPos = Vec3::Zero;
    bool _previewHasSmoothedTarget = false;
    BattleCameraMode _previewPoseMode = BattleCameraMode::PLAYERATTACK;
    PreviewSlotSide _previewPoseSubjectSide = PreviewSlotSide::Hero;
    int _previewPoseSubjectSlot = 1;
    float _previewPoseLastTimeSec = 0.f;
    BattleCameraMode _activePreviewShotMode = BattleCameraMode::PLAYERATTACK;
    string _activePreviewShotPresetId{};
    string _activePreviewShotClipId{};
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
    TimelineEditorTab _activeTimelineEditorTab = TimelineEditorTab::ClipTimeline;
    bool _requestOpenUnsavedSkillChangePopup = false;
    bool _pendingUseSkillOverride = false;
    int _pendingSelectedSkillIndex = -1;

public:
    static Shared<BattleCameraPreview_View> Create();
};

NS_END
