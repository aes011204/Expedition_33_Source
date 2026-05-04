#include "GameInstance.h"
#include "Asset_Manager.h"
#include "Camera_Manager.h"
#include "Collision_Manager.h"
#include "Debug_Manager.h"
#include "Font_Manager.h"
#include "Graphic_Device.h"
#include "Input_Manager.h"
#include "Level_Manager.h"
#include "Light_Manager.h"
#include "Log_Manager.h"
#include "Object_Manager.h"
#include "PipeLine.h"
#include "Prototype_Manager.h"
#include "RegistorFactory.h"
#include "Renderer.h"
#include "Timer_Manager.h"

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

    _typeRegistry = TypeRegistry::Create();
    CHECK_NULL(_typeRegistry, E_FAIL);

    if (FAILED(Flush_PendingTypeRegistrations(*_typeRegistry)))
        return E_FAIL;

    _objectManager = Object_Manager::Create(engineDesc.numLevels);
    CHECK_NULL(_objectManager, E_FAIL);

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

    return S_OK;
}

void GameInstance::Update_Engine(float timeDelta)
{
    _debugManager->Update(timeDelta);
    _inputManager->Update(timeDelta);

    _levelManager->Update(timeDelta);

    _objectManager->Priority_Update(timeDelta);

    _pipeLine->Update();

    _objectManager->Update(timeDelta);

    _collisionManager->Clear_Colliders();
    _objectManager->Late_Update(timeDelta);
    _collisionManager->Update();

    _levelManager->Update(timeDelta);
    _cameraManager->Update(timeDelta);

}

HRESULT GameInstance::Draw()
{
    _renderer->Draw();

    _levelManager->Render();

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
#endif    

#ifdef _DEBUG
    Try_FlushRuntimePerfLog();
#endif

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

    if (hasRenderSummary && runtimeSummary.render.renderCallCount > 0)
    {
        runtimeSummary.approxFps =
            static_cast<float>(runtimeSummary.render.renderCallCount) / windowSec;
    }
    else if (hasCollisionSummary && runtimeSummary.collision.updateFrameCount > 0)
    {
        runtimeSummary.approxFps =
            static_cast<float>(runtimeSummary.collision.updateFrameCount) / windowSec;
    }

    LOG_INFO(
        "[RuntimePerf] fps={:.1f} render_ms(avg={:.2f} max={:.2f}) "
        "priority_ms(avg={:.2f}) nonblend_ms(avg={:.2f}) blend_ms(avg={:.2f}) ui_ms(avg={:.2f}) "
        "collision_ms(avg={:.2f} max={:.2f}) colliders(active={:.1f})",
        runtimeSummary.approxFps,
        runtimeSummary.render.renderAvgMs,
        runtimeSummary.render.renderMaxMs,
        runtimeSummary.render.priorityAvgMs,
        runtimeSummary.render.nonBlendAvgMs,
        runtimeSummary.render.blendAvgMs,
        runtimeSummary.render.uiAvgMs,
        runtimeSummary.collision.updateAvgMs,
        runtimeSummary.collision.updateMaxMs,
        runtimeSummary.collision.activeCollidersAvg
    );

    // TODO (RuntimePerf):
    // preview/deferred/shadow/light/combine pass가 실제 Renderer에 추가되면
    // counter와 summary field를 먼저 연결한 뒤 아래 확장 포맷으로 교체한다.

    // LOG_INFO(
    //     "[RuntimePerf] fps={:.1f} render_ms(avg={:.2f} max={:.2f}) "
    //     "priority_ms(avg={:.2f}) nonblend_ms(avg={:.2f}) blend_ms(avg={:.2f}) ui_ms(avg={:.2f}) "
    //     "shadow_ms(avg={:.2f} max={:.2f}) "
    //     "lights_ms(avg={:.2f}) combined_ms(avg={:.2f}) "
    //     "collision_ms(avg={:.2f} max={:.2f}) colliders(active={:.1f})",
    //     runtimeSummary.approxFps,
    //     runtimeSummary.render.renderAvgMs,
    //     runtimeSummary.render.renderMaxMs,
    //     runtimeSummary.render.priorityAvgMs,
    //     runtimeSummary.render.nonBlendAvgMs,
    //     runtimeSummary.render.blendAvgMs,
    //     runtimeSummary.render.uiAvgMs,
    //     runtimeSummary.render.shadowAvgMs,
    //     runtimeSummary.render.shadowMaxMs,
    //     runtimeSummary.render.lightsAvgMs,
    //     runtimeSummary.render.combinedAvgMs,
    //     runtimeSummary.collision.updateAvgMs,
    //     runtimeSummary.collision.updateMaxMs,
    //     runtimeSummary.collision.activeCollidersAvg
    // );

    if (Should_LogRuntimePerfDetail(runtimeSummary))
    {
        LOG_INFO(
            "[RuntimePerfDetail] render_ms(avg={:.2f} max={:.2f}) "
            "priority_ms(avg={:.2f}) nonblend_ms(avg={:.2f}) blend_ms(avg={:.2f}) ui_ms(avg={:.2f}) "
            "collision_ms(avg={:.2f} max={:.2f}) pairs={:.1f}/s intersects={:.1f}/s hits={:.1f}/s",
            runtimeSummary.render.renderAvgMs,
            runtimeSummary.render.renderMaxMs,
            runtimeSummary.render.priorityAvgMs,
            runtimeSummary.render.nonBlendAvgMs,
            runtimeSummary.render.blendAvgMs,
            runtimeSummary.render.uiAvgMs,
            runtimeSummary.collision.updateAvgMs,
            runtimeSummary.collision.updateMaxMs,
            runtimeSummary.collision.pairsPerSec,
            runtimeSummary.collision.intersectsPerSec,
            runtimeSummary.collision.hitsPerSec
        );

        // TODO (RuntimePerfDetail):
        // 확장 pass counter와 summary field가 연결된 뒤 detail 로그도 같은 기준으로 확장한다.

        // LOG_INFO(
        //     "[RuntimePerfDetail] render_ms(avg={:.2f} max={:.2f}) "
        //     "priority_ms(avg={:.2f}) nonblend_ms(avg={:.2f}) blend_ms(avg={:.2f}) ui_ms(avg={:.2f}) "
        //     "shadow_ms(avg={:.2f} max={:.2f}) lights_ms(avg={:.2f}) "
        //     "combined_ms(avg={:.2f}) "
        //     "collision_ms(avg={:.2f} max={:.2f}) pairs={:.1f}/s intersects={:.1f}/s hits={:.1f}/s",
        //     runtimeSummary.render.renderAvgMs,
        //     runtimeSummary.render.renderMaxMs,
        //     runtimeSummary.render.priorityAvgMs,
        //     runtimeSummary.render.nonBlendAvgMs,
        //     runtimeSummary.render.blendAvgMs,
        //     runtimeSummary.render.uiAvgMs,
        //     runtimeSummary.render.shadowAvgMs,
        //     runtimeSummary.render.shadowMaxMs,
        //     runtimeSummary.render.lightsAvgMs,
        //     runtimeSummary.render.combinedAvgMs,
        //     runtimeSummary.collision.updateAvgMs,
        //     runtimeSummary.collision.updateMaxMs,
        //     runtimeSummary.collision.pairsPerSec,
        //     runtimeSummary.collision.intersectsPerSec,
        //     runtimeSummary.collision.hitsPerSec
        // );
    }

    _runtimePerfWindowStartTime = now;
    _hasRuntimePerfWindowStartTime = true;
}

bool GameInstance::Should_LogRuntimePerfDetail(const RuntimePerfSummary& runtimeSummary)
{
    return
        runtimeSummary.render.renderAvgMs >= detailRenderAvgMsThreshold ||
        runtimeSummary.collision.updateAvgMs >= detailCollisionAvgMsThreshold ||
        runtimeSummary.collision.intersectsPerSec >= detailIntersectsPerSecThreshold;
}
#endif

void GameInstance::Clear_Resources(uint32 levelIndex)
{
    _objectManager->Clear_Layers(levelIndex);
    _prototypeManager->Clear_Prototypes(levelIndex);
}

float GameInstance::Random(float min, float max)
{
    return min + static_cast<float>(rand()) / RAND_MAX * (max - min);
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

uint32 GameInstance::Current_LevelIndex()
{
    return _levelManager->Get_CurrentLevelIndex();
}

Shared<Level> GameInstance::Current_LevelType()
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

Shared<GameObject> GameInstance::Find_FirstObject(uint32 levelIndex, const wstring& layerTag)
{
    return _objectManager->Find_FirstObject(levelIndex, layerTag);
}

void GameInstance::Add_RenderGroup(RenderGroup renderGroup, Shared<GameObject> gameObject)
{
    return _renderer->Add_RenderGroup(renderGroup, gameObject);
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

const Vec4* GameInstance::Get_CamPosition() const
{
    return _pipeLine->Get_CamPosition();
}

void GameInstance::Set_Transform(D3DTS transformState, FXMMATRIX& transformStateMatrix)
{
    _pipeLine->Set_Transform(transformState, transformStateMatrix);
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

void GameInstance::Lock_Mouse()
{
    return _inputManager->Lock_Mouse();
}

void GameInstance::Unlock_Mouse()
{
    return _inputManager->Unlock_Mouse();
}

void GameInstance::Set_InputBlocked(bool isBlocked)
{
    return _inputManager->Set_InputBlocked(isBlocked);
}

const LIGHT_DESC* GameInstance::Get_LightDesc(uint32 index)
{
    return _lightManager->Get_LightDesc(index);
}

HRESULT GameInstance::Add_Light(const LIGHT_DESC& lightDesc)
{
    return _lightManager->Add_Light(lightDesc);
}

HRESULT GameInstance::Add_Font(const wstring& fontTag, const wchar_t* fontFilePath)
{
    return _fontManager->Add_Font(fontTag, fontFilePath);
}

void GameInstance::Draw_Font(const wstring& fontTag, const wchar_t* text, const Vec2& position, FXMVECTOR color)
{
    _fontManager->Draw(fontTag, text, position, color);
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
    return _assetManager->Find_GUID(filePath);
}

const FAssetMeta* GameInstance::Find_AssetByGUID(const string& guid) const
{
    return _assetManager->Find_ByGUID(guid);
}

wstring GameInstance::Resolve_AssetPath(const string& guid) const
{
    return _assetManager->Resolve_Path(guid);
}

wstring GameInstance::Get_AssetRoot() const
{
    return _assetManager->Get_ResourceRoot();
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

void GameInstance::Add_Collider(Shared<ColliderCom> collider)
{
    return _collisionManager->Add_Collider(collider);
}

void GameInstance::Clear_Colliders()
{
    return _collisionManager->Clear_Colliders();
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

    ReleaseManager(_debugManager);
    ReleaseManager(_cameraManager);
    ReleaseManager(_assetManager);
    ReleaseManager(_fontManager);
    ReleaseManager(_lightManager);
    ReleaseManager(_inputManager);
    ReleaseManager(_collisionManager);

    ReleaseManager(_pipeLine);
    ReleaseManager(_renderer);
    ReleaseManager(_objectManager);
    ReleaseManager(_prototypeManager);
    ReleaseManager(_typeRegistry);
    ReleaseManager(_levelManager);
    ReleaseManager(_timerManager);
    ReleaseManager(_graphicDevice);
    ReleaseManager(_logManager);
}

void GameInstance::Free()
{
    Release_Engine();

    __super::Free();
}
