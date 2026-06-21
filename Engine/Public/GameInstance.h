#pragma once

#pragma region INCLUDE

#include "Base.h"
#include "Input_Manager.h"
#include "Prototype_Manager.h"
#include "TypeRegistry.h"
#include "Debug_Types.h"
#include "Engine_Constants.h"
#include <string_view>
#include <utility>
#include "DelegateHub.h"

#include "Log_Define.h"
#ifdef _DEBUG
#include "Print_Manager.h"
#include "Shadow.h"
#endif

#pragma endregion

#pragma region FD
NS_BEGIN(Engine)

class Event_Bus;
class Graphic_Device;
struct ShadowCascadeDebugInfo;
class Timer_Manager;
class Level_Manager;
class Prototype_Manager;
class UI_Manager;
class Object_Manager;
class Renderer;
class PipeLine;
class Light_Manager;
class Font_Manager;
class Asset_Manager;
class Sound_Manager;
class Log_Manager;
class Target_Manager;
class Camera_Manager;
struct ConsoleLogEntry;
class Component;
class GameObject;
class UI_Base;
class Level;
class Camera;
class Debug_Manager;
class Collision_Manager;
class PhysX_Manager;
class ColliderCom;
class Shadow;
class Picking;
class Culling_Manager;
class ThreadPool;
class PostProcessPipeline;
class LightingPipeline;
class ScreenFx_Manager;
class ShaderCom_Controller;
class ShaderResourceRegistry;
struct StaticCollisionData;
class ScreenFx_Instance;
struct DOFPreset;
struct DOFTransitionDesc;
NS_END
#pragma endregion

NS_BEGIN(Engine)

enum class Collision_Preset;

struct PhysXStaticActorHandle;
struct PhysXQueryHit;

struct DebugResourceMemorySummary
{
    uint32 texturePrototypeCount{};
    uint32 textureSrvCount{};
    size_t textureEstimatedBytes{};

    uint32 modelPrototypeCount{};
    uint32 modelMeshCount{};
    uint32 modelMaterialCount{};
    uint32 modelBoneCount{};
    uint32 modelAnimationCount{};
    size_t modelEstimatedBytes{};

    uint32 standaloneBufferPrototypeCount{};
    size_t standaloneBufferEstimatedBytes{};

    uint32 currentLevelObjectCount{};
    uint32 currentLevelComponentCount{};
};

class ENGINE_DLL GameInstance final : public Base
{
    DECLARE_SINGLETON(GameInstance)

public:
    GameInstance();
    ~GameInstance() override;

public:
    HRESULT Initialize_Engine(const ENGINE_DESC& engineDesc, ComPtr<Device>& device, ComPtr<Context>& context);

    void Update_Engine(float timeDelta);
    // ClientInstance쪽 업데이트 순서 맞추려고
    void Update_Engine(float timeDelta, const function<void(float)>& clientUpdate);

    HRESULT Draw();
    HRESULT Draw_RendererOnly(bool preservePostProcess = false); // 에디터용
    void Backup_RenderGroup();
    void Restore_RenderGroup();

    void Clear_Resources(uint32 levelIndex);

    float Random(float min, float max);

    ComPtr<Device> Get_Device() const { return _device; }
    ComPtr<Context> Get_Context() const { return _context; }

public:
    void Set_GameplayPaused(bool paused) { _isGameplayPaused = paused; }
    bool Is_GameplayPaused() const { return _isGameplayPaused; }

    // 월드 시간 조절
    void Set_WorldTimeScale(float timeScale);
    void Slow_WorldTime(float timeScale, float duration);
    void Stop_WorldTimeSlow();

    float Get_WorldTimeScale() const { return _worldTimeScale; }
    float Get_UnscaledTimeDelta() const { return _unscaledTimeDelta; }
    float Get_WorldTimeDelta() const { return _worldTimeDelta; }

public: /* For.Log_Manager */
    void Log_FileOnly(const spdlog::source_loc& sourceLoc, spdlog::level::level_enum level, const string& message) const;
    void Visit_ConsoleLogs(const function<void(const ConsoleLogEntry& logEntry)>& visitor) const;
    void Clear_ConsoleLogs();

    uint32 Get_ConsoleLogCount() const;
    uint32 Get_ConsoleLogCapacity() const;
    void Set_ConsoleLogCapacity(uint32 capacity);
    uint32 Get_ConsoleLogRevision() const;

public: /* For.Graphic_Device */
    HRESULT Clear_Buffers(const Vec4& clearColor);
    HRESULT Present();

    void Bind_BackBuffer();
    HRESULT Resize_BackBuffer(uint32 width, uint32 height);

    ComPtr<SwapChain> Get_SwapChain() const;

public: /* For.Viewport */
    void Set_ViewportSize(uint32 width, uint32 height);
    // editor preview처럼 창 크기와 다른 렌더 비율이 필요할 때 사용하며, swapchain이 아닌 logical viewport만 갱신한다.
    HRESULT Apply_LogicalViewportSize(uint32 width, uint32 height);
    uint32 Get_ViewportWidth() const;
    uint32 Get_ViewportHeight() const;

    // 엔진 백버퍼 기준 viewport 사이즈
    uint32 Get_BackBufferWidth() const;
    uint32 Get_BackBufferHeight() const;

public: /* For.Timer_Manager */
    HRESULT Add_Timer(const wstring& timerTag);
    float Compute_TimeDelta(const wstring& timerTag);

public: /* For.Level_Manager */
    HRESULT Change_Level(uint32 newLevelIndex, Shared<Level> newLevel);

    uint32 Current_LevelIndex() const;
    Shared<Level> Current_LevelType() const;

public: /* For.Prototype_Manager */
    HRESULT Add_Prototype(uint32 levelIndex, const wstring& prototypeTag, Shared<Base> prototype);
    Shared<Base> Clone_Prototype(Prototype prototype, uint32 levelIndex, const wstring& prototypeTag, void* arg = nullptr);

    bool Has_Prototype(uint32 levelIndex, const wstring& prototypeTag) const;

public: /* For.UI_Manager */
    HRESULT Add_UI(
        uint32 prototypeLevelIndex,
        const wstring& prototypeTag,
        void* arg = nullptr,
        Shared<UI_Base>* outUI = nullptr);

    void Pop_UI();

public: /* For.Object_Manager */
    HRESULT Add_GameObject(
        uint32 prototypeLevelIndex,
        const wstring& prototypeTag,
        uint32 layerLevelIndex,
        const wstring& layerTag,
        void* arg = nullptr);

    // 게임오브젝트 추가하면서, 객체 바로 반환용
    HRESULT Add_GameObject(
        uint32 prototypeLevelIndex,
        const wstring& prototypeTag,
        uint32 layerLevelIndex,
        const wstring& layerTag,
        Shared<GameObject>& outGameObject,
        void* arg = nullptr);

    // 프리펩 스폰용
    HRESULT Add_GameObject(
        uint32 layerLevelIndex,
        const wstring& layerTag,
        const Shared<GameObject>& gameObject);

    bool Remove_GameObject(uint32 levelIndex, const Shared<GameObject>& gameObject);
    void Flush_RemovedGameObjects();
    HRESULT Clear_LevelObjects(uint32 levelIndex);

    // Layer_Player의 첫번째 오브젝트 찾기
    Shared<GameObject> Find_FirstObject(uint32 levelIndex, const wstring& layerTag);

    // 특정 레벨의 레이어에 있는 모든 오브젝트 순회 -> 에디터에서 사용
    void Visit_LevelObjects(
        uint32 levelIndex,
        const function<void(const wstring& layerTag, const Shared<GameObject>& gameObject)>& visitor) const;

    void Visit_CurrentLevelObjects(
        const function<void(const wstring& layerTag, const Shared<GameObject>& gameObject)>& visitor) const;

    uint64 Get_CurrentLevelObjectRevision() const;

public: /* For.Renderer */
    void Add_RenderGroup(RenderGroup renderGroup, Shared<GameObject> gameObject);
    uint32 Get_RenderCall() const;
    const Shared<PostProcessPipeline>   Get_PostProcessPipeline() const;
    Shared<PostProcessPipeline>         Get_PostProcessPipeline();
    HRESULT Begin_DOFOverrideScope(const string& ownerTag);
    HRESULT Set_DOFOverridePreset(const string& ownerTag, const DOFPreset& preset, const DOFTransitionDesc& transition);
    HRESULT Set_DOFOverrideFocus(const string& ownerTag, float focusDistance, float focusRange);
    HRESULT Clear_DOFOverrideToBaseline(const string& ownerTag, const DOFTransitionDesc& transition);
    HRESULT End_DOFOverrideScope(const string& ownerTag, const DOFTransitionDesc& transition);
    const Shared<LightingPipeline>      Get_LightingPipeline() const;
    Shared<LightingPipeline>            Get_LightingPipeline();
    HRESULT                             Reload_RendererShaders();
    HRESULT                             Reload_LightingPipelineShaders();
    HRESULT                             Reload_ShaderComs();
    ScreenFx_Manager*                   Get_ScreenFxManager();
    ShaderCom_Controller*               Get_ShaderComController();
    void                                Request_ScreenCapture(bool saveToFile = false);
    RenderGroup                         Get_CurrentRenderGroup() const;
    void                                Set_FogZones(const FogZoneCB& fogZoneCB);
    void                                Set_VolumetricLightShaftZones(const VolumetricLightShaftZoneCB& lightShaftZoneCB);

public: /* For.PipeLine */
    HRESULT Bind_CamPosition(ShaderCom* shader, const char* constantName);
    HRESULT Bind_TransformMatrix(D3DTS transformState, ShaderCom* shader, const char* constantName);
    HRESULT Bind_TransformMatrix_Inverse(D3DTS transformState, ShaderCom* shader, const char* constantName);

    const Matrix* Get_Transform(D3DTS transformState) const;
    const Matrix* Get_Transform_Inverse(D3DTS transformState) const;
    const Vec4* Get_CamPosition() const;

    void Set_Transform(D3DTS transformState, FXMMATRIX& transformStateMatrix);

    HRESULT Bind_CameraCB(Shared<ShaderCom> shader) const;

public: /* For.Input_Manager */
    bool KeyPress(KEY_TYPE eKey) const;
    bool KeyDown(KEY_TYPE eKey) const;
    bool KeyUp(KEY_TYPE eKey) const;

    Vec2 Get_MouseDelta() const;
    float Get_MouseWheel() const;
    void Set_MouseWheel(float mouseWheel);
    void Lock_Mouse();
    void Unlock_Mouse();
    bool Is_MouseLocked() const;

    // 에디터 뷰 전환 중 포커스가 흔들려도 마우스 락을 유지할 때 시작한다.
    void Begin_EditorMouseLockSession();
    // 에디터 뷰 전환용 마우스 락 유지 상태를 끝낸다.
    void End_EditorMouseLockSession();
    // Scene View 같은 부분 뷰 기준으로 마우스 락 영역을 임시 지정한다.
    void Set_MouseLockOverrideRect(const RECT& lockRectScreen);
    // 임시 마우스 락 영역을 해제하고 기본 창 영역으로 되돌린다.
    void Clear_MouseLockOverrideRect();

    // 입력 차단
    void Set_InputBlocked(bool isBlocked);
    // 마우스 위치 반환
    const POINT& Get_MousePosition() const;
    const POINT& Get_RawMousePosition() const;
    void Set_MousePositionOverride(const POINT& mousePosition);
    void Clear_MousePositionOverride();

public: /* For.Light_Manager */
    const LIGHT_DESC* Get_LightDesc(uint32 index);
    const LIGHT_DESC* Get_PrimaryShadowLightDesc() const;
    uint32 Get_LightCount() const;
    HRESULT Add_Light(const LIGHT_DESC& lightDesc);
    HRESULT Update_PrimaryShadowLightDesc(const LIGHT_DESC& lightDesc);
    HRESULT Render_Lights(Shared<ShaderCom> shader, Shared<VIBuffer_Rect> viBuffer);
    void Clear_Lights();
    void Get_LightDescs(vector<LIGHT_DESC>& outLightDescs) const;
    HRESULT Set_Lights(const vector<LIGHT_DESC>& lightDescs);
    HRESULT Bind_PrimaryShadowLightCB(Shared<ShaderCom> shader) const;

public: /* For.Font_Manager */
    HRESULT Add_Font(const wstring& fontTag, const wchar_t* fontFilePath);
    bool Has_Font(const wstring& fontTag) const;
    Vec2 Measure_Font(const wstring& fontTag, const wchar_t* text);
    void Draw_Font(
        const wstring& fontTag,
        const wchar_t* text,
        const Vec2& position,
        FXMVECTOR color = XMVectorSet(1.f, 1.f, 1.f, 1.f),
        float rotationRadians = 0.f,
        const Vec2& pivot = Vec2(0.f, 0.f),
        float scale = 1.f);

public: /* For.Asset_Manager */
    // missingMetaRoot가 비어 있지 않으면 force scan 중 누락 meta 자동 생성 범위를 해당 루트 하위로 제한한다.
    HRESULT Initialize_AssetManager(const wstring& resourceRoot, bool forceScan = false, const wstring& missingMetaRoot = {});

    /* 파일 경로를 GUID로 저장해야 하는 ResourceLoader/Editor 파이프라인에서 호출한다. */
    string Find_AssetGUID(const wstring& filePath) const;

    const AssetMeta* Find_AssetByGUID(const string& guid) const;
    vector<AssetMeta> Get_AssetsByType(const string& type) const;

    wstring Resolve_AssetPath(const string& guid) const;
    wstring Get_AssetRoot() const;

    // 에디터/로더가 "GUID 없으면 생성" 흐름을 쓸 수 있게 하기 -> 프리펩이나 그런거 새로 만들 때
    string Ensure_AssetGUID(const wstring& filePath, const string& type = "") const;

public: /* For.Asset_Manager (access) */
    const Asset_Manager* Get_AssetManager() const { return _assetManager.get(); }
    Asset_Manager* Get_AssetManager() { return _assetManager.get(); }

public: /* For.Sound_Manager */
    bool Play_Sound(const wstring& soundFile, SoundChannel channel, float volume = 1.f);
    bool Play_Sound_Pitched(const wstring& soundFile, SoundChannel channel, float volume, float pitch);
    bool Play_BGM(
        const wstring& soundFile,
        float volume = 1.f,
        bool stopPrevBGM = true,
        float fadeOutDuration = 0.f);
    bool Play_LoopSound(
        const wstring& soundFile,
        SoundChannel channel,
        float volume = 1.f,
        bool stopPrevChannel = false,
        float fadeOutDuration = 0.f);
    void Stop_SoundChannel(SoundChannel channel, float fadeOutDuration = 0.f);
    void Stop_Sound(const wstring& soundFile);
    void Stop_AllSounds(float fadeOutDuration = 0.f);
    void Set_SoundChannelVolume(SoundChannel channel, float volume);
    float Get_SoundChannelVolume(SoundChannel channel) const;
    void Set_SoundChannelPaused(SoundChannel channel, bool paused);
    void Set_AllSoundMuted(bool muted);
    bool Is_AllSoundMuted() const;
    void Set_SoundGlobalPitch(float pitch, bool includeBGM = false);
    bool Has_Sound(const wstring& soundFile) const;
    const wstring& Get_CurrentBGM() const;

    // 사운드 큐
    bool Play_SoundCue(const wstring& cueId, const Vec3* worldPosition = nullptr);
    bool Play_SoundCue(const wstring& cueId, const Vec3* worldPosition, float volume, float pitch);
    bool Try_GetSoundCueDefaultVolumePitch(const wstring& cueId, float& outVolume, float& outPitch) const;
    // 3D 사운드의 기준은, 활성화된 카메라의 기준으로
    void Update_SoundListenerFromActiveCamera();

public: /* For.Debug Profile */
    // Profile View에서 현재 등록 prototype/object의 대략적인 리소스 메모리 사용량을 요약할 때 사용한다.
    bool Build_DebugResourceMemorySummary(DebugResourceMemorySummary& outSummary) const;

public: /* For.ShaderResourceRegistry */
    uint32 Get_Handle(const string& name) const;
    ID3D11ShaderResourceView* Get_SRV(uint32 handle) const;
    ID3D11ShaderResourceView* Get_SRV(const string& name) const;

    HRESULT Bind_ByHandle(ShaderCom* shader, const char* shaderVariableName, uint32 handle) const;
    HRESULT Bind_ByName(ShaderCom* shader, const char* shaderVariableName, const string& name) const;

public: /* For.Camera_Manager */
    HRESULT Add_Camera(Shared<Camera> camera) const;
    bool Is_ActiveCamera(Shared<Camera> camera);
    void Change_ActiveCamera(Shared<Camera> camera) const;
    void SwitchTo_NextCamera();

    bool Activate_CameraByType(ObjectType objectType);
    bool Is_ActiveCameraType(ObjectType objectType) const;
    bool Focus_FreeCameraOnPosition(const Vec3& position);

    // 게임 정지 시에도, 자유 카메라는 움직이게 timer 따로 설정
    void Set_FreeCameraTimeDelta(float timeDelta) { _freeCameraTimeDelta = timeDelta; }
    float Get_FreeCameraTimeDelta() const { return _freeCameraTimeDelta; }

    Shared<Camera> Get_Camera(int index) const;
    Shared<Camera> Get_ActiveCamera();
    int Get_CameraCount();
    Shared<Camera> Get_CameraByType(ObjectType objectType);

    HRESULT Bind_CameraFar(Shared<ShaderCom> shader, const char* constantName);

public: /* For.TypeRegistry */
    const TypeRegistry::ComponentTypeInfo* Find_ComponentType(const string& className) const;
    const TypeRegistry::GameObjectTypeInfo* Find_GameObjectType(const string& className) const;
    const TypeRegistry::BTNodeTypeInfo* Find_BTNodeType(const string& className) const;
    const TypeRegistry::AnimNotifyTypeInfo* Find_AnimNotifyType(const string& className) const;
    const TypeRegistry::AnimNotifyStateTypeInfo* Find_AnimNotifyStateType(const string& className) const;
    const TypeRegistry::ScreenFxTypeInfo* Find_ScreenFxType(const string& className) const;

    void Visit_GameObjectTypes(const function<void(const TypeRegistry::GameObjectTypeInfo&)>& visitor) const;
    void Visit_AnimNotifyTypes(const function<void(const TypeRegistry::AnimNotifyTypeInfo&)>& visitor) const;
    void Visit_AnimNotifyStateTypes(const function<void(const TypeRegistry::AnimNotifyStateTypeInfo&)>& visitor) const;
    void Visit_GlobalEnums(const function<void(const TypeRegistry::GlobalEnumInfo&)>& visitor) const;
    void Visit_ScreenFxTypes(const function<void(const TypeRegistry::ScreenFxTypeInfo&)>& visitor) const;

    Shared<Component> Create_ComponentPrototype(const string& className) const;
    Shared<GameObject> Create_GameObjectPrototype(const string& className) const;
    Shared<Base> Create_BTNodePrototype(const string& className) const;
    Shared<AnimNotify> Create_AnimNotifyPrototype(const string& className) const;
    Shared<AnimNotifyState> Create_AnimNotifyStatePrototype(const string& className) const;
    Shared<ScreenFx_Instance> Create_ScreenFxInstance(const string& className) const;

public : /* For.Target_Manager */
    HRESULT Ready_AllRenderTargets();
    HRESULT Add_RenderTarget(const wstring& targetTag, uint32 sizeX, uint32 sizeY, DXGI_FORMAT pixelFormat, const Color& clearColor, bool resizeWithViewport = true, bool useDSV = false, uint32 arraySize = 1);
    HRESULT Add_MRT(const wstring& mrtTag, const wstring& targetTag);

    HRESULT Begin_MRT(const wstring& mrtTag, bool useDSV = false, bool clearRTV = true, uint32 arraySlice = 0);
    HRESULT End_MRT();
    HRESULT Resize_MRTs(uint32 width, uint32 height);
    HRESULT Bind_RT_ShaderResource(Shared<ShaderCom> shader, const char* constantName, const wstring& targetTag);

    HRESULT Copy_CurrentRTV_To(const wstring& targetTag);
    HRESULT Copy_RTV(const wstring& srcTargetTag, const wstring& dstTargetTag);
    HRESULT Copy_RTV_ArraySlice(const wstring& srcTargetTag, uint32 arraySlice, const wstring& dstTargetTag);

    HRESULT Copy_RT_Resource(const wstring& targetTag, ID3D11Texture2D* destTexture);
    HRESULT Bind_PlayerStencilWriteState(bool depthWrite);
    HRESULT Bind_BackgroundUIBehindPlayerStencilState();
    HRESULT Bind_UINoDepthStencilState();

#ifdef _DEBUG
    ComPtr<ID3D11Texture2D> Get_RT_Texture(const wstring& targetTag);
    HRESULT Initialize_Debug(float sizeX, float sizeY);
    HRESULT Resize_DrawArea(float sizeX, float sizeY);
    HRESULT Display_RT(const wstring& targetTag, bool render);
    HRESULT Display_AllRTs(bool render);
    HRESULT Render_RT_Debug(Shared<VIBuffer_Rect> viBuffer, Shared<ShaderCom> shader);
    const map<wstring, bool>& Get_RTs() const;
    void Set_ShadowCascadeSliceDebugEnabled(bool enabled);
    bool Is_ShadowCascadeSliceDebugEnabled() const;
    void Set_ShadowCascadeProjectionDebugEnabled(bool enabled);
    bool Is_ShadowCascadeProjectionDebugEnabled() const;
    void Set_ShadowCascadeInfluenceDebugEnabled(bool enabled);
    bool Is_ShadowCascadeInfluenceDebugEnabled() const;
#endif

public: /* For.Shadow */
    HRESULT                 Bind_ShadowMatrices(Shared<ShaderCom> shader, const char* viewName, const char* projName);
    HRESULT                 Bind_ShadowLightDesc(Shared<ShaderCom> shader, const char* lightFarName, const char* biasName = nullptr, const char* intensityName = nullptr);
    HRESULT                 Bind_ShadowCB(Shared<ShaderCom> shader);
    HRESULT                 Set_ActiveShadowCascade(uint32 cascadeIndex);
    void                    Set_ShadowEnabled(bool enabled);
    uint32                  Get_ShadowCascadeCount() const;
    HRESULT                 Set_ShadowCascadeLevel(CascadeLevel level);
    CascadeLevel            Get_ShadowCascadeLevel() const;
    HRESULT                 Set_ShadowCascadePresets(const ShadowCascadePresetSet& presets);
    const ShadowCascadePresetSet* Get_ShadowCascadePresets() const;

    HRESULT Update_ShadowLight(const ShadowLightDesc& desc);
    const ShadowLightDesc* Get_ShadowLightDesc() const;
    const Matrix* Get_ShadowTransformMatrix(D3DTS eType) const;
    const ShadowCascadeDebugInfo* Get_ShadowCascadeDebugInfos() const;
    bool Is_Visible_In_ShadowInfluenceVolume(const BoundingBox& worldAABB, uint32 cascadeIndex) const;
    bool Is_Visible_In_ShadowSpace(const BoundingBox& worldAABB, const Matrix& viewProj) const;

    // 카메라가 플레이어 따라다니면서 그림자 위치 조절
    HRESULT Update_ShadowFromCameraFrustum(
        const ShadowLightDesc& desc,
        const Shared<Camera>& camera,
        const Vec3& lightDir);

public: /* For.Collision_Manager */
    void Add_Collider(Shared<ColliderCom> collider);
    void Add_Collider(Shared<ColliderCom> collider, const Matrix& worldMatrix);
    void Clear_Colliders();

public: // For. PhysX_Manager
    PxPhysics* Get_PhysXPhysics() const;
    PxScene* Get_PhysXScene() const;
    PhysX_Manager* Get_PhysXManager() const { return _physxManager.get(); }
    void Unregister_PhysXCollider(ColliderCom* collider);

    PhysXStaticActorHandle Register_PhysXStaticCollision(
        const StaticCollisionData& collisionData,
        const Matrix& worldMatrix,
        Collision_Preset preset);

    void Unregister_PhysXStaticActor(PhysXStaticActorHandle handle);

    bool Sweep_Collider(
        ColliderCom* collider,
        const Matrix& worldMatrix,
        const Vec3& direction,
        float distance,
        PhysXQueryHit& outHit) const;

    bool Sweep_Sphere(
        const Vec3& origin,
        const Vec3& direction,
        float distance,
        float radius,
        Collision_Preset queryPreset,
        PhysXQueryHit& outHit) const;

    bool Raycast_Environment(
        const Vec3& origin,
        const Vec3& direction,
        float distance,
        PhysXQueryHit& outHit) const;

    bool Probe_Ground(
        ColliderCom* collider,
        const Matrix& worldMatrix,
        float probeDistance,
        PhysXQueryHit& outHit) const;

public: // For. Culling_Manager
    void UpdateFrustum(const Matrix& view, const Matrix& proj);
    bool IsVisible(const BoundingBox& worldAABB) const;
    bool IsVisible(const BoundingBox& worldAABB, const Matrix& view, const Matrix& proj) const;
    void BuildOctree(const vector<BoundingBox>& worldAABBs);
    void QueryVisible(vector<uint32>& outIndices) const;

public: // For. BT_Node
    void Visit_BTNodeTypes(const function<void(const TypeRegistry::BTNodeTypeInfo&)>& visitor) const;

public: /* For.Debug_Manager */
    void Draw_DebugBox(const DebugBoxDesc& desc);
    void Draw_DebugSphere(const DebugSphereDesc& desc);
    void Draw_DebugCapsule(const DebugCapsuleDesc& desc);
    void Draw_DebugLine(const DebugLineDesc& desc);
    void Draw_DebugTraceLine(const DebugTraceLineDesc& desc);

    void Clear_DebugDraws();
    void Set_DebugRenderEnabled(bool enabled);

    bool Is_DebugRenderEnabled() const;

    HRESULT Render_DebugDepth();
    HRESULT Render_DebugOverlay();

public: /* For.Print */
    template <typename... Args>
    void Print(std::wstring_view format, Args&&... args)
    {
#ifdef _DEBUG
        if (!_printManager)
            return;

        _printManager->Print(format, std::forward<Args>(args)...);
#endif
    }

    void Clear_Prints();
    void Clear_Print(const wstring& slot);

public: /* 입력 제어 */
    void Set_PlayerInputBlocked(bool isBlocked) { _isPlayerInputBlocked = isBlocked; }
    bool Is_PlayerInputBlocked() const { return _isPlayerInputBlocked; }

    void Set_FreeCameraInputEnabled(bool enabled) { _isFreeCameraInputEnabled = enabled; }
    bool Is_FreeCameraInputEnabled() const { return _isFreeCameraInputEnabled; }

    void Set_GameplayInputMode(GameplayInputMode mode) { _gameplayInputMode = mode; }
    GameplayInputMode Get_GameplayInputMode() const { return _gameplayInputMode; }
    bool Is_WorldGameplayInputBlocked() const;
    bool Is_UIInputEnabled() const;

public: /* For.Json / Prefab Saved & Load */
    HRESULT Save_GameObjectJson(const fs::path& path, const Shared<GameObject>& gameObject);
    json Load_JsonFile(const fs::path& path) const;
    HRESULT Load_GameObjectJson(const fs::path& path, const Shared<GameObject>& gameObject) const;
    Shared<GameObject> Instantiate_GameObjectJson(const json& root);
    Shared<GameObject> Instantiate_GameObjectJson(const fs::path& path);
    HRESULT Create_PrefabAsset(const fs::path& path, const string& staticClass, const string& prefabName);

public: /* For.Picking */
    HRESULT Update_Picking();
    bool Pick_WorldPosition(const POINT& pixelPos, Vec3& outWorldPos);

public: /* For.Thread Pool */
    bool Enqueue_ThreadJob(function<void()> job);
    size_t Get_PendingThreadJobCount() const;

public: /* For.Delegate Hub */
    DelegateHub& Get_DelegateHub() { return _delegateHub; }
    const DelegateHub& Get_DelegateHub() const { return _delegateHub; }

public: /* For.Event Bus */
    Event_Bus& Get_EventBus() { return *_eventBus; }

public: /* ScreenFx Manager */
    void Play_ScreenFx(const Shared<class ScreenFx_Instance>& screenFX) const;

private:
    Unique<Log_Manager> _logManager{};
    Unique<Graphic_Device> _graphicDevice{};
    Unique<Timer_Manager> _timerManager{};
    Unique<Level_Manager> _levelManager{};
    Unique<Prototype_Manager> _prototypeManager{};
    Unique<UI_Manager> _uiManager{};
    Unique<Object_Manager> _objectManager{};
    Unique<Renderer> _renderer{};
    Unique<ScreenFx_Manager> _screenFxManager{};
    Unique<ShaderCom_Controller> _shaderComController{};
    Unique<PipeLine> _pipeLine{};
    Unique<Input_Manager> _inputManager{};
    Unique<Light_Manager> _lightManager{};
    Unique<Font_Manager> _fontManager{};
    Unique<Asset_Manager> _assetManager{}; /* GUID 기반 에셋 registry와 meta/cache를 관리하는 매니저 */
    Unique<Sound_Manager> _soundManager{};
    Unique<ShaderResourceRegistry> _shaderResourceRegistry{};
    Unique<Camera_Manager> _cameraManager{};
    Unique<TypeRegistry> _typeRegistry{};
    Unique<Target_Manager> _targetManager{};
    Unique<Debug_Manager> _debugManager{};
    Unique<Collision_Manager> _collisionManager{};
    Unique<Shadow> _shadow{};
    Unique<PhysX_Manager> _physxManager{};
    Unique<Picking> _picking{};
    Unique<Culling_Manager> _cullingManager{};
    Unique<ThreadPool> _threadPool{};
    Unique<Event_Bus> _eventBus{};

#ifdef _DEBUG
    Unique<Print_Manager> _printManager{};
    bool _shadowCascadeSliceDebugEnabled = false;
    bool _shadowCascadeProjectionDebugEnabled = false;
    bool _shadowCascadeInfluenceDebugEnabled = false;
    bool _hasShadowCascadeDebugSnapshot = false;
    ShadowCascadeDebugInfo _shadowCascadeDebugSnapshot[Shadow::CascadeCount]{};
    Matrix _shadowCascadeViewSnapshot[Shadow::CascadeCount]{};
#endif

private:
    ComPtr<Device> _device = {};
    ComPtr<Context> _context = {};
    ComPtr<ID3D11DepthStencilState> _playerStencilWriteState = {};
    ComPtr<ID3D11DepthStencilState> _playerStencilWriteDepthReadState = {};
    ComPtr<ID3D11DepthStencilState> _backgroundUIBehindPlayerStencilState = {};
    ComPtr<ID3D11DepthStencilState> _uiNoDepthStencilState = {};

    uint32 _viewportWidth{ 0 };
    uint32 _viewportHeight{ 0 };

    bool _isPlayerInputBlocked = false;
    bool _isFreeCameraInputEnabled = false;

    bool _isGameplayPaused = false;
    GameplayInputMode _gameplayInputMode = GameplayInputMode::Normal;

    // 월드 시간 제어
    float _worldTimeScale = 1.f;
    float _unscaledTimeDelta = 0.f;

#ifdef _DEBUG
private:
    void Draw_ShadowCascadeWorldDebug();
    void Capture_ShadowCascadeDebugSnapshot();
#endif
    float _worldTimeDelta = 0.f;
    bool _isWorldTimeSlowActive = false;
    float _worldTimeSlowElapsed = 0.f;
    float _worldTimeSlowDuration = 0.f;
    float _worldTimeScaleBeforeSlow = 1.f;

    // Free 카메라용 time
    float _freeCameraTimeDelta = 0.f;

    DelegateHub _delegateHub = {};

#ifdef _DEBUG

private: //## 디버그 성능 평가용
    static constexpr float runtimePerfSampleWindowSec{ DebugPerf::kRuntimePerfLogWindowSec }; // RuntimePerfDetail을 몇 초마다 출력할건지

    chrono::steady_clock::time_point _runtimePerfWindowStartTime{};
    chrono::steady_clock::time_point _runtimePerfLastSampleTime{};
    bool _hasRuntimePerfWindowStartTime{ false };
    bool _hasRuntimePerfLastSampleTime{ false };
    uint32 _runtimePerfObservedFrameCount{ 0 };

private:
    UpdatePerfWindow _updatePerfWindow{};

    void Accumulate_UpdatePerf(
        double updateMs,
        double debugMgrMs,
        double inputMgrMs,
        double printMgrMs,
        double clientMs,
        double levelMs,
        double priorityObjMs,
        double pipelineMs,
        double objMs,
        double physxBeginMs,
        double collClearMs,
        double lateObjMs,
        double physxUpdateMs,
        double physxGatherMs,
        double collFromPhysxMs,
        double cameraMs,
        double uiMs);

    bool Try_ConsumeUpdatePerfSummary(float windowSec, UpdatePerfSummary& outSummary);
    bool Should_SampleRuntimePerf();
    void Reset_RuntimePerfWindowState(chrono::steady_clock::time_point now);
    void Try_FlushRuntimePerfLog();
#endif

public:
    void Release_Engine();
    void Free() override;
};

NS_END
