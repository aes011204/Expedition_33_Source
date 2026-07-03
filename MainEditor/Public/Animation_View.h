#pragma once

#include "EffectRuntime_Types.h"
#include "Editor_Window.h"
#include "AnimNotify_Types.h"
#include "Editor_Camera_Free.h"
#include "ANS_Trail.h"
#include "TrailAnchor_Asset.h"
#include <unordered_set>

NS_BEGIN(Engine)
class ModelCom;
class GameObject;
class RenderTarget;
class EffectInstance;
NS_END

NS_BEGIN(Client)
class ANS_Trail;
class SkyBoxFlow;
class SkyCloudCard;
class Weapon;
NS_END

NS_BEGIN(MainEditor)

class AnimationNotifyEditor;
class AnimSequencerAdapter;
class Animation_View final : public Editor_Window, public Engine::IEffectTrailSampleProvider, public Engine::IEffectSourcePointSampleProvider
{
    friend class AnimationNotifyEditor;
    friend class AnimSequencerAdapter;

public:
    enum class SourceMode : uint8
    {
        None,
        LiveModel
    };

public:
    Animation_View();
    ~Animation_View() override;

public:
    void Update(float timeDelta) override;
    void Pre_Render() override;
    void Render() override;
    bool CanSave() const override;
    void Save() override;

public:
    // Content Browser에서 .model / .anim 기준으로 열 때
    void Open_FromAsset(const wstring& modelPath, const string& modelGuid, const string& preferredAnimationName = "", uint32 preferredDockId = 0);

    // Prefab / Scene의 실제 ModelCom를 대상으로 열 때
    void Open_FromModelComponent(const Shared<ModelCom>& modelCom, const string& preferredAnimationName = "", uint32 preferredDockId = 0, bool focusOnOpen = true);
    void Clear_LivePreviewBinding(const Shared<GameObject>& previewRoot = nullptr);
    void Refresh_LivePreviewBinding(const Shared<GameObject>& previewRoot = nullptr);

public:
    const string& Get_ModelGuid() const { return _modelGuid; }
    const string& Get_CurrentAnimationName() const { return _currentAnimationName; }
    bool Try_GetTrailSample(EffectTrailSample& outSample) const override;
    bool Try_GetSourcePointSample(EffectSourcePointSample& outSample) const override;
    void Refresh_PreviewTrailAnchorAsset();

    int32 Get_CurrentAnimationFps() const;
    float Get_CurrentAnimationLengthSec() const;
    int32 Get_CurrentAnimationFrameMax() const;

    void MarkDirty() { _isDirty = true; }

public:
    struct AnimationEntry
    {
        string name{};
        string displayName{};
        string playbackName{};
    };

    enum class NotifyClipboardKind : uint8
    {
        None,
        Notify,
        State
    };

    struct CopiedNotifyEntry
    {
        string typeName{};
        json payload{};
        float timeSec = 0.f;
        int32 trackIndex = 0;
    };

    struct CopiedNotifyStateEntry
    {
        string typeName{};
        json payload{};
        float startSec = 0.f;
        float durationSec = 0.f;
        int32 trackIndex = 0;
    };

    struct NotifyUndoSnapshot
    {
        json assetJson{};
        string currentAnimationName{};
        int32 selectedNotifyId = -1;
        int32 selectedStateId = -1;
        int32 selectedNotifyTrackIndex = 0;
        int32 selectedNotifyStateTrackIndex = 0;
        int32 sequencerCurrentFrame = 0;
        vector<int32> selectedNotifyIds{};
        vector<int32> selectedStateIds{};
    };

    struct LiveModelTarget
    {
        wstring objectKey{};
        wstring componentTag{};
        string label{};
        wstring sourceModelPath{};
        Shared<ModelCom> model{};
    };

private:
    enum class TrailAnchorSlot : uint8
    {
        Base,
        Tip
    };

    enum class AnimationLeftTab : uint8
    {
        AnimationClips,
        BoneTree
    };

    enum class NotifyTrailBonePickMode : uint8
    {
        None,
        Base,
        Tip,
        PairBase,
        PairTip
    };

    struct PreviewTrailRuntime
    {
        int32 stateId = -1;
        string partTag = "Part_Weapon";
        string modelGuid{};
        float lastTimeSec = -1.f;
        float offsetPhase = 0.f;
        Client::ANS_Trail::TrailAnchorSource trailAnchorSource = Client::ANS_Trail::TrailAnchorSource::ModelDefault;
        string baseBoneName{};
        Vec3 baseLocalOffset{ Vec3::Zero };
        string tipBoneName{};
        Vec3 tipLocalOffset{ Vec3::Zero };
        Client::ANS_Trail::TipExtendCurve tipExtendCurve{};
        Client::ANS_Trail::LocalOffsetCurve baseLocalOffsetCurve{};
        Client::ANS_Trail::LocalOffsetCurve tipLocalOffsetCurve{};

        Client::TrailAnchorAsset trailAnchorAsset{};
        Shared<EffectInstance> effect{};
        wstring effectName{};
        bool isPlaying = false;
        bool isDraining = false;
        float drainElapsed = 0.f;
        float drainDuration = 0.25f;
    };

    struct PreviewSourceHistoryRuntime
    {
        int32 stateId = -1;
        string partTag = "Part_Weapon";
        int32 sourceBindingMode = 0;
        string socketName{};
        string sourceHistoryAnchorName{ "Source" };
        Vec3 sourceHistoryLocalOffset{ Vec3::Zero };
        string modelGuid{};
        Client::TrailAnchorAsset trailAnchorAsset{};
        float lastTimeSec = -1.f;

        Shared<EffectInstance> effect{};
        wstring effectName{};
        bool isPlaying = false;
        bool isDraining = false;
        float drainElapsed = 0.f;
        float drainDuration = 0.5f;
    };

    struct PreviewWeaponRotateRuntime
    {
        int32 stateId = -1;
        Shared<AnimNotifyState> notifyState{};
        wstring partTag{};
    };

    struct PreviewWeaponEchoRuntime
    {
        int32 stateId = -1;
        Shared<AnimNotifyState> notifyState{};
        wstring partTag{};
        string payloadJson{};
    };

    struct PreviewPlayEffectStateRuntime
    {
        int32 stateId = -1;
        Shared<AnimNotifyState> notifyState{};
        string payloadJson{};
    };

    struct PreviewScreenFxCueRuntime
    {
        int32 stateId = -1;
        Shared<AnimNotifyState> notifyState{};
        string payloadJson{};
    };

    struct PreviewAnimSpeedStateRuntime
    {
        int32 stateId = -1;
        Shared<AnimNotifyState> notifyState{};
        string payloadJson{};
    };

    struct PreviewSLDodgeMoveRuntime
    {
        int32 stateId = -1;
        Shared<AnimNotifyState> notifyState{};
        string payloadJson{};
    };

    struct PreviewRootControllerRuntime
    {
        int32 stateId = -1;
        Shared<AnimNotifyState> notifyState{};
        string payloadJson{};
    };

    struct PreviewWorldTimeScaleState
    {
        float worldTimeScale = 1.f;
        bool scaleEffects = true;
    };

    struct PreviewSkillPhaseContext
    {
        bool enabled = false;
        string skillKey{};
        uint32 phaseIndex = 0;
        string animationKey{};
        float playRate = 1.f;
    };

private:
    void Refresh_Source();
    void Refresh_AssetAnimationList();
    void Refresh_LiveAnimationList();
    void Refresh_LiveModelTargets();
    void Draw_LiveModelTargetSelector();
    void Switch_LiveModelTarget(size_t targetIndex);
    Shared<ModelCom> Resolve_LiveModelTargetModel(const LiveModelTarget& target) const;
    Shared<ModelCom> Resolve_ModifierCollisionModel(const Shared<ModelCom>& modifierModel, const vector<string>& requiredBoneNames) const;
    void Sync_ModifierCollisionSource(const Shared<ModelCom>& modifierModel) const;
    void Rebuild_PreviewClone();
    void Reset_PreviewClone();

    void Load_NotifyAsset();
    void Save_NotifyAsset();
    void Load_TrailAnchorAsset();
    void Save_TrailAnchorAsset() const;
    void Refresh_OpenModelNotifyAssets() const;

    void Draw_Header();
    void Draw_AnimationSelector();
    void Draw_BoneTree();
    void Draw_PlaybackToolbar();
    void Draw_SkillPhasePreviewControls();
    void Draw_PreviewPanel();
    void Draw_Sequencer();
    void Draw_SelectedDetail();
    void Draw_NotifyTrailAnchorPanel(Client::ANS_Trail& trailNotifyState);
    void Draw_TrailOffsetPanel(Client::ANS_Trail& trailNotifyState, const AnimNotifyStateEntry& notifyStateEntry);
    void Draw_ModifierRuntimePanel(const Shared<ModelCom>& playbackModel);
    void Update_ModifierPreviewMotionTest(float timeDelta);
    void Draw_SelectedBoneDetail(const Shared<ModelCom>& playbackModel);
    void Draw_TrailAnchorPanel();
    void Draw_EventListPanel();
    void Draw_NotifyList();
    void Draw_StateList();
    void Draw_CreateNotifyPopup();
    void Draw_CreateNotifyStatePopup();
    void Draw_SequencerContextMenu();
    void Draw_TrackSection();
    float Get_PlaySectionStartSec(const AnimNotifyData* animationData) const;
    float Get_PlaySectionEndSec(const AnimNotifyData* animationData) const;
    void Clamp_PlaySection(AnimNotifyData* animationData);
    void Play_CurrentSection(const Shared<ModelCom>& playbackModel);
    void Stop_CurrentSection(const Shared<ModelCom>& playbackModel);

    void Add_NotifyTrack(const string& trackName);
    void Add_NotifyStateTrack(const string& trackName);
    void Add_Notify_AtFrame(int32 frame, const string& typeName);
    void Add_State_ByFrameRange(int32 startFrame, int32 endFrame, const string& typeName);

    void Delete_SelectedNotify();
    void Delete_SelectedState();
    void Delete_SelectedNotifyTrack();
    void Delete_SelectedNotifyStateTrack();
    void Handle_ClipboardShortcuts();
    void Undo_NotifyEdit();
    void Redo_NotifyEdit();
    NotifyUndoSnapshot Capture_NotifyUndoSnapshot() const;
    void Restore_NotifyUndoSnapshot(const NotifyUndoSnapshot& snapshot);
    void Commit_NotifyUndoSnapshot(const NotifyUndoSnapshot& snapshot);
    void Begin_PendingNotifyUndo();
    void Commit_PendingNotifyUndo();
    void Clear_NotifyUndoHistory();
    void Move_SelectedNotifiesByFrame(int32 frameDelta);
    void Move_SelectedStateByFrame(int32 frameDelta);
    void Copy_SelectedNotify();
    void Copy_SelectedState();
    void Paste_CopiedNotify();
    void Paste_CopiedState();
    void Clear_SelectedNotifySelection();
    void Clear_SelectedStateSelection();
    bool Is_NotifySelected(int32 notifyId) const;
    bool Is_StateSelected(int32 stateId) const;
    AnimNotifyStateEntry* Find_NotifyStateById(int32 stateId);
    Client::ANS_Trail* Find_NotifyTrailPickTarget(int32* outStateTrackIndex = nullptr);
    void Begin_NotifyTrailBonePick(NotifyTrailBonePickMode mode, int32 stateId);
    void Cancel_NotifyTrailBonePick();
    bool Is_NotifyTrailBonePicking() const { return _notifyTrailBonePickMode != NotifyTrailBonePickMode::None; }
    bool Consume_NotifyTrailBonePick(uint32 boneIndex);
    void Restore_NotifyTrailPickSelection();

    void Begin_SequencerContextMenu(int32 frame, bool hasTrackContext, bool isStateTrack, int32 trackIndex);
    bool Try_GetSequencerTrackContext(const ImVec2& sequencerRectMin, const ImVec2& mousePos, bool& outIsStateTrack, int32& outTrackIndex) const;

    Shared<ModelCom> Get_PlaybackModel() const;
    Shared<ModelCom> Get_ModifierRuntimeModel() const;
    void Ensure_PreviewRenderTarget(uint32 width, uint32 height);
    void Ensure_PreviewCamera();
    void Ensure_PreviewGridResources();
    void Ensure_PreviewSkyBackdrop();
    void Update_PreviewSkyBackdrop(float timeDelta);
    void Late_Update_PreviewSkyBackdrop(float timeDelta);
    Shared<GameObject> Get_PreviewRenderObject() const;
    void Fit_PreviewCameraToModel();
    void Draw_PreviewGrid() const;
    void Draw_SelectedBoneMarker() const;
    void Draw_ModifierChainDebug() const;
    void Draw_ModifierCollisionDebug() const;
    void Draw_TrailAnchorPreview() const;
    PreviewWorldTimeScaleState Resolve_PreviewWorldTimeScaleState(float currentTimeSec) const;
    void Update_PreviewAnimSpeedStates(float timeDelta);
    void Reset_PreviewAnimSpeedStates();
    void Clear_SkillPhasePreviewContext();
    bool Sync_SkillPhasePreviewContextForAnimation(const AnimationEntry& entry);
    bool Set_SkillPhasePreviewContext(const string& skillKey, uint32 phaseIndex);
    float Resolve_PreviewBaseFrameSpeed() const;
    void Apply_PreviewBaseFrameSpeed();
    void Update_PreviewPlayEffectNotifies(float previousTimeSec, float currentTimeSec, float timeDelta);
    void Reset_PreviewPlayEffectStates();
    void Update_PreviewScreenFxCueStates(float timeDelta);
    void Reset_PreviewScreenFxCueStates();
    void Update_PreviewWeaponRotateStates(float timeDelta);
    void Reset_PreviewWeaponRotateStates();
    void Update_PreviewWeaponEchoStates(float timeDelta);
    void Reset_PreviewWeaponEchoStates();
    void Update_PreviewSLDodgeMoveStates(float timeDelta);
    void Reset_PreviewSLDodgeMoveStates();
    void Update_PreviewRootControllerStates(float timeDelta);
    void Reset_PreviewRootControllerStates();
    void Update_PreviewTrailEffect(float timeDelta);
    void Reset_PreviewTrailEffect();
    void Begin_PreviewTrailDrain(PreviewTrailRuntime& runtime);
    bool Tick_PreviewTrailDrain(PreviewTrailRuntime& runtime, float timeDelta);
    vector<const AnimNotifyStateEntry*> Find_ActivePreviewTrailStates() const;
    bool Ensure_PreviewTrailEffect(PreviewTrailRuntime& runtime, const wstring& effectName);
    void Update_PreviewSourceHistoryEffect(float timeDelta);
    void Reset_PreviewSourceHistoryEffect();
    void Begin_PreviewSourceHistoryDrain(PreviewSourceHistoryRuntime& runtime);
    bool Tick_PreviewSourceHistoryDrain(PreviewSourceHistoryRuntime& runtime, float timeDelta);
    vector<const AnimNotifyStateEntry*> Find_ActivePreviewSourceHistoryStates() const;
    bool Ensure_PreviewSourceHistoryEffect(PreviewSourceHistoryRuntime& runtime, const wstring& effectName);
    bool Try_ResolvePreviewTrailEffectName(
        const string& effectNameOverride,
        const string& partTag,
        wstring& outEffectName) const;
    void Sync_PreviewAnimationSelection(bool keepCurrentTime = false);
    void Validate_SelectedBoneSelection();
    void Assign_SelectedBoneToTrailAnchor(const Shared<ModelCom>& playbackModel, TrailAnchorSlot slot);
    bool Try_ResolveTrailAnchorWorldPosition(
        const Shared<ModelCom>& playbackModel,
        const Client::TrailAnchorPointDesc& anchor,
        Vec3& outWorldPosition) const;
    bool Try_GetTrailSampleForRuntime(
        const PreviewTrailRuntime& runtime,
        EffectTrailSample& outSample) const;
    bool Try_GetSourcePointSampleForRuntime(
        const PreviewSourceHistoryRuntime& runtime,
        EffectSourcePointSample& outSample) const;
    bool Try_ResolvePreviewTrailAnchorWorldPosition(
        const string& partTag,
        const Client::TrailAnchorPointDesc& anchor,
        Vec3& outWorldPosition) const;
    bool Try_ResolvePreviewWeaponAnchorWorldPosition(
        const string& partTag,
        const string& anchorName,
        const Vec3& localOffset,
        Vec3& outWorldPosition) const;
    bool Try_ResolvePreviewSocketWorldPosition(
        const string& partTag,
        const string& socketName,
        const Vec3& localOffset,
        Vec3& outWorldPosition) const;
    bool Load_PreviewTrailAnchorAsset(
        const string& partTag,
        PreviewTrailRuntime& outRuntime);
    Weapon* Resolve_PreviewWeapon(const wstring& partTag) const;

    AnimNotifyData* Get_CurrentAnimation();
    const AnimNotifyData* Get_CurrentAnimation() const;

    AnimNotifyEventEntry* Find_SelectedNotify();
    AnimNotifyStateEntry* Find_SelectedState();

private:
    static string Normalize_DefaultTrackName(const string& trackName);
    static string Make_AnimationComboLabel(const AnimationEntry& entry);
    static const AnimationEntry* Find_AnimationEntryByAnyName(const vector<AnimationEntry>& entries, const string& name);
    static int32 Find_AnimationIndex(const Shared<ModelCom>& modelCom, const string& animationName);
    static float Resolve_AnimNotifyDataLengthSec(const AnimNotifyData* animationData);
    static Shared<GameObject> Resolve_RootPreviewObject(const Shared<GameObject>& gameObject);
    static void Lock_PreviewObjectToOrigin(const Shared<GameObject>& gameObject);
    static bool Should_DeferToCompanionPreview();
    static Editor_Camera_Free::EDITOR_CAMERA_DESC Build_PreviewCameraDesc(const Shared<GameObject>& gameObject);
    static void Apply_PreviewCameraTransform(const Shared<Editor_Camera_Free>& camera);
    static void Remove_NotifyTrackEntries(AnimNotifyData& animationData, int32 trackIndex);
    static void Remove_NotifyStateTrackEntries(AnimNotifyData& animationData, int32 trackIndex);

private:
    SourceMode _sourceMode = SourceMode::None;

    wstring _sourceModelPath{};
    Shared<ModelCom> _liveModelCom{};
    vector<LiveModelTarget> _liveModelTargets{};
    int32 _selectedLiveModelTargetIndex = -1;
    Shared<GameObject> _previewRootObject{};
    Shared<ModelCom> _previewModelCom{};
    Shared<RenderTarget> _previewRenderTarget{};
    Shared<Editor_Camera_Free> _previewCamera{};
    Shared<Client::SkyBoxFlow> _previewSkyBox{};
    vector<Shared<Client::SkyCloudCard>> _previewSkyCloudCards{};
    Shared<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> _previewGridBatch{};
    Shared<DirectX::BasicEffect> _previewGridEffect{};
    ComPtr<ID3D11InputLayout> _previewGridInputLayout{};
    ComPtr<ID3D11DepthStencilState> _previewGridDepthState{};
    ComPtr<ID3D11DepthStencilState> _previewOverlayDepthState{};

    string _modelGuid{};
    string _currentAnimationName{};
    PreviewSkillPhaseContext _skillPhaseContext{};
    vector<AnimationEntry> _availableAnimations{};
    char _animationSearchBuffer[128] = "";
    char _boneSearchBuffer[128] = "";

    AnimNotifyAsset _asset{};
    Client::TrailAnchorAsset _trailAnchorAsset{};
    AnimationLeftTab _animationLeftTab = AnimationLeftTab::AnimationClips;
    AnimationLeftTab _requestedAnimationLeftTab = AnimationLeftTab::AnimationClips;
    bool _hasRequestedAnimationLeftTab = false;
    AnimationLeftTab _notifyTrailBonePickReturnTab = AnimationLeftTab::AnimationClips;
    NotifyTrailBonePickMode _notifyTrailBonePickMode = NotifyTrailBonePickMode::None;
    int32 _notifyTrailBonePickStateId = -1;
    vector<PreviewPlayEffectStateRuntime> _previewPlayEffectStateRuntimes{};
    vector<PreviewScreenFxCueRuntime> _previewScreenFxCueRuntimes{};
    vector<PreviewAnimSpeedStateRuntime> _previewAnimSpeedStateRuntimes{};
    vector<PreviewWeaponRotateRuntime> _previewWeaponRotateRuntimes{};
    vector<PreviewWeaponEchoRuntime> _previewWeaponEchoRuntimes{};
    vector<PreviewSLDodgeMoveRuntime> _previewSLDodgeMoveRuntimes{};
    vector<PreviewRootControllerRuntime> _previewRootControllerRuntimes{};
    vector<PreviewTrailRuntime> _previewTrailRuntimes{};
    vector<PreviewSourceHistoryRuntime> _previewSourceHistoryRuntimes{};
    mutable int32 _previewTrailProviderStateId = -1; // preview effect가 지금 sample을 요청 중인 runtime state id다.
    mutable int32 _previewSourceHistoryProviderStateId = -1;
    bool _isDirty = false;
    int32 _selectedBoneIndex = -1;

    int32 _selectedNotifyId = -1;
    int32 _selectedStateId = -1;
    int32 _selectedNotifyTrackIndex = 0;
    int32 _selectedNotifyStateTrackIndex = 0;
    std::unordered_set<int32> _selectedNotifyIds{};
    std::unordered_set<int32> _selectedStateIds{};

    int32 _sequencerCurrentFrame = 0;
    int32 _sequencerSelectedRow = -1;
    int32 _sequencerFirstFrame = 0;
    int32 _sequencerVisibleFrameCount = 0;
    uint32 _pendingDockId = 0;
    bool _sequencerExpanded = true;

    bool _requestFocusOnOpen = false;
    bool _isPreviewHovered = false;
    bool _isPreviewActive = false;
    bool _showPreviewGrid = true;
    bool _showPreviewSkyBackdrop = true;
    bool _previewSoundEnabled = true;
    bool _previewPlayEffectIncludeStartEdge = false;
    bool _previewSkyBackdropBegunPlay = false;
    bool _isSectionPreviewPlaying = false;
    bool _showModifierChainDebug = false;
    wstring _modifierChainDebugTag{};
    float _topSectionHeight = 500.f;
    wstring _previewOwnerObjectKey{};
    bool _modifierPreviewMotionTestEnabled = false;
    bool _modifierPreviewMotionTestOscillate = true;
    wstring _modifierPreviewMotionTestTag{};
    Vec3 _modifierPreviewMotionTestDirection = Vec3(1.f, 0.f, 0.f);
    float _modifierPreviewMotionTestDelta = 0.04f;
    float _modifierPreviewMotionTestFrequency = 2.f;
    int32 _modifierPreviewMotionTestAppliedCount = 0;
    Vec3 _modifierPreviewMotionTestPrevOffset = Vec3::Zero;
    bool _modifierPreviewMotionTestHasPrevOffset = false;

    // 사이즈 조절
    uint32 _previewWidth = 640;
    uint32 _previewHeight = 360;

    // 우클릭 -> 생성
    bool _openCreateNotifyPopup = false;
    bool _openCreateNotifyStatePopup = false;
    bool _openSequencerContextPopup = false;
    char _createNotifyFilter[128]{};
    char _createNotifyStateFilter[128]{};

    int32 _requestedCreateNotifyFrame = 0;
    int32 _requestedCreateStateStartFrame = 0;
    int32 _requestedCreateStateEndFrame = 0;

    bool _sequencerMarkingState = false;
    int32 _sequencerMarkStartFrame = -1;
    int32 _sequencerMarkEndFrame = -1;

    int32 _sequencerContextFrame = 0;
    int32 _sequencerContextTrackIndex = -1;
    bool _sequencerContextHasTrack = false;
    bool _sequencerContextIsStateTrack = false;

    // 트랙
    string _newNotifyTrackName{};
    string _newNotifyStateTrackName{};

    NotifyClipboardKind _notifyClipboardKind = NotifyClipboardKind::None;
    vector<CopiedNotifyEntry> _copiedNotifies{};
    vector<CopiedNotifyStateEntry> _copiedNotifyStates{};
    CopiedNotifyStateEntry _copiedNotifyState{};

    vector<NotifyUndoSnapshot> _notifyUndoStack{};
    vector<NotifyUndoSnapshot> _notifyRedoStack{};
    NotifyUndoSnapshot _pendingNotifyUndoSnapshot{};
    bool _hasPendingNotifyUndoSnapshot = false;

    Unique<AnimationNotifyEditor> _notifyEditor{};
    Unique<AnimSequencerAdapter> _sequencer{};

public:
    static Shared<Animation_View> Create();
};

NS_END
