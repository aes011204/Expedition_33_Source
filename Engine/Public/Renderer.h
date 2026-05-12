#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class ShaderCom;
class VIBuffer_Rect;
class GameObject;

class Renderer final : public Base
{
private:
	Renderer(const ComPtr<Device>& device, const ComPtr<Context>& context);

public:
	virtual ~Renderer() = default;

public:
	HRESULT Initialize();
	void    Add_RenderGroup(RenderGroup renderGroup, const Shared<GameObject>& gameObject);
	void    Draw();
    void    Sync_ViewportDependentState(uint32 width, uint32 height);
    uint32  Get_RenderCall() const;


#ifdef _DEBUG
public:
    bool Try_ConsumePerfSummary(float windowSec, RenderPerfSummary& outSummary);
#endif

private:
	void Render_Priority();
    void Render_Shadow();
	void Render_NonBlend();
    void Render_Lights();
    void Render_NonLight();
	void Render_Blend();
	void Render_UI();

    void Render_Combined();

    HRESULT Ready_RenderTargets();
    void Change_ViewportDesc(uint32 width, uint32 height);

    HRESULT Bind_DeferredCB() const;

private:
	ComPtr<Device>  _device;
	ComPtr<Context> _context;

    /* Layer가 소유하는 GameObject를 렌더 프레임 동안만 참조하기 위한 약한 참조 목록. */
	list<Weak<GameObject>>		_renderGroups[ETOI(RenderGroup::END)];

    // shadow
    D3D11_VIEWPORT                  _shadowViewport = {};

    Shared<ShaderCom>               _deferredShader;
    Shared<VIBuffer_Rect>           _viBuffer;

    float                           _shadowScale = 1.f;
    bool                            _shadowEnabled = false;
    uint32                          _shadowMapSize = 0;

    Matrix                          _deferredWorldMatrix;
    Matrix                          _deferredViewMatrix;
    Matrix                          _deferredProjMatrix;
    uint32                          iDrawCall = 0;
    
#ifdef _DEBUG
private:
    struct PerfCounter
    {
        size_t sampleCount{};
        double accumulatedMs{};
        double maxMs{};
    };

    struct RenderPerfWindow
    {
        PerfCounter render{};
        PerfCounter priority{};
        PerfCounter shadow{};
        PerfCounter nonBlend{};
        PerfCounter nonLight{};
        PerfCounter lights{};
        PerfCounter combined{};
        PerfCounter blend{};
        PerfCounter ui{};
        PerfCounter debug{};
    };

    RenderPerfWindow _renderPerfWindow{};

    void Reset_RenderCall();
    void Add_RenderCall(uint32 count = 1);

    void Accumulate_RenderPerf(
        double renderMs,
        double priorityMs,
        double shadowMs,
        double nonBlendMs,
        double lightsMs,
        double combinedMs,
        double nonLightMs,
        double blendMs,
        double uiMs,
        double debugMs);
    static void Accumulate_PassPerf(PerfCounter& counter, double passMs);
    static void Reset_RenderPerfWindow(RenderPerfWindow& window);
    static double Safe_Divide(double numerator, double denominator);

#endif

public:
	static Unique<Renderer> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual void Free() override;

#ifdef _DEBUG

public :
    void Add_DebugRenderGroup(Shared<Component> debugComponent);

private:
    void Render_Debug();

private :
    list<shared_ptr<Component>>     _debugComponents;

#endif
};

#pragma region RENDERER MACRO
#define ENDMRT_RETURN_IF_FAILED(expr)       \
    do                                      \
    {                                       \
        if (FAILED(expr))                   \
        {                                   \
            GAME->End_MRT();                \
            return;                         \
        }                                   \
    } while (false)
#pragma endregion

NS_END


