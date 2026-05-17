#pragma region INCLUDE
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
#include "Helper_String.h"

#include "Collision_Define.h"
#include "PhysX_Types.h"

#ifdef _DEBUG
#include "Print_Manager.h"
#endif

#pragma endregion

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
    _unscaledTimeDelta = timeDelta; // 배속 전

    if (_isWorldTimeSlowActive)
    {
        _worldTimeSlowElapsed += timeDelta;

        if (_worldTimeSlowElapsed >= _worldTimeSlowDuration)
        {
            _worldTimeScale = _worldTimeScaleBeforeSlow;
            _isWorldTimeSlowActive = false;
            _worldTimeSlowElapsed = 0.f;
            _worldTimeSlowDuration = 0.f;
        }
    }

    _worldTimeDelta = timeDelta * _worldTimeScale;

#ifdef _DEBUG
    const auto updateBegin = chrono::steady_clock::now();
    double debugMgrMs = 0.0;
    double inputMgrMs = 0.0;
    double printMgrMs = 0.0;
    double clientMs = 0.0;
    double levelMs = 0.0;
    double priorityObjMs = 0.0;
    double pipelineMs = 0.0;
    double objMs = 0.0;
    double physxBeginMs = 0.0;
    double collClearMs = 0.0;
    double lateObjMs = 0.0;
    double physxUpdateMs = 0.0;
    double physxGatherMs = 0.0;
    double collFromPhysxMs = 0.0;
    double cameraMs = 0.0;
    double uiMs = 0.0;

    const auto debugMgrBegin = chrono::steady_clock::now();
#endif
    _debugManager->Update(timeDelta);
#ifdef _DEBUG
    debugMgrMs = chrono::duration<double, milli>(chrono::steady_clock::now() - debugMgrBegin).count();
    const auto inputMgrBegin = chrono::steady_clock::now();
#endif
    _inputManager->Update(timeDelta);
#ifdef _DEBUG
    inputMgrMs = chrono::duration<double, milli>(chrono::steady_clock::now() - inputMgrBegin).count();
    const auto printMgrBegin = chrono::steady_clock::now();
    _printManager->Update(timeDelta);
    printMgrMs = chrono::duration<double, milli>(chrono::steady_clock::now() - printMgrBegin).count();
#endif

    // 입력 갱신 직후, 레벨/오브젝트/UI 업데이트 전에 클라이언트쪽 런타임 갱신을 위해
    if (clientUpdate)
    {
#ifdef _DEBUG
        const auto clientBegin = chrono::steady_clock::now();
#endif
        clientUpdate(_worldTimeDelta);
#ifdef _DEBUG
        clientMs = chrono::duration<double, milli>(chrono::steady_clock::now() - clientBegin).count();
#endif
    }

#ifdef _DEBUG
    const auto levelBegin = chrono::steady_clock::now();
#endif
    _levelManager->Update(_worldTimeDelta);
#ifdef _DEBUG
    levelMs = chrono::duration<double, milli>(chrono::steady_clock::now() - levelBegin).count();
#endif

    // UI 키면서 게임 멈출 때 Update 안돌릴 애들
    if (!_isGameplayPaused)
    {
#ifdef _DEBUG
        const auto priorityObjBegin = chrono::steady_clock::now();
#endif
        _objectManager->Priority_Update(_worldTimeDelta);
#ifdef _DEBUG
        priorityObjMs = chrono::duration<double, milli>(chrono::steady_clock::now() - priorityObjBegin).count();

        const auto pipelineBegin = chrono::steady_clock::now();
#endif

        _pipeLine->Update();
#ifdef _DEBUG
        pipelineMs = chrono::duration<double, milli>(chrono::steady_clock::now() - pipelineBegin).count();

        const auto objBegin = chrono::steady_clock::now();
#endif

        _objectManager->Update(_worldTimeDelta);
#ifdef _DEBUG
        objMs = chrono::duration<double, milli>(chrono::steady_clock::now() - objBegin).count();

        const auto physxBegin = chrono::steady_clock::now();
#endif

        _physxManager->Begin_Frame();
#ifdef _DEBUG
        physxBeginMs = chrono::duration<double, milli>(chrono::steady_clock::now() - physxBegin).count();

        const auto collClearBegin = chrono::steady_clock::now();
#endif
        _collisionManager->Clear_Colliders();
#ifdef _DEBUG
        collClearMs = chrono::duration<double, milli>(chrono::steady_clock::now() - collClearBegin).count();

        const auto lateObjBegin = chrono::steady_clock::now();
#endif
        _objectManager->Late_Update(_worldTimeDelta);
#ifdef _DEBUG
        lateObjMs = chrono::duration<double, milli>(chrono::steady_clock::now() - lateObjBegin).count();

        const auto physxUpdateBegin = chrono::steady_clock::now();
#endif

        _physxManager->Update(_worldTimeDelta);
#ifdef _DEBUG
        physxUpdateMs = chrono::duration<double, milli>(chrono::steady_clock::now() - physxUpdateBegin).count();
#endif

#ifdef _DEBUG
        const auto physxGatherBegin = chrono::steady_clock::now();
#endif
        vector<PhysXPairEvent> events;
        _physxManager->Gather_FrameEvents(events);
#ifdef _DEBUG
        physxGatherMs = chrono::duration<double, milli>(chrono::steady_clock::now() - physxGatherBegin).count();

        const auto collFromPhysxBegin = chrono::steady_clock::now();
#endif
        _collisionManager->Update_FromPhysX(events);
#ifdef _DEBUG
        collFromPhysxMs = chrono::duration<double, milli>(chrono::steady_clock::now() - collFromPhysxBegin).count();

        const auto cameraBegin = chrono::steady_clock::now();
#endif

        _cameraManager->Update(_worldTimeDelta);
#ifdef _DEBUG
        cameraMs = chrono::duration<double, milli>(chrono::steady_clock::now() - cameraBegin).count();
#endif
    }
    
#ifdef _DEBUG
    const auto uiBegin = chrono::steady_clock::now();
#endif
    _uiManager->Update(timeDelta);
#ifdef _DEBUG
    uiMs = chrono::duration<double, milli>(chrono::steady_clock::now() - uiBegin).count();

    const double updateMs = chrono::duration<double, milli>(chrono::steady_clock::now() - updateBegin).count();
    Accumulate_UpdatePerf(
        updateMs,
        debugMgrMs,
        inputMgrMs,
        printMgrMs,
        clientMs,
        levelMs,
        priorityObjMs,
        pipelineMs,
        objMs,
        physxBeginMs,
        collClearMs,
        lateObjMs,
        physxUpdateMs,
        physxGatherMs,
        collFromPhysxMs,
        cameraMs,
        uiMs);
#endif
}

HRESULT GameInstance::Draw()
{
    if (FAILED(_uiManager->Render(UI_RenderGroup::Background)))
        return E_FAIL;

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

HRESULT GameInstance::Draw_Preview()
{
    _renderer->Draw_Preview();

    return S_OK;
}

void GameInstance::Backup_RenderGroup()
{
    if (_renderer)
        _renderer->Backup_RenderGroups();
}

void GameInstance::Restore_RenderGroup()
{
    if (_renderer)
        _renderer->Restore_RenderGroups();
}

void GameInstance::Set_PreviewDeferredCombine(bool enable)
{
    if (_renderer)
        _renderer->Set_PreviewDeferredCombine(enable);
}

#ifdef _DEBUG

void GameInstance::Accumulate_UpdatePerf(
    const double updateMs,
    const double debugMgrMs,
    const double inputMgrMs,
    const double printMgrMs,
    const double clientMs,
    const double levelMs,
    const double priorityObjMs,
    const double pipelineMs,
    const double objMs,
    const double physxBeginMs,
    const double collClearMs,
    const double lateObjMs,
    const double physxUpdateMs,
    const double physxGatherMs,
    const double collFromPhysxMs,
    const double cameraMs,
    const double uiMs)
{
    LogUtils::Accumulate_PassPerf(_updatePerfWindow.update, updateMs);
    LogUtils::Accumulate_PassPerf(_updatePerfWindow.debugMgr, debugMgrMs);
    LogUtils::Accumulate_PassPerf(_updatePerfWindow.inputMgr, inputMgrMs);
    LogUtils::Accumulate_PassPerf(_updatePerfWindow.printMgr, printMgrMs);
    LogUtils::Accumulate_PassPerf(_updatePerfWindow.client, clientMs);
    LogUtils::Accumulate_PassPerf(_updatePerfWindow.level, levelMs);
    LogUtils::Accumulate_PassPerf(_updatePerfWindow.priorityObj, priorityObjMs);
    LogUtils::Accumulate_PassPerf(_updatePerfWindow.pipeline, pipelineMs);
    LogUtils::Accumulate_PassPerf(_updatePerfWindow.obj, objMs);
    LogUtils::Accumulate_PassPerf(_updatePerfWindow.physxBegin, physxBeginMs);
    LogUtils::Accumulate_PassPerf(_updatePerfWindow.collClear, collClearMs);
    LogUtils::Accumulate_PassPerf(_updatePerfWindow.lateObj, lateObjMs);
    LogUtils::Accumulate_PassPerf(_updatePerfWindow.physxUpdate, physxUpdateMs);
    LogUtils::Accumulate_PassPerf(_updatePerfWindow.physxGather, physxGatherMs);
    LogUtils::Accumulate_PassPerf(_updatePerfWindow.collFromPhysx, collFromPhysxMs);
    LogUtils::Accumulate_PassPerf(_updatePerfWindow.camera, cameraMs);
    LogUtils::Accumulate_PassPerf(_updatePerfWindow.ui, uiMs);
}

bool GameInstance::Try_ConsumeUpdatePerfSummary(const float windowSec, UpdatePerfSummary& outSummary)
{
    outSummary = {};

    if (windowSec <= 0.f || 0 == _updatePerfWindow.update.sampleCount)
        return false;

    outSummary.valid = true;
    outSummary.updateFrameCount = static_cast<uint32>(_updatePerfWindow.update.sampleCount);

    const double frameCount = static_cast<double>(_updatePerfWindow.update.sampleCount);

    outSummary.updateAvgMs = static_cast<float>(LogUtils::Safe_Divide(_updatePerfWindow.update.accumulatedMs, frameCount));
    outSummary.updateMaxMs = static_cast<float>(_updatePerfWindow.update.maxMs);

    outSummary.debugMgrAvgMs = static_cast<float>(LogUtils::Safe_Divide(_updatePerfWindow.debugMgr.accumulatedMs, frameCount));
    outSummary.debugMgrMaxMs = static_cast<float>(_updatePerfWindow.debugMgr.maxMs);

    outSummary.inputMgrAvgMs = static_cast<float>(LogUtils::Safe_Divide(_updatePerfWindow.inputMgr.accumulatedMs, frameCount));
    outSummary.inputMgrMaxMs = static_cast<float>(_updatePerfWindow.inputMgr.maxMs);

    outSummary.printMgrAvgMs = static_cast<float>(LogUtils::Safe_Divide(_updatePerfWindow.printMgr.accumulatedMs, frameCount));
    outSummary.printMgrMaxMs = static_cast<float>(_updatePerfWindow.printMgr.maxMs);

    outSummary.clientAvgMs = static_cast<float>(LogUtils::Safe_Divide(_updatePerfWindow.client.accumulatedMs, frameCount));
    outSummary.clientMaxMs = static_cast<float>(_updatePerfWindow.client.maxMs);

    outSummary.levelAvgMs = static_cast<float>(LogUtils::Safe_Divide(_updatePerfWindow.level.accumulatedMs, frameCount));
    outSummary.levelMaxMs = static_cast<float>(_updatePerfWindow.level.maxMs);

    outSummary.priorityObjAvgMs = static_cast<float>(LogUtils::Safe_Divide(_updatePerfWindow.priorityObj.accumulatedMs, frameCount));
    outSummary.priorityObjMaxMs = static_cast<float>(_updatePerfWindow.priorityObj.maxMs);

    outSummary.pipelineAvgMs = static_cast<float>(LogUtils::Safe_Divide(_updatePerfWindow.pipeline.accumulatedMs, frameCount));
    outSummary.pipelineMaxMs = static_cast<float>(_updatePerfWindow.pipeline.maxMs);

    outSummary.objAvgMs = static_cast<float>(LogUtils::Safe_Divide(_updatePerfWindow.obj.accumulatedMs, frameCount));
    outSummary.objMaxMs = static_cast<float>(_updatePerfWindow.obj.maxMs);

    outSummary.physxBeginAvgMs = static_cast<float>(LogUtils::Safe_Divide(_updatePerfWindow.physxBegin.accumulatedMs, frameCount));
    outSummary.physxBeginMaxMs = static_cast<float>(_updatePerfWindow.physxBegin.maxMs);

    outSummary.collClearAvgMs = static_cast<float>(LogUtils::Safe_Divide(_updatePerfWindow.collClear.accumulatedMs, frameCount));
    outSummary.collClearMaxMs = static_cast<float>(_updatePerfWindow.collClear.maxMs);

    outSummary.lateObjAvgMs = static_cast<float>(LogUtils::Safe_Divide(_updatePerfWindow.lateObj.accumulatedMs, frameCount));
    outSummary.lateObjMaxMs = static_cast<float>(_updatePerfWindow.lateObj.maxMs);

    outSummary.physxUpdateAvgMs = static_cast<float>(LogUtils::Safe_Divide(_updatePerfWindow.physxUpdate.accumulatedMs, frameCount));
    outSummary.physxUpdateMaxMs = static_cast<float>(_updatePerfWindow.physxUpdate.maxMs);

    outSummary.physxGatherAvgMs = static_cast<float>(LogUtils::Safe_Divide(_updatePerfWindow.physxGather.accumulatedMs, frameCount));
    outSummary.physxGatherMaxMs = static_cast<float>(_updatePerfWindow.physxGather.maxMs);

    outSummary.collFromPhysxAvgMs = static_cast<float>(LogUtils::Safe_Divide(_updatePerfWindow.collFromPhysx.accumulatedMs, frameCount));
    outSummary.collFromPhysxMaxMs = static_cast<float>(_updatePerfWindow.collFromPhysx.maxMs);

    outSummary.cameraAvgMs = static_cast<float>(LogUtils::Safe_Divide(_updatePerfWindow.camera.accumulatedMs, frameCount));
    outSummary.cameraMaxMs = static_cast<float>(_updatePerfWindow.camera.maxMs);

    outSummary.uiAvgMs = static_cast<float>(LogUtils::Safe_Divide(_updatePerfWindow.ui.accumulatedMs, frameCount));
    outSummary.uiMaxMs = static_cast<float>(_updatePerfWindow.ui.maxMs);

    LogUtils::Reset_UpdatePerfWindow(_updatePerfWindow);
    return true;
}

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

    const bool hasUpdateSummary =
        Try_ConsumeUpdatePerfSummary(windowSec, runtimeSummary.update);

    const bool hasCollisionSummary =
        nullptr != _collisionManager &&
        _collisionManager->Try_ConsumePerfSummary(windowSec, runtimeSummary.collision);

    if (!hasRenderSummary && !hasUpdateSummary && !hasCollisionSummary)
    {
        _runtimePerfWindowStartTime = now;
        _hasRuntimePerfWindowStartTime = true;
        return;
    }

    runtimeSummary.approxFps = hasUpdateSummary
        ? static_cast<float>(LogUtils::Safe_Divide(static_cast<double>(runtimeSummary.update.updateFrameCount), static_cast<double>(windowSec)))
        : 0.f;

    Log_FileOnly(
        spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION },
        spdlog::level::trace,
        fmt::format(
            "[RuntimePerfDetail] render_ms(avg={:.2f} max={:.2f}) "
            "update_ms(avg={:.2f} max={:.2f}) fps~{:.1f} "
            "debugmgr_ms(avg={:.2f} max={:.2f}) "
            "input_ms(avg={:.2f} max={:.2f}) "
            "print_ms(avg={:.2f} max={:.2f}) "
            "client_ms(avg={:.2f} max={:.2f}) "
            "level_ms(avg={:.2f} max={:.2f}) "
            "priorityobj_ms(avg={:.2f} max={:.2f}) "
            "pipeline_ms(avg={:.2f} max={:.2f}) "
            "obj_ms(avg={:.2f} max={:.2f}) "
            "physxbegin_ms(avg={:.2f} max={:.2f}) "
            "collclear_ms(avg={:.2f} max={:.2f}) "
            "lateobj_ms(avg={:.2f} max={:.2f}) "
            "physxupdate_ms(avg={:.2f} max={:.2f}) "
            "physxgather_ms(avg={:.2f} max={:.2f}) "
            "collfromphysx_ms(avg={:.2f} max={:.2f}) "
            "camera_ms(avg={:.2f} max={:.2f}) "
            "uiupdate_ms(avg={:.2f} max={:.2f}) "
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

            runtimeSummary.update.updateAvgMs,
            runtimeSummary.update.updateMaxMs,
            runtimeSummary.approxFps,

            runtimeSummary.update.debugMgrAvgMs,
            runtimeSummary.update.debugMgrMaxMs,

            runtimeSummary.update.inputMgrAvgMs,
            runtimeSummary.update.inputMgrMaxMs,

            runtimeSummary.update.printMgrAvgMs,
            runtimeSummary.update.printMgrMaxMs,

            runtimeSummary.update.clientAvgMs,
            runtimeSummary.update.clientMaxMs,

            runtimeSummary.update.levelAvgMs,
            runtimeSummary.update.levelMaxMs,

            runtimeSummary.update.priorityObjAvgMs,
            runtimeSummary.update.priorityObjMaxMs,

            runtimeSummary.update.pipelineAvgMs,
            runtimeSummary.update.pipelineMaxMs,

            runtimeSummary.update.objAvgMs,
            runtimeSummary.update.objMaxMs,

            runtimeSummary.update.physxBeginAvgMs,
            runtimeSummary.update.physxBeginMaxMs,

            runtimeSummary.update.collClearAvgMs,
            runtimeSummary.update.collClearMaxMs,

            runtimeSummary.update.lateObjAvgMs,
            runtimeSummary.update.lateObjMaxMs,

            runtimeSummary.update.physxUpdateAvgMs,
            runtimeSummary.update.physxUpdateMaxMs,

            runtimeSummary.update.physxGatherAvgMs,
            runtimeSummary.update.physxGatherMaxMs,

            runtimeSummary.update.collFromPhysxAvgMs,
            runtimeSummary.update.collFromPhysxMaxMs,

            runtimeSummary.update.cameraAvgMs,
            runtimeSummary.update.cameraMaxMs,

            runtimeSummary.update.uiAvgMs,
            runtimeSummary.update.uiMaxMs,

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

void GameInstance::Set_WorldTimeScale(float timeScale)
{
    _isWorldTimeSlowActive = false;
    _worldTimeSlowElapsed = 0.f;
    _worldTimeSlowDuration = 0.f;
    _worldTimeScale = ::clamp(timeScale, 0.f, 8.f);
}

void GameInstance::Slow_WorldTime(float timeScale, float duration)
{
    if (duration <= 0.f)
    {
        Set_WorldTimeScale(timeScale);
        return;
    }

    if (!_isWorldTimeSlowActive)
        _worldTimeScaleBeforeSlow = _worldTimeScale;

    _isWorldTimeSlowActive = true;
    _worldTimeSlowElapsed = 0.f;
    _worldTimeSlowDuration = duration;
    _worldTimeScale = ::clamp(timeScale, 0.f, 8.f);
}

void GameInstance::Stop_WorldTimeSlow()
{
    if (!_isWorldTimeSlowActive)
        return;

    _worldTimeScale = _worldTimeScaleBeforeSlow;
    _isWorldTimeSlowActive = false;
    _worldTimeSlowElapsed = 0.f;
    _worldTimeSlowDuration = 0.f;
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
    if (activeCamera == nullptr)
        return E_FAIL;

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

const POINT& GameInstance::Get_RawMousePosition() const
{
    return _inputManager->Get_RawMousePosition();
}

void GameInstance::Set_MousePositionOverride(const POINT& mousePosition)
{
    return _inputManager->Set_MousePositionOverride(mousePosition);
}

void GameInstance::Clear_MousePositionOverride()
{
    return _inputManager->Clear_MousePositionOverride();
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

void GameInstance::Visit_GlobalEnums(const function<void(const TypeRegistry::GlobalEnumInfo&)>& visitor) const
{
	CHECK_NULL(_typeRegistry);
	_typeRegistry->Visit_GlobalEnums(visitor);
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

PhysXStaticActorHandle GameInstance::Register_PhysXStaticModel(const ModelCom& model, const Matrix& worldMatrix,
    Collision_Preset preset)
{
    CHECK_NULL(_physxManager, {});
    return _physxManager->Register_StaticModel(model, worldMatrix, preset);
}

void GameInstance::Unregister_PhysXStaticActor(PhysXStaticActorHandle handle)
{
    CHECK_NULL(_physxManager);
    _physxManager->Unregister_StaticActor(handle);
}

bool GameInstance::Sweep_Collider(ColliderCom* collider, const Matrix& worldMatrix, const Vec3& direction,
    float distance, PhysXQueryHit& outHit) const
{
    CHECK_NULL(_physxManager, false);
    return _physxManager->Sweep_Collider(collider, worldMatrix, direction, distance, outHit);
}

bool GameInstance::Sweep_Sphere(const Vec3& origin, const Vec3& direction, float distance, float radius,
    Collision_Preset queryPreset, PhysXQueryHit& outHit) const
{
    CHECK_NULL(_physxManager, false);
    return _physxManager->Sweep_Sphere(origin, direction, distance, radius, queryPreset, outHit);
}

bool GameInstance::Probe_Ground(ColliderCom* collider, const Matrix& worldMatrix, float probeDistance,
                                PhysXQueryHit& outHit) const
{
    CHECK_NULL(_physxManager, false);
    return _physxManager->Probe_Ground(collider, worldMatrix, probeDistance, outHit);
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
    ReleaseManager(_levelManager);
    ReleaseManager(_objectManager);
    ReleaseManager(_uiManager);
    ReleaseManager(_prototypeManager);
    ReleaseManager(_typeRegistry);
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
