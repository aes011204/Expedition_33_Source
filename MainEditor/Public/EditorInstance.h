#pragma once
#include "Base.h"
#include "Editor_Defines.h"

NS_BEGIN(Engine)
class RenderTarget;
class ICommand;
class GameObject;
class Component;
NS_END

NS_BEGIN(MainEditor)

class Editor_Window;
class Editor_Manager;
class ImGui_Manager;
class Editor_Context;
class CommandHistory;
class Notification_Manager;
class Inspector_Factory;
class Component_Inspector;

class EditorInstance final : public Base
{
    DECLARE_SINGLETON(EditorInstance)

public:
    EditorInstance();
    virtual ~EditorInstance() = default;

public: /* Core */
    HRESULT Initialize_Editor(const EDITOR_DESC& desc, const ComPtr<Device>& device, const ComPtr<Context>& context);
    void    Release_Editor();

    void    Update_Editor(float timeDelta);
    void    Render_Editor();

public: /* For.Runtime */
    void    Play();
    void    Pause();
    void    Stop();
    void    Resume();

    bool    IsPlaying() const;
    bool    IsPaused() const;

    void    Set_RuntimeTimeScale(float timeScale);
    void    Reset_RuntimeTimeScale();
    float   Get_RuntimeTimeScale() const;

    void    Request_FrameStep();
    bool    Consume_FrameStep();

public: /* For.Window */
    void                    Request_WindowFocus(EDITOR_VIEWPORT_TARGET target);
    bool                    Consume_WindowFocusRequest(EDITOR_VIEWPORT_TARGET target);
    Shared<Editor_Window>   Get_Window(const wstring& windowName) const;

public: /* For.Editor Selection */
    Shared<GameObject>      Selected_Object() const;
    void                    Select_Object(const Shared<GameObject>& gameObject);
    void                    Clear_SelectedObject();
    uint32                  Selection_Revision() const;

public: /* For.Editor Focus */
    void                    Request_FocusObject(const Shared<GameObject>& gameObject);
    Shared<GameObject>      Consume_FocusObjectRequest();

public: /* For.Inspector */
    Shared<Component_Inspector> Find_Inspector(const string& key) const;

public: /* For.Command History */
    void    Execute_Command(const Shared<ICommand>& command);
    void    Undo();
    void    Redo();
    void    Clear_CommandHistory();

public: /* For.Notification */
    public: /* Notification Manager */
    Notification_Manager*       Get_Notification() { return _notificationManager.get(); }

public: /* For.Camera */
    void    Activate_TargetCamera();
    void    Activate_FreeCamera();

public: /* For.Viewport */
    Shared<RenderTarget>      Prepare_SharedViewportRenderTarget(uint32 width, uint32 height);
    void                      Request_ViewportSize(EDITOR_VIEWPORT_TARGET targetType, uint32 width, uint32 height);
    ID3D11ShaderResourceView* Get_SharedViewportSRV() const;
    void                      Capture_BackBuffer_ToViewport();

    void                      Apply_RuntimeInputPolicy();

private:
    void Focus_GameView();
    void Focus_SceneView();

private: /* Core Data */
    ComPtr<Device>               _device = {};
    ComPtr<Context>              _context = {};
    EDITOR_DESC                  _editorDesc = {};

private: /* Manager */
    Unique<Editor_Manager>       _editorManager = {};
    Unique<ImGui_Manager>        _imguiManager = {};
    Unique<Editor_Context>       _editorContext = {};
    Unique<Inspector_Factory>    _inspectorFactory = {};
    Unique<CommandHistory>       _commandHistory = {};
    Unique<Notification_Manager> _notificationManager = {};

private: /* Viewport Data */
    Shared<RenderTarget> _sharedViewportRenderTarget = nullptr;

    uint32 _requestedViewportWidth = 1;
    uint32 _requestedViewportHeight = 1;
    EDITOR_VIEWPORT_TARGET _lastViewportRequester = EDITOR_VIEWPORT_TARGET::Game;

private: /* Runtime Data */
    EDITOR_RUNTIME_STATE _runtimeState = EDITOR_RUNTIME_STATE::Edit;
    float _runtimeTimeScale = 1.f;

    bool _hasPendingWindowFocus = false;
    EDITOR_VIEWPORT_TARGET _pendingWindowFocus = EDITOR_VIEWPORT_TARGET::Game;

    bool _pendingSingleFrameStep = false;

public:
    void Free() override;
};

NS_END
