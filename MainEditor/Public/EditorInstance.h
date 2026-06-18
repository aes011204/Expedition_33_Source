#pragma once
#include "Base.h"
#include "Editor_Defines.h"
#include "PlayerStatCom.h"

NS_BEGIN(Engine)
class RenderTarget;
class ICommand;
class GameObject;
class Component;
class Camera;
class UI_Base;
NS_END

namespace Client
{
    class IRuntimeWorldUI_EditorTarget;
}

NS_BEGIN(MainEditor)

class Editor_Window;
class Editor_Manager;
class ImGui_Manager;
class Editor_Context;
class CommandHistory;
class Notification_Manager;
class Inspector_Factory;
class Component_Inspector;
class EditorPickingSystem;

class EditorInstance final : public Base
{
    DECLARE_SINGLETON(EditorInstance)

public:
    enum class MouseLockOverrideMode : uint8
    {
        Auto,
        Locked,
        Unlocked,
    };

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
    void    Enter_EditAfterLoading();
    void    Force_EnablePlayerInputForPlay();

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

public: /* For.Preview */
    PreviewOwner            Get_ActivePreviewOwner() const;
    bool                    Is_ActivePreviewOwner(PreviewOwner owner) const;

public: /* For.Editor Selection */
    const Editor_Context*   Get_EditorContext() const { return _editorContext.get(); }
    Shared<GameObject>      Selected_Object() const;
    Shared<UI_Base>         Selected_UI() const;
    string                  Selected_UIGroup() const;
    string                  Selected_UIDisplayName() const;
    bool                    Selected_UIPositionDrivenByCode() const;
    Client::IRuntimeWorldUI_EditorTarget* Selected_RuntimeWorldUI() const;
    string                  Selected_RuntimeWorldUIGroup() const;
    string                  Selected_RuntimeWorldUIDisplayName() const;
    void                    Select_Object(const Shared<GameObject>& gameObject);
    void                    Select_UI(
        const Shared<UI_Base>& ui,
        const string& groupName,
        const string& displayName,
        bool positionDrivenByCode = false);
    void                    Select_RuntimeWorldUI(
        Client::IRuntimeWorldUI_EditorTarget* target,
        const string& groupName,
        const string& displayName);
    void                    Clear_SelectedObject();
    void                    Clear_SelectedUI();
    void                    Clear_SelectedRuntimeWorldUI();
    uint32                  Selection_Revision() const;
    void                    Copy_SelectedObject();
    Shared<GameObject>      Paste_CopiedObject();
    Shared<GameObject>      Duplicate_SelectedObject();
    bool                    Delete_SelectedObject();

public: /* For.Editor Focus */
    void                    Request_FocusObject(const Shared<GameObject>& gameObject);
    Shared<GameObject>      Consume_FocusObjectRequest();

public: /* For.Editor World */
    HRESULT Save_CurrentLevelSnapShot(const fs::path& path) const;
    HRESULT Load_CurrentLevelSnapShot(const fs::path& path);
    bool    Capture_DemoCheckpoint(const fs::path& path) const;
    bool    Load_DemoCheckpointInEditor(const fs::path& path);
    void    Apply_SavedPrefabToCurrentLevel(const string& prefabName, const fs::path& prefabPath);

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
    void    Restore_PreviousCameraOrTarget();

public: /* For.Viewport */
    Shared<RenderTarget>      Prepare_SharedViewportRenderTarget(uint32 width, uint32 height);
    Shared<RenderTarget>      Prepare_SharedPreviewRenderTarget(uint32 width, uint32 height);
    void                      Request_ViewportSize(EDITOR_VIEWPORT_TARGET targetType, uint32 width, uint32 height);
    ID3D11ShaderResourceView* Get_SharedViewportSRV() const;
    ID3D11ShaderResourceView* Get_SharedPreviewSRV() const;
    void                      Capture_BackBuffer_ToViewport();
    void                      Capture_BackBuffer_ToPreview();
    void                      Capture_BackBuffer_ToRenderTarget(const Shared<RenderTarget>& renderTarget);
    void                      Invalidate_BackBufferCache();
    void                      EndImGuiFrameIfBegun();
    ComPtr<Device>            Get_Device() const { return _device; }
    ComPtr<Context>           Get_Context() const { return _context; }


    void                     Request_PreviewViewportSize(uint32 width, uint32 height);
    void                     Apply_PreviewViewportSizeImmediate(uint32 width, uint32 height);

    void                     Enable_AutoPreviewViewport();
    void                     Enable_ManualPreviewViewport();
    void                     Set_ManualPreviewViewportSize(uint32 width, uint32 height);
    PreviewViewportMode      Get_PreviewViewportMode() const { return _previewViewportMode; }
    bool                     Is_ManualPreviewViewportEnabled() const { return _previewViewportMode == PreviewViewportMode::Manual; }
    void                     Sync_PreviewViewportMode();
    float                    Get_EffectiveViewportAspect() const;

    void                     Apply_RuntimeInputPolicy();
    void                     Toggle_MouseLockOverride();
    MouseLockOverrideMode    Get_MouseLockOverrideMode() const { return _mouseLockOverrideMode; }

public: // for pickingSystem
    void DoPickAt(const ImVec2& mousePos, const ImVec2& vpMin, const ImVec2& vpMax);
    void Request_Pick(const ImVec2& mousePos, const ImVec2& vpMin, const ImVec2& vpMax);
    void Flush_PendingPick();

public: // for targetCamera edit mode
    void Toogle_TargetCameraEditMode();
    void Set_TargetCameraEditMode(bool enable) { _isTargetCameraEditMode = enable;}
    const bool Is_TargetCameraEditMode() { return _isTargetCameraEditMode;}

private:
    void Focus_GameView();
    void Focus_SceneView();
    bool                    Find_ObjectLayerTag(const Shared<GameObject>& gameObject, wstring& outLayerTag) const;
    Shared<GameObject>      Instantiate_GameObjectFromCopiedData() const;

    // Game, Scene View 하나만 렌더되게
    void Sync_ViewportWindowState();

private:
    void Queue_PreviewViewportSize(uint32 width, uint32 height);
    void Apply_PendingPreviewViewportSize();
    ID3D11Texture2D* Get_CachedBackBufferTexture();

private: /* 저장 */
    struct CameraPoseSnapshot
    {
        bool isValid = false;
        Vec3 position = Vec3::Zero;
        Vec3 rotation = Vec3::Zero;
        float yaw = 0.f;
        float pitch = 0.f;
    };

    struct EditorWorldObjectSnapshot
    {
        wstring layerTag = {};
        json objectJson = {};
        Weak<GameObject> originalObject = {};
    };

    struct EditorHeroStatSnapshot
    {
        HeroId heroId = HeroId::END;
        Client::PlayerStatCom::BattleSyncSnapshot statSnapshot = {};
        EAliveStatus aliveStatus = EAliveStatus::Alive;
    };

    struct PendingPickRequest
    {
        bool pending = false;
        ImVec2 mousePos = {};
        ImVec2 vpMin = {};
        ImVec2 vpMax = {};
    };
    PendingPickRequest _pendingPick = {};

    void Refresh_PlayStartMonsterSnapshotsForPrefab(const string& prefabName, const json& prefabRoot);

    void Capture_PlayStartMonsterSnapshots();
    void Restore_PlayStartMonsterSnapshots();
    void Capture_PlayStartHeroStatSnapshots();
    void Restore_PlayStartHeroStatSnapshots();
    bool Capture_ActiveCameraPose(CameraPoseSnapshot& outPose) const;
    bool Apply_CameraPoseToCameraType(ObjectType objectType, const CameraPoseSnapshot& pose) const;
    bool Is_SnapshotExcludedObject(const Shared<GameObject>& gameObject) const;
    bool Is_PlaySnapshotExcludedObject(const wstring& layerTag, const Shared<GameObject>& gameObject) const;
    vector<EditorWorldObjectSnapshot> Capture_CurrentLevelObjects(
        int32* outSelectedIndex = nullptr,
        bool excludeMapStaticMesh = false) const;
    HRESULT Restore_CurrentLevelObjects(
        const vector<EditorWorldObjectSnapshot>& snapshots,
        int32 selectedIndex,
        bool excludeMapStaticMesh = false);

private:
    bool                        _isTargetCameraEditMode = false;
    MouseLockOverrideMode       _mouseLockOverrideMode = MouseLockOverrideMode::Auto;

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
    Unique<EditorPickingSystem> _pickingSystem = {};

private: /* Viewport Data */
    Shared<RenderTarget> _sharedViewportRenderTarget = nullptr;
    Shared<RenderTarget> _sharedPreviewRenderTarget = nullptr;
    ComPtr<ID3D11Texture2D> _cachedBackBufferTexture = nullptr;
    uint32 _cachedBackBufferWidth = 0;
    uint32 _cachedBackBufferHeight = 0;

    uint32 _requestedViewportWidth = 1;
    uint32 _requestedViewportHeight = 1;
    EDITOR_VIEWPORT_TARGET _lastViewportRequester = EDITOR_VIEWPORT_TARGET::Game;

    PreviewViewportMode _previewViewportMode = PreviewViewportMode::Manual;
    bool _hasPendingPreviewViewportSize = false;
    uint32 _pendingPreviewViewportWidth = 1600;
    uint32 _pendingPreviewViewportHeight = 900;
    uint32 _appliedPreviewViewportWidth = 0;
    uint32 _appliedPreviewViewportHeight = 0;

private: /* Runtime Data */
    EDITOR_RUNTIME_STATE _runtimeState = EDITOR_RUNTIME_STATE::Play;
    float _runtimeTimeScale = 1.f;

    vector<EditorWorldObjectSnapshot> _playStartMonsterSnapshots = {};
    bool _hasPlayStartMonsterSnapshots = false;
    vector<EditorHeroStatSnapshot> _playStartHeroStatSnapshots = {};
    bool _hasPlayStartHeroStatSnapshots = false;
    CameraPoseSnapshot _playStartCameraPose = {};
    Weak<Camera> _cameraBeforeFreeCamera = {};

    bool _hasPendingWindowFocus = false;
    EDITOR_VIEWPORT_TARGET _pendingWindowFocus = EDITOR_VIEWPORT_TARGET::Game;
    uint32 _runtimeGameFocusRecoveryFrames = 0;
    mutable PreviewOwner _stickyPreviewOwner = PreviewOwner::None;

    bool _pendingSingleFrameStep = false;
    string _copiedObjectJson = {};
    wstring _copiedLayerTag = {};

public:
    void Free() override;
};

NS_END
