#pragma once

#include "Editor_Window.h"
#include "AnimNotify_Types.h"

NS_BEGIN(Engine)
class ModelCom;
class GameObject;
class RenderTarget;
NS_END

NS_BEGIN(MainEditor)

class AnimSequencerAdapter;
class Editor_Camera_Free;

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
    void Open_FromModelComponent(const Shared<ModelCom>& modelCom, const string& preferredAnimationName = "", uint32 preferredDockId = 0);

public:
    const string& Get_ModelGuid() const { return _modelGuid; }
    const string& Get_CurrentAnimationName() const { return _currentAnimationName; }

    int32 Get_CurrentAnimationFps() const;
    float Get_CurrentAnimationLengthSec() const;

    void MarkDirty() { _isDirty = true; }

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

    void Add_Notify_AtCurrentTime();
    void Add_State_AtCurrentTime();
    void Delete_Selected();

    Shared<ModelCom> Get_PlaybackModel() const;
    void Ensure_PreviewObject();
    void Ensure_PreviewRenderTarget(uint32 width, uint32 height);
    void Ensure_PreviewGridResources();
    void Ensure_PreviewCamera();
    Shared<GameObject> Get_PreviewRenderObject() const;
    void Fit_PreviewCameraToModel();
    void Draw_PreviewGrid() const;
    void Sync_PreviewAnimationSelection(bool keepCurrentTime = false);

    AnimNotifyData* Get_CurrentAnimation();
    const AnimNotifyData* Get_CurrentAnimation() const;

    AnimNotifyEventEntry* Find_SelectedNotify();
    AnimNotifyStateEntry* Find_SelectedState();

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
    vector<string> _availableAnimationNames{};

    AnimNotifyAsset _asset{};
    bool _isDirty = false;

    int32 _selectedNotifyId = -1;
    int32 _selectedStateId = -1;

    int32 _sequencerCurrentFrame = 0;
    bool _sequencerExpanded = true;
    int32 _sequencerSelectedRow = -1;
    int32 _sequencerFirstFrame = 0;
    uint32 _pendingDockId = 0;
    bool _requestFocusOnOpen = false;
    bool _requestAutoPlayOnOpen = false;
    bool _previewHasBegunPlay = false;
    bool _isPreviewHovered = false;
    bool _isPreviewActive = false;
    bool _showPreviewGrid = true;
    uint32 _previewWidth = 640;
    uint32 _previewHeight = 360;

    Unique<AnimSequencerAdapter> _sequencer{};

public:
    static Shared<Animation_View> Create();
};

NS_END
