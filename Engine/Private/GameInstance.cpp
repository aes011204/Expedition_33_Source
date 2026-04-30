#include "GameInstance.h"
#include "Graphic_Device.h"
#include "Timer_Manager.h"
#include "Level_Manager.h"
#include "Prototype_Manager.h"
#include "Object_Manager.h"
#include "Renderer.h"
#include "PipeLine.h"
#include "Input_Manager.h"
#include "Light_Manager.h"
#include "Font_Manager.h"
#include "Asset_Manager.h"

IMPLEMENT_SINGLETON(GameInstance)

GameInstance::GameInstance()
{
}

GameInstance::~GameInstance() = default;

HRESULT GameInstance::Initialize_Engine(const ENGINE_DESC& EngineDesc, ComPtr<Device>& device, ComPtr<Context>& context)
{
	_graphicDevice = Graphic_Device::Create(
        EngineDesc.hWnd, EngineDesc.eWinMode, EngineDesc.iViewportWidth, EngineDesc.iViewportHeight, device, context);

    _viewportWidth = EngineDesc.iViewportWidth;
    _viewportHeight = EngineDesc.iViewportHeight;

	if (nullptr == _graphicDevice)
		return E_FAIL;

	_timerManager = Timer_Manager::Create();
	if (nullptr == _timerManager)
		return E_FAIL;

	_levelManager = Level_Manager::Create();
	if (nullptr == _levelManager)
		return E_FAIL;

	_prototypeManager = Prototype_Manager::Create(EngineDesc.iNumLevels);
	if (nullptr == _prototypeManager)
		return E_FAIL;

	_objectManager = Object_Manager::Create(EngineDesc.iNumLevels);
	if (nullptr == _objectManager)
		return E_FAIL;

	_renderer = Renderer::Create(device, context);
	if (nullptr == _renderer)
		return E_FAIL;

	_pipeLine = PipeLine::Create();
	if (nullptr == _pipeLine)
		return E_FAIL;

	_inputManager = Input_Manager::Create(EngineDesc.hWnd);
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

void GameInstance::Update_Engine(float fTimeDelta)
{
	_inputManager->Update(fTimeDelta);

	_objectManager->Priority_Update(fTimeDelta);

	_pipeLine->Update();

	_objectManager->Update(fTimeDelta);

	_objectManager->Late_Update(fTimeDelta);

	_levelManager->Update(fTimeDelta);
}

HRESULT GameInstance::Draw()
{
	_renderer->Draw();

	_levelManager->Render();

	return S_OK;
}

void GameInstance::Clear_Resources(uint32 iLevelIndex)
{
	_objectManager->Clear_Layers(iLevelIndex);
	_prototypeManager->Clear_Prototypes(iLevelIndex);
}

float GameInstance::Random(float fMin, float fMax)
{
	return fMin + (static_cast<float>(rand()) / RAND_MAX) * (fMax - fMin);
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
     return _viewportWidth;
}

HRESULT GameInstance::Add_Timer(const wstring& strTimerTag)
{
	return _timerManager->Add_Timer(strTimerTag);
}

float GameInstance::Compute_TimeDelta(const wstring& strTimerTag)
{
	return _timerManager->Compute_TimeDelta(strTimerTag);
}

HRESULT GameInstance::Change_Level(uint32 iNewLevelIndex, Shared<Level> pNewLevel)
{
	return _levelManager->Change_Level(iNewLevelIndex, pNewLevel);
}

HRESULT GameInstance::Add_Prototype(uint32 iLevelIndex, const wstring& strPrototypeTag, Shared<Base> pPrototype)
{
	return _prototypeManager->Add_Prototype(iLevelIndex, strPrototypeTag, pPrototype);
}

Shared<Base> GameInstance::Clone_Prototype(Prototype ePrototype, uint32 iLevelIndex, const wstring& strPrototypeTag, void* pArg)
{
	return _prototypeManager->Clone_Prototype(ePrototype, iLevelIndex, strPrototypeTag, pArg);
}

HRESULT GameInstance::Add_GameObject(uint32 iPrototypeLevelIndex, const wstring& strPrototypeTag, uint32 iLayerLevelIndex, const wstring& strLayerTag, void* pArg)
{
	return _objectManager->Add_GameObject(iPrototypeLevelIndex, strPrototypeTag, iLayerLevelIndex, strLayerTag, pArg);
}

void GameInstance::Add_RenderGroup(RenderGroup eRenderGroup, GameObject* pGameObject)
{
	return _renderer->Add_RenderGroup(eRenderGroup, pGameObject);
}

HRESULT GameInstance::Bind_CamPosition(Shader* pShader, const char* pConstantName)
{
	return _pipeLine->Bind_CamPosition(pShader, pConstantName);
}

HRESULT GameInstance::Bind_TransformMatrix(D3DTS eTransformState, Shader* pShader, const char* pConstantName)
{
	return _pipeLine->Bind_TransformMatrix(eTransformState, pShader, pConstantName);
}

HRESULT GameInstance::Bind_TransformMatrix_Inverse(D3DTS eTransformState, Shader* pShader, const char* pConstantName)
{
	return _pipeLine->Bind_TransformMatrix_Inverse(eTransformState, pShader, pConstantName);
}

const XMFLOAT4X4* GameInstance::Get_Transform(D3DTS eTransformState) const
{
	return _pipeLine->Get_Transform(eTransformState);
}

const XMFLOAT4* GameInstance::Get_CamPosition() const
{
	return _pipeLine->Get_CamPosition();
}

void GameInstance::Set_Transform(D3DTS eTransformState, FXMMATRIX TransformStateMatrix)
{
	_pipeLine->Set_Transform(eTransformState, TransformStateMatrix);
}


bool GameInstance::KeyPress(KEY_TYPE eKey) const
{
	return nullptr != _inputManager && _inputManager->KeyPress(eKey);
}

bool GameInstance::KeyDown(KEY_TYPE eKey) const
{
	return nullptr != _inputManager && _inputManager->KeyDown(eKey);
}

bool GameInstance::KeyUp(KEY_TYPE eKey) const
{
	return nullptr != _inputManager && _inputManager->KeyUp(eKey);
}

Vec2 GameInstance::Get_MouseDelta() const
{
	if (nullptr == _inputManager)
		return Vec2::Zero;

	return _inputManager->Get_MouseDelta();
}

void GameInstance::Lock_Mouse()
{
	if (nullptr != _inputManager)
		_inputManager->Lock_Mouse();
}

void GameInstance::Unlock_Mouse()
{
	if (nullptr != _inputManager)
		_inputManager->Unlock_Mouse();
}

const LIGHT_DESC* GameInstance::Get_LightDesc(uint32 iIndex)
{
	return _lightManager->Get_LightDesc(iIndex);
}

HRESULT GameInstance::Add_Light(const LIGHT_DESC& LightDesc)
{
	return _lightManager->Add_Light(LightDesc);
}

HRESULT GameInstance::Add_Font(const wstring& strFontTag, const wchar_t* pFontFilePath)
{
	return _fontManager->Add_Font(strFontTag, pFontFilePath);
}

void GameInstance::Draw_Font(const wstring& strFontTag, const wchar_t* pText, const Vec2& position, FXMVECTOR vColor)
{
	_fontManager->Draw(strFontTag, pText, position, vColor);
}

HRESULT GameInstance::Initialize_AssetManager(const wstring& strResourceRoot)
{
	_assetManager = Asset_Manager::Create(strResourceRoot);
	if (nullptr == _assetManager)
		return E_FAIL;

	return S_OK;
}

string GameInstance::Find_AssetGUID(const wstring& strFilePath) const
{
	if (nullptr == _assetManager)
		return {};

	return _assetManager->Find_GUID(strFilePath);
}

const FAssetMeta* GameInstance::Find_AssetByGUID(const string& strGuid) const
{
	if (nullptr == _assetManager)
		return nullptr;

	return _assetManager->Find_ByGUID(strGuid);
}

wstring GameInstance::Resolve_AssetPath(const string& strGuid) const
{
	if (nullptr == _assetManager)
		return {};

	return _assetManager->Resolve_Path(strGuid);
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

	DestroyInstance();
}

void GameInstance::Free()
{
	__super::Free();

	
}
