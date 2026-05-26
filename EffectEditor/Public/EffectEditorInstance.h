#pragma once

#include "Base.h"
#include "Engine_Enum.h"
#include "Shader_CBuffer_Define.h"

namespace DirectX::DX11
{
class BasicEffect;
template <typename TVertex>
class PrimitiveBatch;
struct VertexPositionColor;
}

NS_BEGIN(Engine)
class RenderTarget;
class ICommand;
class GameObject;
NS_END

NS_BEGIN(EffectEditor)

class Editor_Window;
class EffectEditor_Manager;
class EffectEditorCamera;
class ImGui_Manager;
class Editor_Context;
class CommandHistory;
class Notification_Manager;

struct EffectEditorRenderPerfSample
{
    double editorWindowsMs{};
    double notificationMs{};
    double imguiMs{};
};

struct EffectEditorPostProcessOptions
{
    PostProcessFeature features{ PostProcessFeature::None };
    PostProcessCB params{};
};

class EffectEditorInstance final : public Base
{
    DECLARE_SINGLETON(EffectEditorInstance)

public:
    EffectEditorInstance();
    ~EffectEditorInstance() override;

public:
    HRESULT Initialize_Editor(const EffectEditorDesc& editorDesc, const ComPtr<Device>& device, const ComPtr<Context>& context);
    void Release_Editor();

    void Update_Editor(float timeDelta);
    void Render_Editor(EffectEditorRenderPerfSample* outPerfSample = nullptr);

public: //## Accessor::Graphics
    ComPtr<Device> Get_Device() const { return _device; }
    ComPtr<Context> Get_Context() const { return _context; }

public: //## Accessor::Window
    Shared<Editor_Window> Get_Window(const wstring& windowName) const;
    HWND Get_WindowHandle() const;
    Editor_Context* Get_EditorContext() const { return _editorContext.get(); }

public: //## Accessor::Notification
    Notification_Manager* Get_Notification() { return _notificationManager.get(); }

public: //## Accessor::Camera
    Shared<EffectEditorCamera> Get_PreviewCamera() const { return _previewCamera; }

public: //## Accessor::Runtime
    bool IsPlaying() const;
    bool IsPaused() const;
    float Get_RuntimeTimeScale() const;

public: //## Accessor::PreviewOption
    bool Is_TrailPreviewVisible() const { return _isTrailPreviewVisible; }
    // Trail preview anchor marker/line 표시 여부다.
    bool Is_TrailPreviewDebugRenderEnabled() const { return _isTrailPreviewDebugRenderEnabled; }
    // Trail preview weapon-local base marker의 editor-session offset이다.
    Vec3 Get_TrailPreviewBaseLocalOffset() const { return _trailPreviewBaseLocalOffset; }
    // Trail preview weapon-local tip marker의 editor-session offset이다.
    Vec3 Get_TrailPreviewTipLocalOffset() const { return _trailPreviewTipLocalOffset; }
    // Trail preview base offset reset 기준값이다.
    Vec3 Get_DefaultTrailPreviewBaseLocalOffset() const { return kDefaultTrailPreviewBaseLocalOffset; }
    // Trail preview tip offset reset 기준값이다.
    Vec3 Get_DefaultTrailPreviewTipLocalOffset() const { return kDefaultTrailPreviewTipLocalOffset; }
    const EffectEditorPostProcessOptions& Get_PostProcessOptions() const { return _postProcessOptions; }
    EffectEditorPostProcessOptions Get_DefaultPostProcessOptions() const;

public: //## Accessor::InputPolicy
    bool Is_PreviewCameraControlEnabled() const { return _isPreviewCameraControlEnabled; }
    bool Is_PreviewCameraDragging() const { return _isPreviewCameraDragging; }

public: //## Accessor::Viewport
    ID3D11ShaderResourceView* Get_SharedViewportSRV() const;
    PreviewViewportMode Get_PreviewViewportMode() const { return _previewViewportMode; }
    bool Is_ManualPreviewViewportEnabled() const { return _previewViewportMode == PreviewViewportMode::Manual; }
    float Get_EffectiveViewportAspect() const;

public: //## Behavior::RuntimeControl
    void Play();
    void Pause();
    void Stop();
    void Resume();

    void Set_RuntimeTimeScale(float timeScale);
    void Reset_RuntimeTimeScale();

    void Request_FrameStep();
    bool Consume_FrameStep();
    void Request_RestartPreview();

public: //## Behavior::PreviewOption
    void Set_TrailPreviewVisible(bool visible);
    // Trail preview anchor marker/line 표시 여부만 바꾼다.
    void Set_TrailPreviewDebugRenderEnabled(bool enabled);
    // Detail_View의 Trail Preview 조정값을 현재 editor session에만 반영한다.
    void Set_TrailPreviewLocalOffsets(const Vec3& baseLocalOffset, const Vec3& tipLocalOffset);
    // Trail Preview 조정값을 기본 weapon-local marker 위치로 되돌린다.
    void Reset_TrailPreviewLocalOffsets();
    void Set_PostProcessFeature(PostProcessFeature feature, bool enabled);
    void Set_PostProcessParams(const PostProcessCB& params);

public: //## Behavior::Window
    void Request_WindowFocus(EditorViewportTarget target);
    bool Consume_WindowFocusRequest(EditorViewportTarget target);

public: //## Behavior::Command
    void Execute_Command(const Shared<ICommand>& command);
    void Undo();
    void Redo();
    void Clear_CommandHistory();

public: //## Behavior::Selection
    void Copy_SelectedObject();
    Shared<GameObject> Paste_CopiedObject();
    Shared<GameObject> Duplicate_SelectedObject();
    bool Delete_SelectedObject();

public: //## Behavior::CameraControl
    void Activate_TargetCamera();
    void Activate_PreviewCamera();
    void Register_PreviewCamera(const Shared<EffectEditorCamera>& previewCamera);
    bool Is_PreviewCameraActive() const;

public: //## Behavior::InputPolicy
    // scene viewport가 preview camera 입력을 받아도 되는지와 overlay 차단 상태를 함께 반영한다.
    void Set_SceneViewportInputState(bool canInteract, bool blocked);
    // game viewport가 runtime 입력을 받을 수 있는 상태만 반영한다.
    void Set_GameViewportInputState(bool canInteract);
    void Set_PreviewCameraLockScreenRect(const RECT& lockRectScreen);
    void Clear_PreviewCameraLockScreenRect();
    void Begin_PreviewCameraDrag();
    void End_PreviewCameraDrag();

    // 현재 scene/game viewport 상태를 보고 editor/runtime 입력 정책을 한 번에 동기화한다.
    void Apply_RuntimeInputPolicy();

public: //## Behavior::Viewport
    // Scene_View가 공유 preview SRV를 그리기 전에 target 크기를 맞출 때 사용한다.
    Shared<RenderTarget> Prepare_SharedViewportRenderTarget(uint32 width, uint32 height);
    // runtime back buffer 결과를 editor 공유 viewport target으로 복사한다.
    void Capture_BackBuffer_ToViewport();
    // EffectEditor scene grid를 현재 바인딩된 world render target에 그린다.
    void Render_SceneGrid();

    void Request_PreviewViewportSize(uint32 width, uint32 height);

    void Enable_AutoPreviewViewport();
    void Enable_ManualPreviewViewport();
    void Set_ManualPreviewViewportSize(uint32 width, uint32 height);
    // queued viewport 요청과 현재 모드를 맞춰 preview target 적용 상태를 동기화한다.
    void Sync_PreviewViewportMode();

private: //## Static Values
    static constexpr uint32 kDefaultPreviewViewportWidth{ 900 };
    static constexpr uint32 kDefaultPreviewViewportHeight{ 900 };
    static constexpr int kSceneGridDivisions{ 40 };
    static constexpr float kSceneGridHalfExtent{ 30.f };
    static constexpr float kSceneGridHeight{ 0.f };
    inline static const Vec3 kDefaultTrailPreviewBaseLocalOffset{ 0.f, 1.f, 20.f };
    inline static const Vec3 kDefaultTrailPreviewTipLocalOffset{ 0.f, 1.f, 85.f };

private: //## Data::Core
    ComPtr<Device> _device{};
    ComPtr<Context> _context{};
    EffectEditorDesc _editorDesc{};

    Unique<EffectEditor_Manager> _editorManager{};
    Unique<ImGui_Manager> _imguiManager{};
    Unique<Editor_Context> _editorContext{};
    Unique<CommandHistory> _commandHistory{};
    Unique<Notification_Manager> _notificationManager{};

private: //## Data::Viewport
    Shared<RenderTarget> _sharedViewportRenderTarget{ nullptr };
    Shared<PrimitiveBatch<VertexPositionColor>> _sceneGridBatch{ nullptr };
    Shared<BasicEffect> _sceneGridEffect{ nullptr };
    ComPtr<ID3D11InputLayout> _sceneGridInputLayout{};
    ComPtr<ID3D11DepthStencilState> _sceneGridDepthState{};

    PreviewViewportMode _previewViewportMode{ PreviewViewportMode::Auto };
    bool _hasPendingPreviewViewportSize{ false };
    uint32 _pendingPreviewViewportWidth{ kDefaultPreviewViewportWidth };
    uint32 _pendingPreviewViewportHeight{ kDefaultPreviewViewportHeight };
    uint32 _appliedPreviewViewportWidth{ 0 };
    uint32 _appliedPreviewViewportHeight{ 0 };

private: //## Data::Runtime
    EffectEditorRuntimeState _runtimeState{ EffectEditorRuntimeState::Edit };
    float _runtimeTimeScale{ 1.f };
    bool _pendingSingleFrameStep{ false };
    string _copiedObjectJson{};
    wstring _copiedLayerTag{};

private: //## Data::PreviewOption
    bool _isTrailPreviewVisible{ false };
    bool _isTrailPreviewDebugRenderEnabled{ true };
    Vec3 _trailPreviewBaseLocalOffset{ kDefaultTrailPreviewBaseLocalOffset };
    Vec3 _trailPreviewTipLocalOffset{ kDefaultTrailPreviewTipLocalOffset };
    EffectEditorPostProcessOptions _postProcessOptions{};

private: //## Data::Window
    bool _hasPendingWindowFocus{ false };
    EditorViewportTarget _pendingWindowFocus{ EditorViewportTarget::Game };

private: //## Data::CameraControl
    Shared<EffectEditorCamera> _previewCamera{ nullptr };

private: //## Data::InputPolicy
    bool _isSceneViewportInputEnabled{ false };
    bool _isSceneViewportInputBlocked{ false };
    bool _isGameViewportInputEnabled{ false };
    bool _isPreviewCameraControlEnabled{ false };
    bool _isPreviewCameraDragging{ false };
    bool _hasPreviewCameraLockScreenRect{ false };
    RECT _previewCameraLockScreenRect{};

private: //## Helper::PreviewViewport
    void Ensure_SceneGridResources();
    void Queue_PreviewViewportSize(uint32 width, uint32 height);
    void Apply_PendingPreviewViewportSize();

private: //## Helper::Selection
    bool Find_ObjectLayerTag(const Shared<GameObject>& gameObject, wstring& outLayerTag) const;
    Shared<GameObject> Instantiate_GameObjectFromCopiedData() const;

public:
    void Free() override;
};

NS_END
