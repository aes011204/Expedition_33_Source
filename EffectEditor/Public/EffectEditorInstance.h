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
    Editor_Context* Get_Context() const { return _editorContext.get(); }

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
    void Request_ViewportSize(EditorViewportTarget targetType, uint32 width, uint32 height);
    ID3D11ShaderResourceView* Get_SharedViewportSRV() const;
    void Capture_BackBuffer_ToViewport();

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

    uint32 _requestedViewportWidth = 1;
    uint32 _requestedViewportHeight = 1;
    EditorViewportTarget _lastViewportRequester = EditorViewportTarget::Game;

    EffectEditorRuntimeState _runtimeState = EffectEditorRuntimeState::Edit;
    float _runtimeTimeScale = 1.f;

    bool _hasPendingWindowFocus = false;
    EditorViewportTarget _pendingWindowFocus = EditorViewportTarget::Game;

    bool _pendingSingleFrameStep = false;

public:
    void Free() override;
};

NS_END
