#pragma once

#include "EffectRuntime_Types.h"
#include "Editor_Window.h"
#include "AnimNotify_Types.h"
#include "Editor_Camera_Free.h"
#include "TrailAnchor_Asset.h"
#include <unordered_set>

NS_BEGIN(Engine)
class ModelCom;
class GameObject;
class RenderTarget;
class EffectInstance;
NS_END

NS_BEGIN(Client)
class Weapon;
NS_END

NS_BEGIN(MainEditor)

class AnimSequencerAdapter;
class Animation_View final : public Editor_Window, public Engine::IEffectTrailSampleProvider
{
    friend class AnimSequencerAdapter;

public:
    enum class SourceMode : uint8
    {
        None,
        LiveModel
    };

public:
    Animation_View();
    ~Animation_View() override = default;

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

public:
    const string& Get_ModelGuid() const { return _modelGuid; }
    const string& Get_CurrentAnimationName() const { return _currentAnimationName; }
    bool Try_GetTrailSample(EffectTrailSample& outSample) const override;
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

private:
    enum class TrailAnchorSlot : uint8
    {
        Base,
        Tip
    };

    struct PreviewTrailRuntime
    {
        int32 stateId = -1;
        string partTag = "Part_Weapon";
        string modelGuid{};
        float lastTimeSec = -1.f;

        Client::TrailAnchorAsset trailAnchorAsset{};
        Shared<EffectInstance> effect{};
        wstring effectName{};
        bool isPlaying = false;
        bool isDraining = false;
        float drainElapsed = 0.f;
        float drainDuration = 0.25f;
    };

    struct PreviewWeaponRotateRuntime
    {
        int32 stateId = -1;
        Shared<AnimNotifyState> notifyState{};
        wstring partTag{};
    };

    struct PreviewPlayEffectStateRuntime
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

private:
    void Refresh_Source();
    void Refresh_AssetAnimationList();
    void Refresh_LiveAnimationList();
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
    void Draw_PreviewPanel();
    void Draw_Sequencer();
    void Draw_SelectedDetail();
    void Draw_SelectedBoneDetail(const Shared<ModelCom>& playbackModel);
    void Draw_TrailAnchorPanel();
    void Draw_EventListPanel();
    void Draw_NotifyList();
    void Draw_StateList();
    void Draw_CreateNotifyPopup();
    void Draw_CreateNotifyStatePopup();
    void Draw_SequencerContextMenu();
    void Draw_TrackSection();

    void Add_NotifyTrack(const string& trackName);
    void Add_NotifyStateTrack(const string& trackName);
    void Add_Notify_AtFrame(int32 frame, const string& typeName);
    void Add_State_ByFrameRange(int32 startFrame, int32 endFrame, const string& typeName);

    void Delete_SelectedNotify();
    void Delete_SelectedState();
    void Delete_SelectedNotifyTrack();
    void Delete_SelectedNotifyStateTrack();
    void Handle_ClipboardShortcuts();
    void Move_SelectedNotifiesByFrame(int32 frameDelta);
    void Copy_SelectedNotify();
    void Copy_SelectedState();
    void Paste_CopiedNotify();
    void Paste_CopiedState();
    void Clear_SelectedNotifySelection();
    bool Is_NotifySelected(int32 notifyId) const;

    void Begin_SequencerContextMenu(int32 frame, bool hasTrackContext, bool isStateTrack, int32 trackIndex);
    bool Try_GetSequencerTrackContext(const ImVec2& sequencerRectMin, const ImVec2& mousePos, bool& outIsStateTrack, int32& outTrackIndex) const;

    Shared<ModelCom> Get_PlaybackModel() const;
    void Ensure_PreviewRenderTarget(uint32 width, uint32 height);
    void Ensure_PreviewCamera();
    void Ensure_PreviewGridResources();
    Shared<GameObject> Get_PreviewRenderObject() const;
    void Fit_PreviewCameraToModel();
    void Draw_PreviewGrid() const;
    void Draw_SelectedBoneMarker() const;
    void Draw_TrailAnchorPreview() const;
    void Update_PreviewAnimSpeedStates(float timeDelta);
    void Reset_PreviewAnimSpeedStates();
    void Update_PreviewPlayEffectNotifies(float previousTimeSec, float currentTimeSec, float timeDelta);
    void Reset_PreviewPlayEffectStates();
    void Update_PreviewWeaponRotateStates(float timeDelta);
    void Reset_PreviewWeaponRotateStates();
    void Update_PreviewTrailEffect(float timeDelta);
    void Reset_PreviewTrailEffect();
    void Begin_PreviewTrailDrain(PreviewTrailRuntime& runtime);
    bool Tick_PreviewTrailDrain(PreviewTrailRuntime& runtime, float timeDelta);
    vector<const AnimNotifyStateEntry*> Find_ActivePreviewTrailStates() const;
    bool Ensure_PreviewTrailEffect(PreviewTrailRuntime& runtime, const wstring& effectName);
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
    bool Try_ResolvePreviewTrailAnchorWorldPosition(
        const string& partTag,
        const Client::TrailAnchorPointDesc& anchor,
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
    Shared<GameObject> _previewRootObject{};
    Shared<ModelCom> _previewModelCom{};
    Shared<RenderTarget> _previewRenderTarget{};
    Shared<Editor_Camera_Free> _previewCamera{};
    Shared<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> _previewGridBatch{};
    Shared<DirectX::BasicEffect> _previewGridEffect{};
    ComPtr<ID3D11InputLayout> _previewGridInputLayout{};
    ComPtr<ID3D11DepthStencilState> _previewGridDepthState{};
    ComPtr<ID3D11DepthStencilState> _previewOverlayDepthState{};

    string _modelGuid{};
    string _currentAnimationName{};
    vector<AnimationEntry> _availableAnimations{};
    char _animationSearchBuffer[128] = "";
    char _boneSearchBuffer[128] = "";

    AnimNotifyAsset _asset{};
    Client::TrailAnchorAsset _trailAnchorAsset{};
    vector<PreviewPlayEffectStateRuntime> _previewPlayEffectStateRuntimes{};
    vector<PreviewAnimSpeedStateRuntime> _previewAnimSpeedStateRuntimes{};
    vector<PreviewWeaponRotateRuntime> _previewWeaponRotateRuntimes{};
    vector<PreviewTrailRuntime> _previewTrailRuntimes{};
    mutable int32 _previewTrailProviderStateId = -1; // preview effect가 지금 sample을 요청 중인 runtime state id다.
    bool _isDirty = false;
    int32 _selectedBoneIndex = -1;

    int32 _selectedNotifyId = -1;
    int32 _selectedStateId = -1;
    int32 _selectedNotifyTrackIndex = 0;
    int32 _selectedNotifyStateTrackIndex = 0;
    std::unordered_set<int32> _selectedNotifyIds{};

    int32 _sequencerCurrentFrame = 0;
    int32 _sequencerSelectedRow = -1;
    int32 _sequencerFirstFrame = 0;
    uint32 _pendingDockId = 0;
    bool _sequencerExpanded = true;

    bool _requestFocusOnOpen = false;
    bool _isPreviewHovered = false;
    bool _isPreviewActive = false;
    bool _showPreviewGrid = true;
    float _topSectionHeight = 500.f;
    wstring _previewOwnerObjectKey{};
    
    // 사이즈 조절
    uint32 _previewWidth = 640;
    uint32 _previewHeight = 360;

    // 우클릭 -> 생성
    bool _openCreateNotifyPopup = false;
    bool _openCreateNotifyStatePopup = false;
    bool _openSequencerContextPopup = false;

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
    CopiedNotifyStateEntry _copiedNotifyState{};

    Unique<AnimSequencerAdapter> _sequencer{};

public:
    static Shared<Animation_View> Create();
};

NS_END
