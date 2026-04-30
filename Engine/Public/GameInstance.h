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

public: /* For.Timer_Manager */
	HRESULT	Add_Timer(const wstring& strTimerTag);
	float Compute_TimeDelta(const wstring& strTimerTag);

public: /* For.Level_Manager */
	HRESULT Change_Level(uint32 iNewLevelIndex, Shared<class Level> pNewLevel);

public: /* For.Prototype_Manager */
	HRESULT Add_Prototype(uint32 iLevelIndex, const wstring& strPrototypeTag, Shared<Base> pPrototype);	
	Shared<Base> Clone_Prototype(Prototype ePrototype, uint32 iLevelIndex, const wstring& strPrototypeTag, void* pArg = nullptr);

public: /* For.Object_Manager */
	HRESULT Add_GameObject(uint32 iPrototypeLevelIndex, const wstring& strPrototypeTag, uint32 iLayerLevelIndex, const wstring& strLayerTag, void* pArg = nullptr);

public: /* For.Renderer */
	void Add_RenderGroup(RenderGroup eRenderGroup, class GameObject* pGameObject);

public: /* For.PipeLine */
	HRESULT Bind_CamPosition(class Shader* pShader, const char* pConstantName);
	HRESULT Bind_TransformMatrix(D3DTS eTransformState, class Shader* pShader, const char* pConstantName);
	HRESULT Bind_TransformMatrix_Inverse(D3DTS eTransformState, class Shader* pShader, const char* pConstantName);
	const XMFLOAT4X4* Get_Transform(D3DTS eTransformState) const;
	const XMFLOAT4* Get_CamPosition() const;
	void Set_Transform(D3DTS eTransformState, FXMMATRIX TransformStateMatrix);

public: /* For.Input_Manager */
	/* 키가 눌린 상태로 유지되는지 확인할 때 호출한다. */
	bool KeyPress(KEY_TYPE eKey) const;

	/* 키가 이번 프레임에 처음 눌렸는지 확인할 때 호출한다. */
	bool KeyDown(KEY_TYPE eKey) const;

	/* 키가 이번 프레임에 처음 떼졌는지 확인할 때 호출한다. */
	bool KeyUp(KEY_TYPE eKey) const;

	/* 이전 프레임 대비 마우스 이동량을 얻을 때 호출한다. */
	Vec2 Get_MouseDelta() const;

	/* 마우스 고정을 켜야 하는 카메라/에디터 모드에서 호출한다. */
	void Lock_Mouse();

	/* 마우스 고정을 해제해야 하는 카메라/에디터 모드에서 호출한다. */
	void Unlock_Mouse();

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
	Unique<Graphic_Device>		_graphicDevice       = { nullptr };
	Unique<Timer_Manager>		_timerManager        = { nullptr };
	Unique<Level_Manager>		_levelManager        = { nullptr };
	Unique<Prototype_Manager>	_prototypeManager    = { nullptr };
	Unique<Object_Manager>		_objectManager       = { nullptr };
	Unique<Renderer>			_renderer            = { nullptr };
	Unique<PipeLine>			_pipeLine            = { nullptr };
	Unique<Input_Manager>		_inputManager        = { nullptr }; /* Win32 기반 키/마우스 상태를 관리하는 입력 매니저 */
	Unique<Light_Manager>		_lightManager        = { nullptr };
	Unique<Font_Manager>		_fontManager         = { nullptr };
	Unique<Asset_Manager>		_assetManager        = { nullptr }; /* GUID 기반 에셋 registry와 meta/cache를 관리하는 매니저 */

public:	
	void Release_Engine();
	virtual void Free() override;
};

NS_END
