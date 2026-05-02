#include "GameInstance.h"
#include "Asset_Manager.h"
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

    _viewportWidth = engineDesc.viewportWidth;
    _viewportHeight = engineDesc.viewportHeight;

    if (nullptr == _graphicDevice)
        return E_FAIL;

    _logManager = Log_Manager::Create(engineDesc.projectName);
    CHECK_NULL(_logManager, E_FAIL);

    _timerManager = Timer_Manager::Create();
    if (nullptr == _timerManager)
        return E_FAIL;

    _levelManager = Level_Manager::Create();
    if (nullptr == _levelManager)
        return E_FAIL;

    _prototypeManager = Prototype_Manager::Create(engineDesc.numLevels);
    if (nullptr == _prototypeManager)
        return E_FAIL;

    _typeRegistry = TypeRegistry::Create();
    if (nullptr == _typeRegistry)
        return E_FAIL;

    if (FAILED(Flush_PendingTypeRegistrations(*_typeRegistry)))
        return E_FAIL;

    _objectManager = Object_Manager::Create(engineDesc.numLevels);
    if (nullptr == _objectManager)
        return E_FAIL;

    _renderer = Renderer::Create(device, context);
    if (nullptr == _renderer)
        return E_FAIL;

    _pipeLine = PipeLine::Create();
    if (nullptr == _pipeLine)
        return E_FAIL;

    _inputManager = Input_Manager::Create(engineDesc.hWnd);
    if (nullptr == _inputManager)
        return E_FAIL;

    _lightManager = Light_Manager::Create();
    if (nullptr == _lightManager)
        return E_FAIL;

    _fontManager = Font_Manager::Create(device, context);
    if (nullptr == _fontManager)
        return E_FAIL;

    return S_OK;
}

void GameInstance::Update_Engine(float timeDelta)
{
    _inputManager->Update(timeDelta);

    _objectManager->Priority_Update(timeDelta);

    _pipeLine->Update();

    _objectManager->Update(timeDelta);

    _objectManager->Late_Update(timeDelta);

    _levelManager->Update(timeDelta);
}

HRESULT GameInstance::Draw()
{
    _renderer->Draw();

    _levelManager->Render();

    return S_OK;
}

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

HRESULT GameInstance::Add_Prototype(uint32 levelIndex, const wstring& prototypeTag, Shared<Base> prototype)
{
    return _prototypeManager->Add_Prototype(levelIndex, prototypeTag, prototype);
}

Shared<Base> GameInstance::Clone_Prototype(Prototype prototype, uint32 levelIndex, const wstring& prototypeTag, void* arg)
{
    return _prototypeManager->Clone_Prototype(prototype, levelIndex, prototypeTag, arg);
}

HRESULT GameInstance::Add_GameObject(
    uint32 prototypeLevelIndex,
    const wstring& prototypeTag,
    uint32 layerLevelIndex,
    const wstring& layerTag,
    void* arg)
{
    return _objectManager->Add_GameObject(prototypeLevelIndex, prototypeTag, layerLevelIndex, layerTag, arg);
}

void GameInstance::Add_RenderGroup(RenderGroup renderGroup, Shared<GameObject> gameObject)
{
    return _renderer->Add_RenderGroup(renderGroup, gameObject);
}

HRESULT GameInstance::Bind_CamPosition(Shader* shader, const char* constantName)
{
    return _pipeLine->Bind_CamPosition(shader, constantName);
}

HRESULT GameInstance::Bind_TransformMatrix(D3DTS transformState, Shader* shader, const char* constantName)
{
    return _pipeLine->Bind_TransformMatrix(transformState, shader, constantName);
}

HRESULT GameInstance::Bind_TransformMatrix_Inverse(D3DTS transformState, Shader* shader, const char* constantName)
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
    if (nullptr == _assetManager)
        return {};

    return _assetManager->Find_GUID(filePath);
}

const FAssetMeta* GameInstance::Find_AssetByGUID(const string& guid) const
{
    if (nullptr == _assetManager)
        return nullptr;

    return _assetManager->Find_ByGUID(guid);
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

const TypeRegistry::ComponentTypeInfo* GameInstance::Find_ComponentType(const string& className) const
{
    return (nullptr == _typeRegistry) ? nullptr : _typeRegistry->Find_ComponentType(className);
}

const TypeRegistry::GameObjectTypeInfo* GameInstance::Find_GameObjectType(const string& className) const
{
    return (nullptr == _typeRegistry) ? nullptr : _typeRegistry->Find_GameObjectType(className);
}

const TypeRegistry::BTNodeTypeInfo* GameInstance::Find_BTNodeType(const string& className) const
{
    return (nullptr == _typeRegistry) ? nullptr : _typeRegistry->Find_BTNodeType(className);
}

Shared<Component> GameInstance::Create_ComponentPrototype(const string& className) const
{
    if (nullptr == _typeRegistry || nullptr == _graphicDevice)
        return nullptr;

    return _typeRegistry->Create_Component(className, _graphicDevice->Get_Device(), _graphicDevice->Get_Context());
}

Shared<GameObject> GameInstance::Create_GameObjectPrototype(const string& className) const
{
    if (nullptr == _typeRegistry || nullptr == _graphicDevice)
        return nullptr;

    return _typeRegistry->Create_GameObject(className, _graphicDevice->Get_Device(), _graphicDevice->Get_Context());
}

Shared<Base> GameInstance::Create_BTNodePrototype(const string& className) const
{
    return (nullptr == _typeRegistry) ? nullptr : _typeRegistry->Create_BTNode(className);
}

void GameInstance::Release_Engine()
{
    if (nullptr != _assetManager)
    {
        _assetManager->Free();
        _assetManager.reset();
    }

    if (nullptr != _fontManager)
    {
        _fontManager->Free();
        _fontManager.reset();
    }

    if (nullptr != _lightManager)
    {
        _lightManager->Free();
        _lightManager.reset();
    }

    if (nullptr != _inputManager)
    {
        _inputManager->Free();
        _inputManager.reset();
    }

    if (nullptr != _pipeLine)
    {
        _pipeLine->Free();
        _pipeLine.reset();
    }

    if (nullptr != _renderer)
    {
        _renderer->Free();
        _renderer.reset();
    }

    if (nullptr != _objectManager)
    {
        _objectManager->Free();
        _objectManager.reset();
    }

    if (nullptr != _prototypeManager)
    {
        _prototypeManager->Free();
        _prototypeManager.reset();
    }

    if (nullptr != _typeRegistry)
    {
        _typeRegistry->Free();
        _typeRegistry.reset();
    }

    if (nullptr != _levelManager)
    {
        _levelManager->Free();
        _levelManager.reset();
    }

    if (nullptr != _timerManager)
    {
        _timerManager->Free();
        _timerManager.reset();
    }

    if (nullptr != _graphicDevice)
    {
        _graphicDevice->Free();
        _graphicDevice.reset();
    }

    if (nullptr != _logManager)
    {
        _logManager->Free();
        _logManager.reset();
    }

    DestroyInstance();
}

void GameInstance::Free()
{
    __super::Free();
}
