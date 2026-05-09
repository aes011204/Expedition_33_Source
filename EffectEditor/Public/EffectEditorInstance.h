#pragma once

#include "Base.h"

namespace DirectX
{
namespace DX11
{
    class BasicEffect;
    template <typename TVertex>
    class PrimitiveBatch;
    struct VertexPositionColor;
}
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

class EffectEditorInstance final : public Base
{
    DECLARE_SINGLETON(EffectEditorInstance)

public:
    EffectEditorInstance();
    ~EffectEditorInstance() override = default;

public:
    HRESULT Initialize_Editor(const EffectEditorDesc& editorDesc, const ComPtr<Device>& device, const ComPtr<Context>& context);
    void Release_Editor();

    void Update_Editor(float timeDelta);
    void Render_Editor();

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

public: //## Behavior::Window
    void Request_WindowFocus(EditorViewportTarget target);
    bool Consume_WindowFocusRequest(EditorViewportTarget target);

public: //## Behavior::Command
    void Execute_Command(const Shared<ICommand>& command);
    void Undo();
    void Redo();
    void Clear_CommandHistory();

public: //## Behavior::CameraControl
    void Activate_TargetCamera();
    void Activate_PreviewCamera();
    void Register_PreviewCamera(const Shared<EffectEditorCamera>& previewCamera);
    bool Is_PreviewCameraActive() const;

public: //## Behavior::InputPolicy
    void Set_SceneViewportInputState(bool canInteract, bool blocked);
    void Set_GameViewportInputState(bool canInteract);
    void Set_PreviewCameraDragging(bool isDragging) { _isPreviewCameraDragging = isDragging; }

    void Apply_RuntimeInputPolicy();

public: //## Behavior::Viewport
    Shared<RenderTarget> Prepare_SharedViewportRenderTarget(uint32 width, uint32 height);
    void Capture_BackBuffer_ToViewport();

    void Request_PreviewViewportSize(uint32 width, uint32 height);

    void Enable_AutoPreviewViewport();
    void Enable_ManualPreviewViewport();
    void Set_ManualPreviewViewportSize(uint32 width, uint32 height);
    void Sync_PreviewViewportMode();

private:
    static constexpr uint32 defaultPreviewViewportWidth = 900;
    static constexpr uint32 defaultPreviewViewportHeight = 900;
    static constexpr int sceneGridDivisions = 40;
    static constexpr float sceneGridHalfExtent = 30.f;
    static constexpr float sceneGridHeight = 0.f;

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
    uint32 _pendingPreviewViewportWidth{ defaultPreviewViewportWidth };
    uint32 _pendingPreviewViewportHeight{ defaultPreviewViewportHeight };
    uint32 _appliedPreviewViewportWidth{ 0 };
    uint32 _appliedPreviewViewportHeight{ 0 };

private: //## Data::Runtime
    EffectEditorRuntimeState _runtimeState{ EffectEditorRuntimeState::Edit };
    float _runtimeTimeScale{ 1.f };
    bool _pendingSingleFrameStep{ false };

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

private: //## Helper::PreviewViewport
    void Ensure_SceneGridResources();
    void Render_SceneGridToBackBuffer();
    void Queue_PreviewViewportSize(uint32 width, uint32 height);
    void Apply_PendingPreviewViewportSize();

public:
    void Free() override;
};

NS_END
