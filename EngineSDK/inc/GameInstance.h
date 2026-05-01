#pragma once

#include "Prototype_Manager.h"
#include "Input_Manager.h"

NS_BEGIN(Engine)

class Graphic_Device;
class Timer_Manager;
class Level_Manager;
class Prototype_Manager;
class Object_Manager;
class Renderer;
class PipeLine;
class Light_Manager;
class Font_Manager;
class Asset_Manager;

class GameObject;
class Level;

class ENGINE_DLL GameInstance final : public Base
{
	DECLARE_SINGLETON(GameInstance)
private:
	GameInstance();

public:
	virtual ~GameInstance();

public:
	HRESULT Initialize_Engine(const ENGINE_DESC& EngineDesc, ComPtr<Device>& device, ComPtr<Context>& context);
	void    Update_Engine(float fTimeDelta);
	HRESULT Draw();
	void    Clear_Resources(uint32 iLevelIndex);

	float   Random(float fMin, float fMax);

	
public: /* For.Graphic_Device */
	HRESULT Clear_Buffers(const Vec4& clearColor);
	HRESULT Present();

    void    Bind_BackBuffer();
    HRESULT Resize_BackBuffer(uint32 width, uint32 height);

    ComPtr<SwapChain> Get_SwapChain() const;

public: /* For.Viewport */
    void   Set_ViewportSize(uint32 width, uint32 height);
    uint32 Get_ViewportWidth() const;
    uint32 Get_ViewportHeight() const;

    // 엔진 백버퍼 기준 viewport 사이즈
    uint32 Get_BackBufferWidth() const;
    uint32 Get_BackBufferHeight() const;

public: /* For.Timer_Manager */
	HRESULT	Add_Timer(const wstring& strTimerTag);
	float Compute_TimeDelta(const wstring& strTimerTag);

public: /* For.Level_Manager */
	HRESULT Change_Level(uint32 iNewLevelIndex, Shared<Level> pNewLevel);

public: /* For.Prototype_Manager */
	HRESULT Add_Prototype(uint32 iLevelIndex, const wstring& strPrototypeTag, Shared<Base> pPrototype);	
	Shared<Base> Clone_Prototype(Prototype ePrototype, uint32 iLevelIndex, const wstring& strPrototypeTag, void* pArg = nullptr);

public: /* For.Object_Manager */
	HRESULT Add_GameObject(uint32 iPrototypeLevelIndex, const wstring& strPrototypeTag, uint32 iLayerLevelIndex, const wstring& strLayerTag, void* pArg = nullptr);

public: /* For.Renderer */
	void Add_RenderGroup(RenderGroup eRenderGroup, Shared<GameObject> gameObject);

public: /* For.PipeLine */
	HRESULT Bind_CamPosition(Shader* pShader, const char* pConstantName);
	HRESULT Bind_TransformMatrix(D3DTS eTransformState, Shader* pShader, const char* pConstantName);
	HRESULT Bind_TransformMatrix_Inverse(D3DTS eTransformState, Shader* pShader, const char* pConstantName);

	const XMFLOAT4X4* Get_Transform(D3DTS eTransformState) const;
	const XMFLOAT4* Get_CamPosition() const;

	void Set_Transform(D3DTS eTransformState, FXMMATRIX TransformStateMatrix);

public: /* For.Input_Manager */
	bool KeyPress(KEY_TYPE eKey) const;
	bool KeyDown(KEY_TYPE eKey) const;
	bool KeyUp(KEY_TYPE eKey) const;

	Vec2 Get_MouseDelta() const;
	void Lock_Mouse();
	void Unlock_Mouse();

    // 입력 차단
    void Set_InputBlocked(bool isBlocked);

public: /* For.Light_Manager */
	const LIGHT_DESC* Get_LightDesc(uint32 iIndex);
	HRESULT Add_Light(const LIGHT_DESC& LightDesc);

public: /* For.Font_Manager */
	HRESULT Add_Font(const wstring& strFontTag, const wchar_t* pFontFilePath);
	void Draw_Font(const wstring& strFontTag, const wchar_t* pText, const Vec2& position, FXMVECTOR vColor = XMVectorSet(1.f, 1.f, 1.f, 1.f));

public: /* For.Asset_Manager */
	/* 리소스 루트를 기준으로 GUID/meta/cache registry를 준비할 때 호출한다. */
	HRESULT Initialize_AssetManager(const wstring& strResourceRoot);

	/* 파일 경로를 GUID로 저장해야 하는 ResourceLoader/Editor 파이프라인에서 호출한다. */
	string Find_AssetGUID(const wstring& strFilePath) const;

	/* GUID로 에셋 타입과 경로 메타를 조회할 때 호출한다. */
	const FAssetMeta* Find_AssetByGUID(const string& strGuid) const;

	/* lazy-load가 GUID만 들고 있을 때 실제 파일 경로를 복원하기 위해 호출한다. */
	wstring Resolve_AssetPath(const string& strGuid) const;

private:
	Unique<Graphic_Device>		_graphicDevice;
	Unique<Timer_Manager>		_timerManager;
	Unique<Level_Manager>		_levelManager;
	Unique<Prototype_Manager>	_prototypeManager;
	Unique<Object_Manager>		_objectManager;
	Unique<Renderer>			_renderer;
	Unique<PipeLine>			_pipeLine;           
	Unique<Input_Manager>		_inputManager;      
	Unique<Light_Manager>		_lightManager;       
	Unique<Font_Manager>		_fontManager;        
	Unique<Asset_Manager>		_assetManager; /* GUID 기반 에셋 registry와 meta/cache를 관리하는 매니저 */

private:
    uint32 _viewportWidth = 0;
    uint32 _viewportHeight = 0;

public:	
	void Release_Engine();
	virtual void Free() override;
};

NS_END
