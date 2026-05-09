#pragma once

#include "Editor_Window.h"
#include "AnimNotify_Types.h"
#include "Editor_Camera_Free.h"

NS_BEGIN(Engine)
class ModelCom;
class GameObject;
class RenderTarget;
NS_END

NS_BEGIN(MainEditor)

class AnimSequencerAdapter;
class Animation_View final : public Editor_Window
{
    friend class AnimSequencerAdapter;

public:
    enum class SourceMode : uint8
    {
        None,
        AssetOnly,
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

public:
    const string& Get_ModelGuid() const { return _modelGuid; }
    const string& Get_CurrentAnimationName() const { return _currentAnimationName; }

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

private:
    void Refresh_Source();
    void Refresh_AssetAnimationList();
    void Refresh_LiveAnimationList();

    void Load_NotifyAsset();
    void Save_NotifyAsset();

    void Draw_Header();
    void Draw_AnimationSelector();
    void Draw_PlaybackToolbar();
    void Draw_PreviewPanel();
    void Draw_Sequencer();
    void Draw_SelectedDetail();
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

    void Begin_SequencerContextMenu(int32 frame, bool hasTrackContext, bool isStateTrack, int32 trackIndex);
    bool Try_GetSequencerTrackContext(const ImVec2& sequencerRectMin, const ImVec2& mousePos, bool& outIsStateTrack, int32& outTrackIndex) const;

    Shared<ModelCom> Get_PlaybackModel() const;
    void Ensure_PreviewObject();
    void Ensure_PreviewRenderTarget(uint32 width, uint32 height);
    void Ensure_PreviewCamera();
    void Ensure_PreviewGridResources();
    Shared<GameObject> Get_PreviewRenderObject() const;
    void Fit_PreviewCameraToModel();
    void Draw_PreviewGrid() const;
    void Sync_PreviewAnimationSelection(bool keepCurrentTime = false);

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
    static bool Is_PrefabViewFocused();
    static Editor_Camera_Free::EDITOR_CAMERA_DESC Build_PreviewCameraDesc(const Shared<GameObject>& gameObject);
    static void Apply_PreviewCameraTransform(const Shared<Editor_Camera_Free>& camera);
    static void Remove_NotifyTrackEntries(AnimNotifyData& animationData, int32 trackIndex);
    static void Remove_NotifyStateTrackEntries(AnimNotifyData& animationData, int32 trackIndex);

private:
    SourceMode _sourceMode = SourceMode::None;

    wstring _sourceModelPath{};
    Shared<ModelCom> _liveModelCom{};
    Shared<GameObject> _previewObject{};
    Shared<ModelCom> _previewModelCom{};
    Shared<RenderTarget> _previewRenderTarget{};
    Shared<Editor_Camera_Free> _previewCamera{};
    Shared<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> _previewGridBatch{};
    Shared<DirectX::BasicEffect> _previewGridEffect{};
    ComPtr<ID3D11InputLayout> _previewGridInputLayout{};
    ComPtr<ID3D11DepthStencilState> _previewGridDepthState{};

    string _modelGuid{};
    string _currentAnimationName{};
    vector<AnimationEntry> _availableAnimations{};

    AnimNotifyAsset _asset{};
    bool _isDirty = false;

    int32 _selectedNotifyId = -1;
    int32 _selectedStateId = -1;
    int32 _selectedNotifyTrackIndex = 0;
    int32 _selectedNotifyStateTrackIndex = 0;

    int32 _sequencerCurrentFrame = 0;
    int32 _sequencerSelectedRow = -1;
    int32 _sequencerFirstFrame = 0;
    uint32 _pendingDockId = 0;
    bool _sequencerExpanded = true;

    bool _requestFocusOnOpen = false;
    bool _requestAutoPlayOnOpen = false;

    bool _previewHasBegunPlay = false; // 프리뷰 시작했는지
    bool _isPreviewHovered = false;
    bool _isPreviewActive = false;
    bool _showPreviewGrid = true;
    float _topSectionHeight = 500.f;
    
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

    Unique<AnimSequencerAdapter> _sequencer{};

public:
    static Shared<Animation_View> Create();
};

NS_END
