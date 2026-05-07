#pragma once

#include "Base.h"

NS_BEGIN(Engine)
class RenderTarget;
class ICommand;
class GameObject;
NS_END

NS_BEGIN(EffectEditor)

class Editor_Window;
class EffectEditor_Manager;
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

public:
    void Play();
    void Pause();
    void Stop();
    void Resume();

    bool IsPlaying() const;
    bool IsPaused() const;

    void Set_RuntimeTimeScale(float timeScale);
    void Reset_RuntimeTimeScale();
    float Get_RuntimeTimeScale() const;

    void Request_WindowFocus(EditorViewportTarget target);
    bool Consume_WindowFocusRequest(EditorViewportTarget target);

public:
    Shared<Editor_Window> Get_Window(const wstring& windowName) const;
    HWND Get_WindowHandle() const;
    Editor_Context* Get_EditorContext() const { return _editorContext.get(); }

public:
    void Execute_Command(const Shared<ICommand>& command);
    void Undo();
    void Redo();
    void Clear_CommandHistory();

public:
    Notification_Manager* Get_Notification() { return _notificationManager.get(); }

public:
    void Activate_TargetCamera();
    void Activate_FreeCamera();

public:
    Shared<RenderTarget> Prepare_SharedViewportRenderTarget(uint32 width, uint32 height);
    ID3D11ShaderResourceView* Get_SharedViewportSRV() const;
    void Capture_BackBuffer_ToViewport();
    ComPtr<Device> Get_Device() const { return _device; }
    ComPtr<Context> Get_Context() const { return _context; }

    void Request_PreviewViewportSize(uint32 width, uint32 height);

    void Enable_AutoPreviewViewport();
    void Enable_ManualPreviewViewport();
    void Set_ManualPreviewViewportSize(uint32 width, uint32 height);
    PreviewViewportMode Get_PreviewViewportMode() const { return _previewViewportMode; }
    bool Is_ManualPreviewViewportEnabled() const { return _previewViewportMode == PreviewViewportMode::Manual; }
    void Sync_PreviewViewportMode();
    float Get_EffectiveViewportAspect() const;

    void Apply_RuntimeInputPolicy();

    void Request_FrameStep();
    bool Consume_FrameStep();

private:
    ComPtr<Device> _device = {};
    ComPtr<Context> _context = {};
    EffectEditorDesc _editorDesc = {};

    Unique<EffectEditor_Manager> _editorManager = {};
    Unique<ImGui_Manager> _imguiManager = {};
    Unique<Editor_Context> _editorContext = {};
    Unique<CommandHistory> _commandHistory = {};
    Unique<Notification_Manager> _notificationManager = {};

private:
    Shared<RenderTarget> _sharedViewportRenderTarget = nullptr;

    PreviewViewportMode _previewViewportMode = PreviewViewportMode::Auto;
    bool _hasPendingPreviewViewportSize = false;
    uint32 _pendingPreviewViewportWidth = 1;
    uint32 _pendingPreviewViewportHeight = 1;
    uint32 _appliedPreviewViewportWidth = 0;
    uint32 _appliedPreviewViewportHeight = 0;

    EffectEditorRuntimeState _runtimeState = EffectEditorRuntimeState::Edit;
    float _runtimeTimeScale = 1.f;

    bool _hasPendingWindowFocus = false;
    EditorViewportTarget _pendingWindowFocus = EditorViewportTarget::Game;

    bool _pendingSingleFrameStep = false;

private:
    void Queue_PreviewViewportSize(uint32 width, uint32 height);
    void Apply_PendingPreviewViewportSize();

public:
    void Free() override;
};

NS_END
