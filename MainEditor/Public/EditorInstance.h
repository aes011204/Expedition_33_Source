#pragma once
#include "Base.h"
#include "Editor_Defines.h"

NS_BEGIN(Engine)
class RenderTarget;
class ICommand;
class GameObject;
NS_END

NS_BEGIN(MainEditor)

class Editor_Window;
class Editor_Manager;
class ImGui_Manager;
class Editor_Context;
class CommandHistory;
class Notification_Manager;

class EditorInstance final : public Base
{
    DECLARE_SINGLETON(EditorInstance)

public:
    EditorInstance();
    virtual ~EditorInstance() = default;

public:
    HRESULT                     Initialize_Editor(const EDITOR_DESC& desc, const ComPtr<Device>& device, const ComPtr<Context>& context);
    void                        Release_Editor();

    void                        Update_Editor(float timeDelta);
    void                        Render_Editor();

public:
    void                        Play();
    void                        Pause();
    void                        Stop();
    void                        Resume();

    bool                        IsPlaying() const;
    bool                        IsPaused() const;

    void                        Set_RuntimeTimeScale(float timeScale);
    void                        Reset_RuntimeTimeScale();
    float                       Get_RuntimeTimeScale() const;

    void                        Request_WindowFocus(EDITOR_VIEWPORT_TARGET target);
    bool                        Consume_WindowFocusRequest(EDITOR_VIEWPORT_TARGET target);

public:
    Shared<Editor_Window>       Get_Window(const wstring& windowName) const;
    Editor_Context*             Get_Context() const { return _editorContext.get(); }

public: /* Command History */
    void                        Execute_Command(const Shared<ICommand>& command);
    void                        Undo();
    void                        Redo();
    void                        Clear_CommandHistory();

public: /* Notification Manager */
    Notification_Manager*       Get_Notification() { return _notificationManager.get(); }

public: /* Camera */
    void                        Activate_TargetCamera();
    void                        Activate_FreeCamera();

public:
    Shared<RenderTarget>        Prepare_SharedViewportRenderTarget(uint32 width, uint32 height);
    void                        Request_ViewportSize(EDITOR_VIEWPORT_TARGET targetType, uint32 width, uint32 height);
    ID3D11ShaderResourceView*   Get_SharedViewportSRV() const;
    void                        Capture_BackBuffer_ToViewport();

    void                        Apply_RuntimeInputPolicy();

    // Next Frame
    void                        Request_FrameStep();
    bool                        Consume_FrameStep();

private:
    void                        Focus_GameView();
    void                        Focus_SceneView();

private:
    ComPtr<Device>  _device = {};
    ComPtr<Context> _context = {};
    EDITOR_DESC     _editorDesc = {};

    Unique<Editor_Manager>  _editorManager = {};
    Unique<ImGui_Manager>   _imguiManager = {};
    Unique<Editor_Context>  _editorContext = {};
    Unique<CommandHistory>  _commandHistory = {};

    Unique<Notification_Manager>    _notificationManager;

private:
    Shared<RenderTarget> _sharedViewportRenderTarget = nullptr;

    uint32 _requestedViewportWidth = 1;
    uint32 _requestedViewportHeight = 1;
    EDITOR_VIEWPORT_TARGET _lastViewportRequester = EDITOR_VIEWPORT_TARGET::Game;

    EDITOR_RUNTIME_STATE _runtimeState = EDITOR_RUNTIME_STATE::Edit;
    float _runtimeTimeScale = 1.f;

    // 전환될 상태
    bool _hasPendingWindowFocus = false;
    EDITOR_VIEWPORT_TARGET _pendingWindowFocus = EDITOR_VIEWPORT_TARGET::Game;

    bool _pendingSingleFrameStep = false;

public:
    void Free() override;
};

NS_END
