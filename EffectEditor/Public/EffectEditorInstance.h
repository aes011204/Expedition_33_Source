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

enum class EffectEditorPostProcessPreset
{
    PreviewFriendly,
    ClientCurrent,
    Custom,
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
    bool Is_SceneFocusModeActive() const;
    uint32 Get_ActiveDockSpaceId() const;

public: //## Accessor::Notification
    Notification_Manager* Get_Notification() { return _notificationManager.get(); }

public: //## Accessor::Camera
    Shared<EffectEditorCamera> Get_PreviewCamera() const { return _previewCamera; }

public: //## Accessor::Runtime
    bool IsPlaying() const;
    bool IsPaused() const;
    float Get_RuntimeTimeScale() const;

public: //## Accessor::PreviewOption
    bool Is_AutoReplayPreviewEnabled() const { return _autoReplayPreviewEnabled; }
    bool Is_TrailPreviewVisible() const { return _isTrailPreviewVisible; }
    bool Is_PreviewPlaneVisible() const { return _isPreviewPlaneVisible; }
    TrailPreviewGateMode Get_TrailPreviewGateMode() const { return _trailPreviewGateMode; }
    float Get_TrailPreviewGateStartRatio() const { return _trailPreviewGateStartRatio; }
    float Get_TrailPreviewGateEndRatio() const { return _trailPreviewGateEndRatio; }
    // SourceHistorySpriteTrail preview source sample 공급 구간의 editor-session mode다.
    TrailPreviewGateMode Get_SourceHistorySpriteTrailPreviewGateMode() const { return _sourceHistorySpriteTrailPreviewGateMode; }
    // SourceHistorySpriteTrail preview source sample 공급 시작 ratio다.
    float Get_SourceHistorySpriteTrailPreviewGateStartRatio() const { return _sourceHistorySpriteTrailPreviewGateStartRatio; }
    // SourceHistorySpriteTrail preview source sample 공급 종료 ratio다.
    float Get_SourceHistorySpriteTrailPreviewGateEndRatio() const { return _sourceHistorySpriteTrailPreviewGateEndRatio; }
    bool Is_TrailPreviewGateOpen() const;
    // 실제 attack animation ratio 기준으로 Trail preview gate를 판정한다.
    bool Is_TrailPreviewGateOpen(float sourceRatio) const;
    // 실제 attack animation ratio 기준으로 SourceHistorySpriteTrail preview gate를 판정한다.
    bool Is_SourceHistorySpriteTrailPreviewGateOpen(float sourceRatio) const;
    // Trail preview anchor marker/line 표시 여부다.
    bool Is_TrailPreviewDebugRenderEnabled() const { return _isTrailPreviewDebugRenderEnabled; }
    // Trail preview player animation의 editor-session 배속이다.
    float Get_TrailPreviewAnimationSpeed() const { return _trailPreviewAnimationSpeed; }
    // Trail preview player animation 배속 reset 기준값이다.
    float Get_DefaultTrailPreviewAnimationSpeed() const { return kDefaultTrailPreviewAnimationSpeed; }
    // Effect shader 기준 influence 범위 외곽선 표시 여부다.
    bool Is_EffectInfluenceOutlineEnabled() const { return _isEffectInfluenceOutlineEnabled; }
    // Trail preview weapon-local base marker의 editor-session offset이다.
    Vec3 Get_TrailPreviewBaseLocalOffset() const { return _trailPreviewBaseLocalOffset; }
    // Trail preview weapon-local tip marker의 editor-session offset이다.
    Vec3 Get_TrailPreviewTipLocalOffset() const { return _trailPreviewTipLocalOffset; }
    // SourceHistory preview weapon-local source marker의 editor-session offset이다.
    Vec3 Get_TrailPreviewSourceLocalOffset() const { return _sourceHistorySpriteTrailPreviewSourceLocalOffset; }
    // SourceHistorySpriteTrail preview weapon-local source marker의 editor-session offset이다.
    Vec3 Get_SourceHistorySpriteTrailPreviewSourceLocalOffset() const { return _sourceHistorySpriteTrailPreviewSourceLocalOffset; }
    // Trail preview base offset reset 기준값이다.
    Vec3 Get_DefaultTrailPreviewBaseLocalOffset() const { return kDefaultTrailPreviewBaseLocalOffset; }
    // Trail preview tip offset reset 기준값이다.
    Vec3 Get_DefaultTrailPreviewTipLocalOffset() const { return kDefaultTrailPreviewTipLocalOffset; }
    // SourceHistory preview source offset reset 기준값이다.
    Vec3 Get_DefaultTrailPreviewSourceLocalOffset() const { return kDefaultTrailPreviewSourceLocalOffset; }
    // SourceHistorySpriteTrail preview source offset reset 기준값이다.
    Vec3 Get_DefaultSourceHistorySpriteTrailPreviewSourceLocalOffset() const { return kDefaultTrailPreviewSourceLocalOffset; }
    float Get_DefaultTrailPreviewGateStartRatio() const { return kDefaultTrailPreviewGateStartRatio; }
    float Get_DefaultTrailPreviewGateEndRatio() const { return kDefaultTrailPreviewGateEndRatio; }
    // SourceHistorySpriteTrail preview gate 시작 ratio reset 기준값이다.
    float Get_DefaultSourceHistorySpriteTrailPreviewGateStartRatio() const { return kDefaultSourceHistorySpriteTrailPreviewGateStartRatio; }
    // SourceHistorySpriteTrail preview gate 종료 ratio reset 기준값이다.
    float Get_DefaultSourceHistorySpriteTrailPreviewGateEndRatio() const { return kDefaultSourceHistorySpriteTrailPreviewGateEndRatio; }
    const EffectEditorPostProcessOptions& Get_PostProcessOptions() const { return _postProcessOptions; }
    EffectEditorPostProcessOptions Get_DefaultPostProcessOptions() const;
    bool Has_ClientPostProcessSnapshot() const { return _hasClientPostProcessSnapshot; }
    const EffectEditorPostProcessOptions& Get_ClientPostProcessSnapshot() const { return _clientPostProcessSnapshot; }
    EffectEditorPostProcessPreset Get_PostProcessPreset() const { return _postProcessPreset; }

public: //## Accessor::InputPolicy
    bool Is_PreviewCameraControlEnabled() const { return _isPreviewCameraControlEnabled; }
    bool Is_PreviewCameraDragging() const { return _isPreviewCameraDragging; }

public: //## Accessor::Viewport
    ShaderResourceView* Get_SharedViewportSRV() const;
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
    void Tick_AutoReplayPreview(float timeDelta);
    void Tick_TrailPreviewGate(float timeDelta);
    void Reset_TrailPreviewGate();

public: //## Behavior::PreviewOption
    void Set_AutoReplayPreviewEnabled(bool enabled);
    void Set_TrailPreviewVisible(bool visible);
    void Set_PreviewPlaneVisible(bool visible);
    void Set_TrailPreviewGateMode(TrailPreviewGateMode mode);
    void Set_TrailPreviewGateWindow(float startRatio, float endRatio);
    void Set_TrailPreviewGateDuration(float duration);
    // SourceHistorySpriteTrail preview source sample 공급 mode를 바꾼다.
    void Set_SourceHistorySpriteTrailPreviewGateMode(TrailPreviewGateMode mode);
    // SourceHistorySpriteTrail preview source sample 공급 ratio 구간을 바꾼다.
    void Set_SourceHistorySpriteTrailPreviewGateWindow(float startRatio, float endRatio);
    // Trail preview anchor marker/line 표시 여부만 바꾼다.
    void Set_TrailPreviewDebugRenderEnabled(bool enabled);
    // Trail preview player animation 배속을 현재 editor session에만 반영한다.
    void Set_TrailPreviewAnimationSpeed(float speed);
    // EffectEditor 세션 전용 influence outline debug 표시를 켜고 끈다.
    void Set_EffectInfluenceOutlineEnabled(bool enabled);
    // Detail_View의 Trail Preview 조정값을 현재 editor session에만 반영한다.
    void Set_TrailPreviewLocalOffsets(const Vec3& baseLocalOffset, const Vec3& tipLocalOffset);
    void Set_TrailPreviewLocalOffsets(const Vec3& baseLocalOffset, const Vec3& tipLocalOffset, const Vec3& sourceLocalOffset);
    // Detail_View의 SourceHistorySpriteTrail source marker 조정값을 현재 editor session에만 반영한다.
    void Set_SourceHistorySpriteTrailPreviewSourceLocalOffset(const Vec3& sourceLocalOffset);
    // Trail Preview 조정값을 기본 weapon-local marker 위치로 되돌린다.
    void Reset_TrailPreviewLocalOffsets();
    void Apply_PostProcessOptions(const EffectEditorPostProcessOptions& options);
    void Capture_ClientPostProcessSnapshot(const EffectEditorPostProcessOptions& options);
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
    inline static const Vec3 kDefaultTrailPreviewBaseLocalOffset{ 0.f, 1.f, 35.f };
    inline static const Vec3 kDefaultTrailPreviewTipLocalOffset{ 0.f, 1.f, 85.f };
    inline static const Vec3 kDefaultTrailPreviewSourceLocalOffset{ 0.f, 1.f, 85.f };
    static constexpr float kDefaultTrailPreviewGateStartRatio{ 0.f };
    static constexpr float kDefaultTrailPreviewGateEndRatio{ 0.4f };
    static constexpr float kDefaultSourceHistorySpriteTrailPreviewGateStartRatio{ 0.f };
    static constexpr float kDefaultSourceHistorySpriteTrailPreviewGateEndRatio{ 0.4f };
    static constexpr float kDefaultTrailPreviewGateDuration{ 1.f };
    static constexpr float kDefaultTrailPreviewAnimationSpeed{ 1.f };
    static constexpr float kMinTrailPreviewAnimationSpeed{ 0.01f };
    static constexpr float kMaxTrailPreviewAnimationSpeed{ 10.f };
    static constexpr float kAutoReplayPreviewDelay{ 1.f };

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
    bool _autoReplayPreviewEnabled{ false };
    float _autoReplayPreviewElapsedTime{ 0.f };
    string _copiedObjectJson{};
    wstring _copiedLayerTag{};

private: //## Data::PreviewOption
    bool _isTrailPreviewVisible{ false };
    bool _isPreviewPlaneVisible{ false };
    TrailPreviewGateMode _trailPreviewGateMode{ TrailPreviewGateMode::Window };
    float _trailPreviewGateStartRatio{ kDefaultTrailPreviewGateStartRatio };
    float _trailPreviewGateEndRatio{ kDefaultTrailPreviewGateEndRatio };
    float _trailPreviewGateElapsedTime{ 0.f };
    float _trailPreviewGateDuration{ kDefaultTrailPreviewGateDuration };
    TrailPreviewGateMode _sourceHistorySpriteTrailPreviewGateMode{ TrailPreviewGateMode::Window };
    float _sourceHistorySpriteTrailPreviewGateStartRatio{ kDefaultSourceHistorySpriteTrailPreviewGateStartRatio };
    float _sourceHistorySpriteTrailPreviewGateEndRatio{ kDefaultSourceHistorySpriteTrailPreviewGateEndRatio };
    bool _isTrailPreviewDebugRenderEnabled{ true };
    float _trailPreviewAnimationSpeed{ kDefaultTrailPreviewAnimationSpeed };
    bool _isEffectInfluenceOutlineEnabled{ false };
    Vec3 _trailPreviewBaseLocalOffset{ kDefaultTrailPreviewBaseLocalOffset };
    Vec3 _trailPreviewTipLocalOffset{ kDefaultTrailPreviewTipLocalOffset };
    Vec3 _sourceHistorySpriteTrailPreviewSourceLocalOffset{ kDefaultTrailPreviewSourceLocalOffset };
    EffectEditorPostProcessOptions _postProcessOptions{};
    bool _hasClientPostProcessSnapshot{ false };
    EffectEditorPostProcessOptions _clientPostProcessSnapshot{};
    EffectEditorPostProcessPreset _postProcessPreset{ EffectEditorPostProcessPreset::PreviewFriendly };

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
