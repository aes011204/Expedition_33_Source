#include "GameInstance.h"
#include "Asset_Manager.h"
#include "Camera_Manager.h"
#include "Camera.h"
#include "Collision_Manager.h"
#include "PhysX_Manager.h"
#include "Debug_Manager.h"
#include "Font_Manager.h"
#include "Graphic_Device.h"
#include "Input_Manager.h"
#include "Level_Manager.h"
#include "Light_Manager.h"
#include "Log_Manager.h"
#include "UI_Manager.h"
#include "Object_Manager.h"
#include "PipeLine.h"
#include "GameObject.h"
#include "Prototype_Manager.h"
#include "RegistorFactory.h"
#include "Renderer.h"
#include "Timer_Manager.h"
#include "Target_Manager.h"
#include "Shadow.h"
#include "Picking.h"
#include "ColliderCom.h"
#include "Culling_Manager.h"
#include "ThreadPool.h"

#ifdef _DEBUG
#include "Print_Manager.h"
#endif

IMPLEMENT_SINGLETON(GameInstance)

GameInstance::GameInstance()
{
}

GameInstance::~GameInstance() = default;

HRESULT GameInstance::Initialize_Engine(const ENGINE_DESC& engineDesc, ComPtr<Device>& device, ComPtr<Context>& context)
{
    _graphicDevice = Graphic_Device::Create(
        engineDesc.hWnd,
        engineDesc.winMode,
        engineDesc.viewportWidth,
        engineDesc.viewportHeight,
        device,
        context
    );

    _device = device;
    _context = context;

    CHECK_NULL(_graphicDevice, E_FAIL);

    _viewportWidth = engineDesc.viewportWidth;
    _viewportHeight = engineDesc.viewportHeight;

    _logManager = Log_Manager::Create(engineDesc.projectName);
    CHECK_NULL(_logManager, E_FAIL);

    _timerManager = Timer_Manager::Create();
    CHECK_NULL(_timerManager, E_FAIL);

    _levelManager = Level_Manager::Create();
    CHECK_NULL(_levelManager, E_FAIL);

    _prototypeManager = Prototype_Manager::Create(engineDesc.numLevels);
    CHECK_NULL(_prototypeManager, E_FAIL);

    _uiManager = UI_Manager::Create(_prototypeManager.get());
    CHECK_NULL(_uiManager, E_FAIL);

    _typeRegistry = TypeRegistry::Create();
    CHECK_NULL(_typeRegistry, E_FAIL);

    if (FAILED(Flush_PendingTypeRegistrations(*_typeRegistry)))
        return E_FAIL;

    _objectManager = Object_Manager::Create(engineDesc.numLevels);
    CHECK_NULL(_objectManager, E_FAIL);

    _targetManager = Target_Manager::Create(device, context);
    CHECK_NULL(_targetManager, E_FAIL);

    _renderer = Renderer::Create(device, context);
    CHECK_NULL(_renderer, E_FAIL);

    _pipeLine = PipeLine::Create();
    CHECK_NULL(_pipeLine, E_FAIL);

    _inputManager = Input_Manager::Create(engineDesc.hWnd);
    CHECK_NULL(_inputManager, E_FAIL);

    _lightManager = Light_Manager::Create();
    CHECK_NULL(_lightManager, E_FAIL);

    _fontManager = Font_Manager::Create(device, context);
    CHECK_NULL(_fontManager, E_FAIL);

    _cameraManager = Camera_Manager::Create();
    CHECK_NULL(_cameraManager, E_FAIL);

    _debugManager = Debug_Manager::Create(device, context);
    CHECK_NULL(_debugManager, E_FAIL);

    _collisionManager = Collision_Manager::Create();
    CHECK_NULL(_collisionManager, E_FAIL);

    _cullingManager = Culling_Manager::Create();
    CHECK_NULL(_cullingManager, E_FAIL);

    _shadow = Shadow::Create();
    CHECK_NULL(_shadow, E_FAIL);

    _physxManager = PhysX_Manager::Create();
    CHECK_NULL(_physxManager, E_FAIL);

    _picking = Picking::Create(device, context);
    CHECK_NULL(_picking, E_FAIL);

    // Thread Pool
    {
        // PC가 동시에 돌릴 수 있는 하드웨어 스레드 수 가져오기
        const size_t hardwareThreadCount = thread::hardware_concurrency();
        // 메인 스레드도 게임 업데이트/렌더링을 해야하니까 CPU를 전부 worker한테 넘기는게 아니라 하나 뺴기
        const size_t workerCount = max<size_t>(1, hardwareThreadCount > 1 ? hardwareThreadCount - 1 : 1);

        _threadPool = ThreadPool::Create(workerCount);
        CHECK_NULL(_threadPool, E_FAIL);

        LOG_ERROR("[GameInstance::Initialize_Engine] ThreadPool created. workerCount={}",
        static_cast<int>(workerCount));
    }
    


#ifdef _DEBUG
    _printManager = Print_Manager::Create();
    CHECK_NULL(_printManager, E_FAIL);
#endif

    return S_OK;
}

void GameInstance::Update_Engine(float timeDelta)
{
    Update_Engine(timeDelta, nullptr);
}

void GameInstance::Update_Engine(float timeDelta, const function<void(float)>& clientUpdate)
{
    _debugManager->Update(timeDelta);
    _inputManager->Update(timeDelta);
#ifdef _DEBUG
    _printManager->Update(timeDelta);
#endif

    // 입력 갱신 직후, 레벨/오브젝트/UI 업데이트 전에 클라이언트쪽 런타임 갱신을 위해
    if (clientUpdate)
    {
        clientUpdate(timeDelta);
    }

    _levelManager->Update(timeDelta);

    // UI 키면서 게임 멈출 때 Update 안돌릴 애들
    if (!_isGameplayPaused)
    {
        _objectManager->Priority_Update(timeDelta);

        _pipeLine->Update();

        _objectManager->Update(timeDelta);

        _physxManager->Begin_Frame();
        _collisionManager->Clear_Colliders();
        _objectManager->Late_Update(timeDelta);

        _physxManager->Update(timeDelta);

        vector<PhysXPairEvent> events;
        _physxManager->Gather_FrameEvents(events);
        _collisionManager->Update_FromPhysX(events);

        _cameraManager->Update(timeDelta);
    }
    
    _uiManager->Update(timeDelta);
}

HRESULT GameInstance::Draw()
{
    _renderer->Draw();

    _levelManager->Render();

    //CHECK_FAILED(Update_Picking(), E_FAIL);

#ifdef _DEBUG
    // Collision Manager
    {
        _collisionManager->Render_Debug();
    }

    // Debug Manager
    {
        _debugManager->Render_Depth();
        _debugManager->Render_Overlay();
    }

    _printManager->Render();
#endif

#ifdef _DEBUG
    Try_FlushRuntimePerfLog();
#endif

    if (FAILED(_uiManager->Render()))
        return E_FAIL;

    return S_OK;
}

HRESULT GameInstance::Draw_RendererOnly()
{
    _renderer->Draw();

    return S_OK;
}

#ifdef _DEBUG
void GameInstance::Try_FlushRuntimePerfLog()
{
    const auto now = chrono::steady_clock::now();
    if (!_hasRuntimePerfWindowStartTime)
    {
        _runtimePerfWindowStartTime = now;
        _hasRuntimePerfWindowStartTime = true;
        return;
    }

    const float windowSec = static_cast<float>(
        chrono::duration<double>(now - _runtimePerfWindowStartTime).count());

    if (windowSec < runtimePerfSampleWindowSec)
        return;

    RuntimePerfSummary runtimeSummary{};
    runtimeSummary.windowSec = windowSec;

    const bool hasRenderSummary =
        nullptr != _renderer &&
        _renderer->Try_ConsumePerfSummary(windowSec, runtimeSummary.render);

    const bool hasCollisionSummary =
        nullptr != _collisionManager &&
        _collisionManager->Try_ConsumePerfSummary(windowSec, runtimeSummary.collision);

    if (!hasRenderSummary && !hasCollisionSummary)
    {
        _runtimePerfWindowStartTime = now;
        _hasRuntimePerfWindowStartTime = true;
        return;
    }

    Log_FileOnly(
        spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION },
        spdlog::level::trace,
        fmt::format(
            "[RuntimePerfDetail] render_ms(avg={:.2f} max={:.2f}) "
            "priority_ms(avg={:.2f} max={:.2f}) "
            "shadow_ms(avg={:.2f} max={:.2f}) "
            "nonblend_ms(avg={:.2f} max={:.2f}) "
            "lights_ms(avg={:.2f} max={:.2f}) "
            "combined_ms(avg={:.2f} max={:.2f}) "
            "nonlight_ms(avg={:.2f} max={:.2f}) "
            "blend_ms(avg={:.2f} max={:.2f}) "
            "ui_ms(avg={:.2f} max={:.2f}) "
            "debug_ms(avg={:.2f} max={:.2f}) "
            "collision_ms(avg={:.2f} max={:.2f}) pairs={:.1f}/s intersects={:.1f}/s hits={:.1f}/s",

            runtimeSummary.render.renderAvgMs,
            runtimeSummary.render.renderMaxMs,

            runtimeSummary.render.priorityAvgMs,
            runtimeSummary.render.priorityMaxMs,

            runtimeSummary.render.shadowAvgMs,
            runtimeSummary.render.shadowMaxMs,

            runtimeSummary.render.nonBlendAvgMs,
            runtimeSummary.render.nonBlendMaxMs,

            runtimeSummary.render.lightsAvgMs,
            runtimeSummary.render.lightsMaxMs,

            runtimeSummary.render.combinedAvgMs,
            runtimeSummary.render.combinedMaxMs,

            runtimeSummary.render.nonLightAvgMs,
            runtimeSummary.render.nonLightMaxMs,

            runtimeSummary.render.blendAvgMs,
            runtimeSummary.render.blendMaxMs,

            runtimeSummary.render.uiAvgMs,
            runtimeSummary.render.uiMaxMs,

            runtimeSummary.render.debugAvgMs,
            runtimeSummary.render.debugMaxMs,

            runtimeSummary.collision.updateAvgMs,
            runtimeSummary.collision.updateMaxMs,
            runtimeSummary.collision.pairsPerSec,
            runtimeSummary.collision.intersectsPerSec,
            runtimeSummary.collision.hitsPerSec
        )
    );

    _runtimePerfWindowStartTime = now;
    _hasRuntimePerfWindowStartTime = true;
}

#endif

void GameInstance::Clear_Resources(uint32 levelIndex)
{
    _uiManager->Clear_Level(levelIndex);
    _objectManager->Visit_LevelObjects(levelIndex,
        [this](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            UNREFERENCED_PARAMETER(layerTag);

            Shared<Camera> camera = dynamic_pointer_cast<Camera>(gameObject);
            if (camera != nullptr)
                _cameraManager->Remove_Camera(camera);
        });

    _objectManager->Clear_Layers(levelIndex);
    _prototypeManager->Clear_Prototypes(levelIndex);
}

float GameInstance::Random(float min, float max)
{
    return min + static_cast<float>(rand()) / RAND_MAX * (max - min);
}

void GameInstance::Log_FileOnly(const spdlog::source_loc& sourceLoc, spdlog::level::level_enum level, const string& message) const
{
    CHECK_NULL(_logManager);
    _logManager->Log_FileOnly(sourceLoc, level, message);
}

void GameInstance::Visit_ConsoleLogs(const function<void(const ConsoleLogEntry& logEntry)>& visitor) const
{
    CHECK_NULL(_logManager);
    _logManager->Visit_ConsoleLogs(visitor);
}

void GameInstance::Clear_ConsoleLogs()
{
    CHECK_NULL(_logManager);
    _logManager->Clear_ConsoleLogs();
}

uint32 GameInstance::Get_ConsoleLogCount() const
{
    CHECK_NULL(_logManager, 0);
    return _logManager->Get_ConsoleLogCount();
}

uint32 GameInstance::Get_ConsoleLogCapacity() const
{
    CHECK_NULL(_logManager, 0);
    return _logManager->Get_ConsoleLogCapacity();
}

void GameInstance::Set_ConsoleLogCapacity(uint32 capacity)
{
    CHECK_NULL(_logManager);
    _logManager->Set_ConsoleLogCapacity(capacity);
}

uint32 GameInstance::Get_ConsoleLogRevision() const
{
    CHECK_NULL(_logManager, 0);
    return _logManager->Get_ConsoleLogRevision();
}

HRESULT GameInstance::Clear_Buffers(const Vec4& clearColor)
{
    if (FAILED(_graphicDevice->Clear_BackBuffer_View(clearColor)))
        return E_FAIL;
    if (FAILED(_graphicDevice->Clear_DepthStencil_View()))
        return E_FAIL;

    return S_OK;
}

HRESULT GameInstance::Present()
{
    return _graphicDevice->Present();
}

void GameInstance::Bind_BackBuffer()
{
    return _graphicDevice->Bind_BackBuffer();
}

HRESULT GameInstance::Resize_BackBuffer(uint32 width, uint32 height)
{
    return _graphicDevice->Resize(width, height);
}

ComPtr<SwapChain> GameInstance::Get_SwapChain() const
{
    return _graphicDevice->Get_SwapChain();
}

void GameInstance::Set_ViewportSize(uint32 width, uint32 height)
{
    _viewportWidth = max(1u, width);
    _viewportHeight = max(1u, height);
}

HRESULT GameInstance::Apply_LogicalViewportSize(uint32 width, uint32 height)
{
    CHECK_NULL(_renderer, E_FAIL);
    CHECK_NULL(_targetManager, E_FAIL);

    const uint32 backBufferWidth = max(1u, Get_BackBufferWidth());
    const uint32 backBufferHeight = max(1u, Get_BackBufferHeight());
    const uint32 logicalWidth = min(max(1u, width), backBufferWidth);
    const uint32 logicalHeight = min(max(1u, height), backBufferHeight);

    Set_ViewportSize(logicalWidth, logicalHeight);

    CHECK_FAILED(Resize_MRTs(logicalWidth, logicalHeight), E_FAIL);

    _renderer->Sync_ViewportDependentState(logicalWidth, logicalHeight);
    return S_OK;
}

uint32 GameInstance::Get_ViewportWidth() const
{
    return _viewportWidth;
}

uint32 GameInstance::Get_ViewportHeight() const
{
    return _viewportHeight;
}

uint32 GameInstance::Get_BackBufferWidth() const
{
    return _graphicDevice->Get_ViewportWidth();
}

uint32 GameInstance::Get_BackBufferHeight() const
{
    return _graphicDevice->Get_ViewportHeight();
}

HRESULT GameInstance::Add_Timer(const wstring& timerTag)
{
    return _timerManager->Add_Timer(timerTag);
}

float GameInstance::Compute_TimeDelta(const wstring& timerTag)
{
    return _timerManager->Compute_TimeDelta(timerTag);
}

HRESULT GameInstance::Change_Level(uint32 newLevelIndex, Shared<Level> newLevel)
{
    return _levelManager->Change_Level(newLevelIndex, newLevel);
}

uint32 GameInstance::Current_LevelIndex() const
{
    return _levelManager->Get_CurrentLevelIndex();
}

Shared<Level> GameInstance::Current_LevelType() const
{
    return _levelManager->Get_CurrentLevelType();
}

HRESULT GameInstance::Add_Prototype(uint32 levelIndex, const wstring& prototypeTag, Shared<Base> prototype)
{
    return _prototypeManager->Add_Prototype(levelIndex, prototypeTag, prototype);
}

Shared<Base> GameInstance::Clone_Prototype(Prototype prototype, uint32 levelIndex, const wstring& prototypeTag, void* arg)
{
    return _prototypeManager->Clone_Prototype(prototype, levelIndex, prototypeTag, arg);
}

bool GameInstance::Has_Prototype(uint32 levelIndex, const wstring& prototypeTag) const
{
    return _prototypeManager->Has_Prototype(levelIndex, prototypeTag);
}

HRESULT GameInstance::Add_UI(uint32 prototypeLevelIndex, const wstring& prototypeTag, void* arg, Shared<UI_Base>* outUI)
{
    return _uiManager->Add_UI(prototypeLevelIndex, prototypeTag, arg, outUI);
}

void GameInstance::Pop_UI()
{
    _uiManager->Pop_UI();
}

HRESULT GameInstance::Add_GameObject(
    uint32 prototypeLevelIndex, const wstring& prototypeTag, uint32 layerLevelIndex, const wstring& layerTag, void* arg)
{
    return _objectManager->Add_GameObject(prototypeLevelIndex, prototypeTag, layerLevelIndex, layerTag, arg);
}

HRESULT GameInstance::Add_GameObject(uint32 prototypeLevelIndex, const wstring& prototypeTag, uint32 layerLevelIndex,
                                     const wstring& layerTag, Shared<GameObject>& outGameObject, void* arg)
{
    return _objectManager->Add_GameObject(prototypeLevelIndex, prototypeTag, layerLevelIndex, layerTag, outGameObject, arg);
}

HRESULT GameInstance::Add_GameObject(uint32 layerLevelIndex, const wstring& layerTag,
                                     const Shared<GameObject>& gameObject)
{
    return _objectManager->Add_GameObject(layerLevelIndex, layerTag, gameObject);
}

bool GameInstance::Remove_GameObject(uint32 levelIndex, const Shared<GameObject>& gameObject)
{
    Shared<Camera> camera = dynamic_pointer_cast<Camera>(gameObject);
    if (camera != nullptr)
        _cameraManager->Remove_Camera(camera);

    return _objectManager->Remove_GameObject(levelIndex, gameObject);
}

HRESULT GameInstance::Clear_LevelObjects(uint32 levelIndex)
{
    _objectManager->Visit_LevelObjects(levelIndex,
        [this](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            UNREFERENCED_PARAMETER(layerTag);

            Shared<Camera> camera = dynamic_pointer_cast<Camera>(gameObject);
            if (camera != nullptr)
                _cameraManager->Remove_Camera(camera);
        });

    return _objectManager->Clear_Layers(levelIndex);
}

Shared<GameObject> GameInstance::Find_FirstObject(uint32 levelIndex, const wstring& layerTag)
{
    return _objectManager->Find_FirstObject(levelIndex, layerTag);
}

void GameInstance::Visit_LevelObjects(uint32 levelIndex,
    const function<void(const wstring& layerTag, const Shared<GameObject>& gameObject)>& visitor) const
{
    return _objectManager->Visit_LevelObjects(levelIndex, visitor);
}

void GameInstance::Visit_CurrentLevelObjects(const function<void(const wstring& layerTag, const Shared<GameObject>& gameObject)>& visitor) const
{
    return _objectManager->Visit_LevelObjects(visitor);
}

uint64 GameInstance::Get_CurrentLevelObjectRevision() const
{
    return _objectManager->Get_ObjectRevision();
}

void GameInstance::Add_RenderGroup(RenderGroup renderGroup, Shared<GameObject> gameObject)
{
    return _renderer->Add_RenderGroup(renderGroup, gameObject);
}

uint32 GameInstance::Get_RenderCall() const
{
    CHECK_NULL(_renderer, 0);
    return _renderer->Get_RenderCall();
}

HRESULT GameInstance::Bind_CamPosition(ShaderCom* shader, const char* constantName)
{
    return _pipeLine->Bind_CamPosition(shader, constantName);
}

HRESULT GameInstance::Bind_TransformMatrix(D3DTS transformState, ShaderCom* shader, const char* constantName)
{
    return _pipeLine->Bind_TransformMatrix(transformState, shader, constantName);
}

HRESULT GameInstance::Bind_TransformMatrix_Inverse(D3DTS transformState, ShaderCom* shader, const char* constantName)
{
    return _pipeLine->Bind_TransformMatrix_Inverse(transformState, shader, constantName);
}

const Matrix* GameInstance::Get_Transform(D3DTS transformState) const
{
    return _pipeLine->Get_Transform(transformState);
}

const Matrix* GameInstance::Get_Transform_Inverse(D3DTS transformState) const
{
    return _pipeLine->Get_Transform_Inverse(transformState);
}

const Vec4* GameInstance::Get_CamPosition() const
{
    return _pipeLine->Get_CamPosition();
}

void GameInstance::Set_Transform(D3DTS transformState, FXMMATRIX& transformStateMatrix)
{
    _pipeLine->Set_Transform(transformState, transformStateMatrix);
}

HRESULT GameInstance::Bind_CameraCB(Shared<ShaderCom> shader) const
{
    CHECK_NULL(shader, E_FAIL);
    CHECK_NULL(_pipeLine, E_FAIL);
    CHECK_NULL(_cameraManager, E_FAIL);

    Shared<Camera> activeCamera = _cameraManager->Get_ActiveCamera();
    CHECK_NULL(activeCamera, E_FAIL);

    return _pipeLine->Bind_CameraCB(shader.get(), activeCamera->Get_Far());
}

bool GameInstance::KeyPress(KEY_TYPE key) const
{
    return _inputManager->KeyPress(key);
}

bool GameInstance::KeyDown(KEY_TYPE key) const
{
    return _inputManager->KeyDown(key);
}

bool GameInstance::KeyUp(KEY_TYPE key) const
{
    return _inputManager->KeyUp(key);
}

Vec2 GameInstance::Get_MouseDelta() const
{
    return _inputManager->Get_MouseDelta();
}

float GameInstance::Get_MouseWheel() const
{
    return _inputManager->Get_MouseWheel();
}

void GameInstance::Set_MouseWheel(float mouseWheel)
{
    return _inputManager->Set_MouseWheel(mouseWheel);
}

void GameInstance::Lock_Mouse()
{
    return _inputManager->Lock_Mouse();
}

void GameInstance::Unlock_Mouse()
{
    return _inputManager->Unlock_Mouse();
}

void GameInstance::Begin_EditorMouseLockSession()
{
    return _inputManager->Begin_EditorMouseLockSession();
}

void GameInstance::End_EditorMouseLockSession()
{
    return _inputManager->End_EditorMouseLockSession();
}

void GameInstance::Set_MouseLockOverrideRect(const RECT& lockRectScreen)
{
    return _inputManager->Set_MouseLockOverrideRect(lockRectScreen);
}

void GameInstance::Clear_MouseLockOverrideRect()
{
    return _inputManager->Clear_MouseLockOverrideRect();
}

void GameInstance::Set_InputBlocked(bool isBlocked)
{
    return _inputManager->Set_InputBlocked(isBlocked);
}

const POINT& GameInstance::Get_MousePosition() const
{
    return _inputManager->Get_MousePosition();
}

const LIGHT_DESC* GameInstance::Get_LightDesc(uint32 index)
{
    return _lightManager->Get_LightDesc(index);
}

uint32 GameInstance::Get_LightCount() const
{
    return _lightManager->Get_LightCount();
}

HRESULT GameInstance::Add_Light(const LIGHT_DESC& lightDesc)
{
    return _lightManager->Add_Light(lightDesc);
}

HRESULT GameInstance::Render_Lights(Shared<ShaderCom> shader, Shared<VIBuffer_Rect> viBuffer)
{
    return _lightManager->Render_Lights(shader, viBuffer);
}

void GameInstance::Clear_Lights()
{
    _lightManager->Clear_Lights();
}

void GameInstance::Get_LightDescs(vector<LIGHT_DESC>& outLightDescs) const
{
    _lightManager->Get_LightDescs(outLightDescs);
}

HRESULT GameInstance::Set_Lights(const vector<LIGHT_DESC>& lightDescs)
{
    return _lightManager->Set_Lights(lightDescs);
}

HRESULT GameInstance::Add_Font(const wstring& fontTag, const wchar_t* fontFilePath)
{
    return _fontManager->Add_Font(fontTag, fontFilePath);
}

Vec2 GameInstance::Measure_Font(const wstring& fontTag, const wchar_t* text)
{
    return _fontManager->Measure(fontTag, text);
}

void GameInstance::Draw_Font(
    const wstring& fontTag,
    const wchar_t* text,
    const Vec2& position,
    FXMVECTOR color,
    float rotationRadians,
    const Vec2& pivot,
    float scale)
{
    _fontManager->Draw(fontTag, text, position, color, rotationRadians, pivot, scale);
}

HRESULT GameInstance::Initialize_AssetManager(const wstring& resourceRoot)
{
    _assetManager = Asset_Manager::Create(resourceRoot);
    if (nullptr == _assetManager)
        return E_FAIL;

    return S_OK;
}

string GameInstance::Find_AssetGUID(const wstring& filePath) const
{
    if (nullptr == _assetManager)
        return {};

    return _assetManager->Find_GUID(filePath);
}

const AssetMeta* GameInstance::Find_AssetByGUID(const string& guid) const
{
    if (nullptr == _assetManager)
        return nullptr;

    return _assetManager->Find_ByGUID(guid);
}

vector<AssetMeta> GameInstance::Get_AssetsByType(const string& type) const
{
    if (nullptr == _assetManager)
        return {};

    return _assetManager->Get_AssetsByType(type);
}

wstring GameInstance::Resolve_AssetPath(const string& guid) const
{
    if (nullptr == _assetManager)
        return {};

    return _assetManager->Resolve_Path(guid);
}

wstring GameInstance::Get_AssetRoot() const
{
    if (nullptr == _assetManager)
        return {};

    return _assetManager->Get_ResourceRoot();
}

string GameInstance::Ensure_AssetGUID(const wstring& filePath, const string& type) const
{
    if (nullptr == _assetManager)
        return {};

    const string existingGuid = _assetManager->Find_GUID(filePath);
    if (!existingGuid.empty())
        return existingGuid;

    return _assetManager->Register_Asset(filePath, type);
}

HRESULT GameInstance::Add_Camera(Shared<Camera> camera) const
{
    return _cameraManager->Add_Camera(camera);
}

bool GameInstance::Is_ActiveCamera(Shared<Camera> camera)
{
    return _cameraManager->Is_ActiveCamera(camera);
}

void GameInstance::Change_ActiveCamera(Shared<Camera> camera) const
{
    return _cameraManager->Change_ActiveCamera(camera);
}

void GameInstance::SwitchTo_NextCamera()
{
    return _cameraManager->SwitchTo_NextCamera();
}

bool GameInstance::Activate_CameraByType(ObjectType objectType)
{
    return _cameraManager->Activate_CameraByType(objectType);
}

bool GameInstance::Is_ActiveCameraType(ObjectType objectType) const
{
    return _cameraManager->Is_ActiveCameraType(objectType);
}

bool GameInstance::Focus_FreeCameraOnPosition(const Vec3& position)
{
    return _cameraManager->Focus_FreeCameraOnPosition(position);
}

Shared<Camera> GameInstance::Get_Camera(int index) const
{
    return _cameraManager->Get_Camera(index);
}

Shared<Camera> GameInstance::Get_ActiveCamera()
{
    return _cameraManager->Get_ActiveCamera();
}

int GameInstance::Get_CameraCount()
{
    return _cameraManager->Get_CameraCount();
}

HRESULT GameInstance::Bind_CameraFar(Shared<ShaderCom> shader, const char* constantName)
{
    return _cameraManager->Bind_CameraFar(shader, constantName);
}


const TypeRegistry::ComponentTypeInfo* GameInstance::Find_ComponentType(const string& className) const
{
    return _typeRegistry->Find_ComponentType(className);
}

const TypeRegistry::GameObjectTypeInfo* GameInstance::Find_GameObjectType(const string& className) const
{
    return _typeRegistry->Find_GameObjectType(className);
}

const TypeRegistry::BTNodeTypeInfo* GameInstance::Find_BTNodeType(const string& className) const
{
    return _typeRegistry->Find_BTNodeType(className);
}

const TypeRegistry::AnimNotifyTypeInfo* GameInstance::Find_AnimNotifyType(const string& className) const
{
    return _typeRegistry->Find_AnimNotifyType(className);
}

const TypeRegistry::AnimNotifyStateTypeInfo* GameInstance::Find_AnimNotifyStateType(const string& className) const
{
    return _typeRegistry->Find_AnimNotifyStateType(className);
}

void GameInstance::Visit_GameObjectTypes(const function<void(const TypeRegistry::GameObjectTypeInfo&)>& visitor) const
{
    CHECK_NULL(_typeRegistry);
    _typeRegistry->Visit_GameObjectTypes(visitor);
}

void GameInstance::Visit_AnimNotifyTypes(const function<void(const TypeRegistry::AnimNotifyTypeInfo&)>& visitor) const
{
    return _typeRegistry->Visit_AnimNotifyTypes(visitor);
}

void GameInstance::Visit_AnimNotifyStateTypes(
    const function<void(const TypeRegistry::AnimNotifyStateTypeInfo&)>& visitor) const
{
    return _typeRegistry->Visit_AnimNotifyStateTypes(visitor);
}

Shared<Component> GameInstance::Create_ComponentPrototype(const string& className) const
{
    return _typeRegistry->Create_Component(className, _graphicDevice->Get_Device(), _graphicDevice->Get_Context());
}

Shared<GameObject> GameInstance::Create_GameObjectPrototype(const string& className) const
{
    return _typeRegistry->Create_GameObject(className, _graphicDevice->Get_Device(), _graphicDevice->Get_Context());
}

Shared<Base> GameInstance::Create_BTNodePrototype(const string& className) const
{
    return _typeRegistry->Create_BTNode(className);
}

Shared<AnimNotify> GameInstance::Create_AnimNotifyPrototype(const string& className) const
{
    return _typeRegistry->Create_AnimNotify(className);
}

Shared<AnimNotifyState> GameInstance::Create_AnimNotifyStatePrototype(const string& className) const
{
    return _typeRegistry->Create_AnimNotifyState(className);
}

HRESULT GameInstance::Add_RenderTarget(const wstring& targetTag, uint32 sizeX, uint32 sizeY, DXGI_FORMAT pixelFormat,
                                       const Color& clearColor, bool resizeWithViewport, bool useDSV)
{
    return _targetManager->Add_RenderTarget(targetTag, sizeX, sizeY, pixelFormat, clearColor, resizeWithViewport, useDSV);
}

HRESULT GameInstance::Add_MRT(const wstring& mrtTag, const wstring& targetTag)
{
    return _targetManager->Add_MRT(mrtTag, targetTag);
}

HRESULT GameInstance::Begin_MRT(const wstring& mrtTag, bool useDSV)
{
    return _targetManager->Begin_MRT(mrtTag, useDSV);
}

HRESULT GameInstance::End_MRT()
{
    return _targetManager->End_MRT();
}

HRESULT GameInstance::Resize_MRTs(uint32 width, uint32 height)
{
    return _targetManager->Resize_MRTs(width, height);
}

HRESULT GameInstance::Bind_RT_ShaderResource(Shared<ShaderCom> shader, const char* constantName, const wstring& targetTag)
{
    return _targetManager->Bind_ShaderResource(shader, constantName, targetTag);
}

HRESULT GameInstance::Copy_CurrentRTV_To(const wstring& targetTag)
{
    return _targetManager->Copy_CurrentRTV_To(targetTag);
}

HRESULT GameInstance::Copy_RTV(const wstring& srcTargetTag, const wstring& dstTargetTag)
{
    return _targetManager->Copy_RTV(srcTargetTag, dstTargetTag);
}

HRESULT GameInstance::Copy_RT_Resource(const wstring& targetTag, ID3D11Texture2D* destTexture)
{
    return _targetManager->Copy_Resource(targetTag, destTexture);
}

void GameInstance::Add_Collider(Shared<ColliderCom> collider)
{
    CHECK_NULL(collider);
    CHECK_NULL(_collisionManager);
    CHECK_NULL(_physxManager);

    _collisionManager->Add_Collider(collider);

    Shared<GameObject> owner = collider->Get_Owner();
    CHECK_NULL(owner);

    Shared<TransformCom> transform = owner->Get_Transform();
    CHECK_NULL(transform);

    _physxManager->Register_OrSync_Collider(collider, transform->Get_WorldMatrix());
}

void GameInstance::Clear_Colliders()
{
    return _collisionManager->Clear_Colliders();
}

PxPhysics* GameInstance::Get_PhysXPhysics() const
{
    CHECK_NULL(_physxManager, nullptr);
    return _physxManager->Get_Physics();
}

PxScene* GameInstance::Get_PhysXScene() const
{
    CHECK_NULL(_physxManager, nullptr);
    return _physxManager->Get_Scene();
}

void GameInstance::UpdateFrustum(const Matrix& view, const Matrix& proj)
{
    if (_cullingManager)
    {
        _cullingManager->UpdateFrustum(view, proj);
    }
}

bool GameInstance::IsVisible(const BoundingBox& worldAABB) const
{
    if (_cullingManager)
    {
        return _cullingManager->IsVisible(worldAABB);
    }

    return true;
}

void GameInstance::BuildOctree(const vector<BoundingBox>& worldAABBs)
{
    if (_cullingManager)
    {
        _cullingManager->BuildOctree(worldAABBs);
    }
}

void GameInstance::QueryVisible(vector<uint32>& outIndices) const
{
    if (_cullingManager)
    {
        _cullingManager->QueryVisible(outIndices);
    }
}

void GameInstance::Visit_BTNodeTypes(const function<void(const TypeRegistry::BTNodeTypeInfo&)>& visitor) const
{
    if (_typeRegistry == nullptr)
        return;

    _typeRegistry->Visit_BTNodeTypes(visitor);
}

void GameInstance::Draw_DebugBox(const DebugBoxDesc& desc)
{
    return _debugManager->Draw_Box(desc);
}

void GameInstance::Draw_DebugSphere(const DebugSphereDesc& desc)
{
    return _debugManager->Draw_Sphere(desc);
}

void GameInstance::Draw_DebugLine(const DebugLineDesc& desc)
{
    return _debugManager->Draw_Line(desc);
}

void GameInstance::Draw_DebugTraceLine(const DebugTraceLineDesc& desc)
{
    return _debugManager->Draw_TraceLine(desc);
}

void GameInstance::Clear_DebugDraws()
{
    return _debugManager->Clear();
}

void GameInstance::Set_DebugRenderEnabled(bool enabled)
{
    return _debugManager->Set_Enabled(enabled);
}

bool GameInstance::Is_DebugRenderEnabled() const
{
    return _debugManager->Is_Enabled();
}

HRESULT GameInstance::Render_DebugDepth()
{
    return _debugManager->Render_Depth();
}

HRESULT GameInstance::Render_DebugOverlay()
{
    return _debugManager->Render_Overlay();
}

void GameInstance::Clear_Prints()
{
#ifdef _DEBUG
    _printManager->Clear();
#endif
}


void GameInstance::Clear_Print(const wstring& slot)
{
#ifdef _DEBUG
    _printManager->Clear(slot);
#endif
}

HRESULT GameInstance::Save_GameObjectJson(const fs::path& path, const Shared<GameObject>& gameObject)
{
    CHECK_NULL(gameObject, E_FAIL);

    json root = gameObject->To_Json();

    if (root.is_null() || !root.is_object())
        return E_FAIL;

    if (!path.has_parent_path())
        return E_FAIL;

    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);

    std::ofstream file(path);
    if (!file.is_open())
        return E_FAIL;

    file << root.dump(4);
    file.close();

    return S_OK;
}

json GameInstance::Load_JsonFile(const fs::path& path) const
{
    if (!fs::exists(path))
        return json{};

    std::ifstream file(path);
    if (!file.is_open())
        return json{};

    json root{};
    file >> root;
    file.close();

    return root;
}

HRESULT GameInstance::Load_GameObjectJson(const fs::path& path, const Shared<GameObject>& gameObject) const
{
    CHECK_NULL(gameObject, E_FAIL);

    json root = Load_JsonFile(path);
    if (root.is_null() || !root.is_object())
        return E_FAIL;

    gameObject->From_Json(root);

    return S_OK;
}

Shared<GameObject> GameInstance::Instantiate_GameObjectJson(const json& root)
{
    if (root.is_null() || !root.is_object())
        return nullptr;

    if (!root.contains("static_class"))
        return nullptr;

    const string staticClass = root["static_class"].get<string>();
    if (staticClass.empty())
        return nullptr;

    Shared<GameObject> gameObject = Create_GameObjectPrototype(staticClass);
    CHECK_NULL(gameObject, nullptr);

    CHECK_FAILED(gameObject->Initialize(nullptr), nullptr);

    gameObject->From_Json(root);

    return gameObject;
}

Shared<GameObject> GameInstance::Instantiate_GameObjectJson(const fs::path& path)
{
    json root = Load_JsonFile(path);

    return Instantiate_GameObjectJson(root);
}

HRESULT GameInstance::Create_PrefabAsset(const fs::path& path, const string& staticClass, const string& prefabName)
{
    if (staticClass.empty() || prefabName.empty())
        return E_FAIL;

    Shared<GameObject> gameObject = Create_GameObjectPrototype(staticClass);
    CHECK_NULL(gameObject, E_FAIL);

    CHECK_FAILED(gameObject->Initialize(nullptr), E_FAIL);

    json root = gameObject->To_Json();
    root["prefab_name"] = prefabName;

    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);

    std::ofstream file(path);
    if (!file.is_open())
        return E_FAIL;

    file << root.dump(4);
    file.close();

    return S_OK;
}

HRESULT GameInstance::Update_Picking()
{
    return _picking->Update();
}

bool GameInstance::Pick_WorldPosition(const POINT& pixelPos, Vec3& outWorldPos)
{
    return _picking->Pick_WorldPosition(pixelPos, outWorldPos);
}

bool GameInstance::Enqueue_ThreadJob(function<void()> job)
{
    if (_threadPool == nullptr)
        return false;

    return _threadPool->Enqueue(std::move(job));
}

size_t GameInstance::Get_PendingThreadJobCount() const
{
    if (_threadPool == nullptr)
        return 0;

    return _threadPool->Get_PendingJobCount();
}

void GameInstance::Release_Engine()
{
    auto ReleaseManager = [](auto& manager)
    {
        if (manager)
        {
            manager->Free();
            manager.reset();
        }
    };

    ReleaseManager(_threadPool);
    ReleaseManager(_debugManager);
    ReleaseManager(_cameraManager);
    ReleaseManager(_assetManager);

    ReleaseManager(_targetManager);
    ReleaseManager(_shadow);

#ifdef _DEBUG
    ReleaseManager(_printManager);
#endif
    ReleaseManager(_fontManager);

    ReleaseManager(_lightManager);
    ReleaseManager(_inputManager);
    ReleaseManager(_collisionManager);
    ReleaseManager(_physxManager);

    ReleaseManager(_pipeLine);
    ReleaseManager(_renderer);
    ReleaseManager(_objectManager);
    ReleaseManager(_uiManager);
    ReleaseManager(_prototypeManager);
    ReleaseManager(_typeRegistry);
    ReleaseManager(_levelManager);
    ReleaseManager(_picking);
    ReleaseManager(_timerManager);

    _context.Reset();
    _device.Reset();

    ReleaseManager(_graphicDevice);
    ReleaseManager(_logManager);
}

#ifdef _DEBUG

HRESULT GameInstance::Initialize_Debug(float sizeX, float sizeY)
{
    return _targetManager->Initialize_Debug(sizeX, sizeY);
}

HRESULT GameInstance::Resize_DrawArea(float sizeX, float sizeY)
{
    return _targetManager->Resize_DrawArea(sizeX, sizeY);
}

HRESULT GameInstance::Display_RT(const wstring& targetTag, bool render)
{
    return _targetManager->Display_RT(targetTag, render);
}

HRESULT GameInstance::Display_AllRTs(bool render)
{
    return _targetManager->Display_AllRTs(render);
}

HRESULT GameInstance::Render_RT_Debug(Shared<VIBuffer_Rect> viBuffer, Shared<ShaderCom> shader)
{
    return _targetManager->Render_Debug(shader, viBuffer);
}

const map<wstring, bool>& GameInstance::Get_RTs() const
{
    return _targetManager->Get_RTs();
}

HRESULT GameInstance::Bind_ShadowMatrices(Shared<ShaderCom> shader, const char* viewName, const char* projName)
{
    CHECK_NULL(_shadow, E_FAIL);
    return _shadow->Bind_TransformStateMatrix(shader, viewName, projName);
}

HRESULT GameInstance::Bind_ShadowLightDesc(Shared<ShaderCom> shader, const char* lightFarName, const char* biasName, const char* intensityName)
{
    CHECK_NULL(_shadow, E_FAIL);
    return _shadow->Bind_ShadowLightDesc(shader, lightFarName, biasName, intensityName);
}

HRESULT GameInstance::Bind_ShadowCB(Shared<ShaderCom> shader)
{
    CHECK_NULL(_shadow, E_FAIL);
    return _shadow->Bind_ShadowCB(shader);
}

HRESULT GameInstance::Update_ShadowLight(const ShadowLightDesc& desc)
{
    return _shadow->Update_ShadowLight(desc);
}

const ShadowLightDesc* GameInstance::Get_ShadowLightDesc() const
{
    CHECK_NULL(_shadow, nullptr);
    return _shadow->Get_ShadowLightDesc();
}

const Matrix* GameInstance::Get_ShadowTransformMatrix(D3DTS eType) const
{
    CHECK_NULL(_shadow, nullptr);
    if (eType == D3DTS::View)
        return _shadow->Get_ViewMatrix();
    if (eType == D3DTS::Proj)
        return _shadow->Get_ProjMatrix();

    return nullptr;
}

#endif

void GameInstance::Free()
{
    Release_Engine();

    __super::Free();
}
