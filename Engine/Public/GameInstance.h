#pragma once

#include "Base.h"
#include "Input_Manager.h"
#include "Prototype_Manager.h"
#include "TypeRegistry.h"

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
class Log_Manager;
struct ConsoleLogEntry;

class Component;
class GameObject;
class Level;

class ENGINE_DLL GameInstance final : public Base
{
    DECLARE_SINGLETON(GameInstance)

private:
    GameInstance();

public:
    ~GameInstance() override;

public:
    HRESULT Initialize_Engine(const ENGINE_DESC& engineDesc, ComPtr<Device>& device, ComPtr<Context>& context);
    void    Update_Engine(float timeDelta);
    HRESULT Draw();
    void    Clear_Resources(uint32 levelIndex);

    float   Random(float min, float max);

public: /* For.Log_Manager */
    void    Visit_ConsoleLogs(const function<void(const ConsoleLogEntry& logEntry)>& visitor) const;
    void    Clear_ConsoleLogs();

    uint32  Get_ConsoleLogCount() const;
    uint32  Get_ConsoleLogCapacity() const;
    void    Set_ConsoleLogCapacity(uint32 capacity);
    uint32  Get_ConsoleLogRevision() const;

public: /* For.Graphic_Device */
    HRESULT Clear_Buffers(const Vec4& clearColor);
    HRESULT Present();

    void    Bind_BackBuffer();
    HRESULT Resize_BackBuffer(uint32 width, uint32 height);

    ComPtr<SwapChain> Get_SwapChain() const;

public: /* For.Viewport */
    void    Set_ViewportSize(uint32 width, uint32 height);
    uint32  Get_ViewportWidth() const;
    uint32  Get_ViewportHeight() const;

    // 엔진 백버퍼 기준 viewport 사이즈
    uint32  Get_BackBufferWidth() const;
    uint32  Get_BackBufferHeight() const;

public: /* For.Timer_Manager */
    HRESULT Add_Timer(const wstring& timerTag);
    float   Compute_TimeDelta(const wstring& timerTag);

public: /* For.Level_Manager */
    HRESULT Change_Level(uint32 newLevelIndex, Shared<Level> newLevel);

public: /* For.Prototype_Manager */
    HRESULT Add_Prototype(uint32 levelIndex, const wstring& prototypeTag, Shared<Base> prototype);
    Shared<Base> Clone_Prototype(Prototype prototype, uint32 levelIndex, const wstring& prototypeTag, void* arg = nullptr);

public: /* For.Object_Manager */
    HRESULT Add_GameObject(
        uint32 prototypeLevelIndex,
        const wstring& prototypeTag,
        uint32 layerLevelIndex,
        const wstring& layerTag,
        void* arg = nullptr);

public: /* For.Renderer */
    void Add_RenderGroup(RenderGroup renderGroup, Shared<GameObject> gameObject);

public: /* For.PipeLine */
    HRESULT Bind_CamPosition(Shader* shader, const char* constantName);
    HRESULT Bind_TransformMatrix(D3DTS transformState, Shader* shader, const char* constantName);
    HRESULT Bind_TransformMatrix_Inverse(D3DTS transformState, Shader* shader, const char* constantName);

    const Matrix* Get_Transform(D3DTS transformState) const;
    const Vec4*   Get_CamPosition() const;

    void Set_Transform(D3DTS transformState, FXMMATRIX& transformStateMatrix);

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
    const LIGHT_DESC* Get_LightDesc(uint32 index);
    HRESULT Add_Light(const LIGHT_DESC& lightDesc);

public: /* For.Font_Manager */
    HRESULT Add_Font(const wstring& fontTag, const wchar_t* fontFilePath);
    void Draw_Font(const wstring& fontTag, const wchar_t* text, const Vec2& position, FXMVECTOR color = XMVectorSet(1.f, 1.f, 1.f, 1.f));

public: /* For.Asset_Manager */
    /* 리소스 루트를 기준으로 GUID/meta/cache registry를 준비할 때 호출한다. */
    HRESULT Initialize_AssetManager(const wstring& resourceRoot);

    /* 파일 경로를 GUID로 저장해야 하는 ResourceLoader/Editor 파이프라인에서 호출한다. */
    string Find_AssetGUID(const wstring& filePath) const;

    /* GUID로 에셋 타입과 경로 메타를 조회할 때 호출한다. */
    const FAssetMeta* Find_AssetByGUID(const string& guid) const;

    /* lazy-load가 GUID만 들고 있을 때 실제 파일 경로를 복원하기 위해 호출한다. */
    wstring Resolve_AssetPath(const string& guid) const;

    /* ResourceLoader가 현재 정본 리소스 루트를 절대경로로 해석할 때 호출한다. */
    wstring Get_AssetRoot() const;

public: /* For.TypeRegistry */
    const TypeRegistry::ComponentTypeInfo* Find_ComponentType(const string& className) const;
    const TypeRegistry::GameObjectTypeInfo* Find_GameObjectType(const string& className) const;
    const TypeRegistry::BTNodeTypeInfo* Find_BTNodeType(const string& className) const;

    Shared<Component> Create_ComponentPrototype(const string& className) const;
    Shared<GameObject> Create_GameObjectPrototype(const string& className) const;
    Shared<Base> Create_BTNodePrototype(const string& className) const;

private:
    Unique<Log_Manager>         _logManager{};
    Unique<Graphic_Device>      _graphicDevice{};
    Unique<Timer_Manager>       _timerManager{};
    Unique<Level_Manager>       _levelManager{};
    Unique<Prototype_Manager>   _prototypeManager{};
    Unique<Object_Manager>      _objectManager{};
    Unique<Renderer>            _renderer{};
    Unique<PipeLine>            _pipeLine{};
    Unique<Input_Manager>       _inputManager{};
    Unique<Light_Manager>       _lightManager{};
    Unique<Font_Manager>        _fontManager{};
    Unique<Asset_Manager>       _assetManager{}; /* GUID 기반 에셋 registry와 meta/cache를 관리하는 매니저 */
    Unique<TypeRegistry>        _typeRegistry{};

private:
    uint32 _viewportWidth{ 0 };
    uint32 _viewportHeight{ 0 };

public:
    void Release_Engine();
    void Free() override;
};

NS_END
