#pragma once

#include "Base.h"
#include "Log_Define.h"
#include "Renderer_Define.h"

NS_BEGIN(Engine)

class ShaderCom;
class VIBuffer_Rect;
class GameObject;
class PostProcessPipeline;
class LightPipeline;

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
    void    Draw_Preview();
    void    Backup_RenderGroups();
    void    Restore_RenderGroups();
    void    Sync_ViewportDependentState(uint32 width, uint32 height);
    uint32  Get_RenderCall() const;

    const Shared<PostProcessPipeline>   Get_PostProcessPipeline() const
    {
        return _ppPipeline;
    }
    Shared<PostProcessPipeline>    Get_PostProcessPipeline()
    {
        return _ppPipeline;
    }

    void    Set_PreviewDeferredCombine(bool enable) { _usePreviewDeferredCombine = enable; }

#ifdef _DEBUG
public:
    bool Try_ConsumePerfSummary(float windowSec, RenderPerfSummary& outSummary);
#endif

public:
    void Register_StaticShadowCaster(const Shared<GameObject>& gameObject);
    void Mark_StaticShadowDirty();

private:
	void Render_Priority();
    void Render_Shadow();

	void Render_NonBlend();
    void Render_Lights();
    void Render_NonLight();
	void Render_Blend();
	void Render_UI();

    void Render_CombinedHDR();
    void Render_PostProcess();

    HRESULT Ready_RenderTargets();
    void Change_ViewportDesc(uint32 width, uint32 height);

    HRESULT Bind_TransformCB(Shared<ShaderCom> shader) const;

private: /* 그림자 */
    void Update_DynamicShadowCamera();  // 카메라가 플레이어 위치 추적
    void Render_StaticShadowOnce();     // Static Shadow는 한번만
    void Render_DynamicShadow();        // 기존 Render Shadow 로직 이동
    bool Build_StaticShadowBounds(BoundingBox& outBounds) const;
    void Update_StaticShadowCamera(const BoundingBox& bounds);

private:
	ComPtr<Device>                  _device;
	ComPtr<Context>                 _context;
    Shared<PostProcessPipeline>     _ppPipeline;

    /* Layer가 소유하는 GameObject를 렌더 프레임 동안만 참조하기 위한 약한 참조 목록. */
	list<Weak<GameObject>>		_renderGroups[ETOI(RenderGroup::END)];
    list<Weak<GameObject>>      _renderGroupBackup[ETOI(RenderGroup::END)];
    bool                        _hasRenderGroupBackup = false;

    // shadow
    D3D11_VIEWPORT                  _shadowViewport = {};

    Shared<ShaderCom>               _postprocessShader;
    Shared<ShaderCom>               _deferredShader;
    Shared<VIBuffer_Rect>           _viBuffer;

    float                           _shadowScale = 1.f;
    bool                            _shadowEnabled = false;
    uint32                          _shadowMapSize = 0;

    Matrix                          _pipelineWorldMatrix;
    Matrix                          _PipelineViewMatrix;
    Matrix                          _pipelineProjMatrix;
    uint32                          iDrawCall = 0;

    bool                            _usePreviewDeferredCombine = false;
    bool                            _useRTVDump = false;

private:
    list<Weak<GameObject>>          _staticShadowCasters;
    bool                            _staticShadowDirty = true;
    bool                            _staticShadowReady = false;

    uint32                          _shadowMapWidth = 4096;
    uint32                          _shadowMapHeight = 4096;

    uint32                          _staticShadowMapWidth = 4096;
    uint32                          _staticShadowMapHeight = 4096;


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
        PerfCounter combinedHDR{};
        PerfCounter postProcess{};
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
        double combinedHDRMs,
        double postprocessMs,
        double nonLightMs,
        double blendMs,
        double uiMs,
        double debugMs);
    static void Accumulate_PassPerf(PerfCounter& counter, double passMs);
    static void Reset_RenderPerfWindow(RenderPerfWindow& window);
    static double Safe_Divide(double numerator, double denominator);
    void DumpCurrentOMState(const wchar_t* stageTag) const;

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

NS_END


