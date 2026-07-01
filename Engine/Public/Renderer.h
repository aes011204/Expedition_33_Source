#pragma once

#include "Base.h"
#include "Engine_Constants.h"
#include "Log_Define.h"
#include "Renderer_Define.h"
#include "Shader_CBuffer_Define.h"

NS_BEGIN(Engine)

class ShaderCom;
class VIBuffer_Rect;
class GameObject;
class PostProcessPipeline;
class LightingPipeline;
class LightPipeline;
struct DOFPreset;
struct DOFTransitionDesc;

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
    void    Backup_RenderGroups();
    void    Restore_RenderGroups();
    void    Sync_ViewportDependentState(uint32 width, uint32 height);
    uint32  Get_RenderCall() const;
    bool    Try_GetRenderViewHotspotSnapshot(RenderViewHotspotSnapshot& outSnapshot) const;
    void    Set_ShadowEnabled(bool enabled) { _shadowEnabled = enabled; }
    bool    Is_ShadowEnabled() const { return _shadowEnabled; }
    void    Set_RenderOnlyPreviewMode(bool enabled) { _renderOnlyPreviewMode = enabled; }
    bool    Is_RenderOnlyPreviewMode() const { return _renderOnlyPreviewMode; }
    void    Set_RenderEnvironment(bool enabled) { _renderEnvironment = enabled; }
    bool    Is_RenderEnvironment() const { return _renderEnvironment; }
    void    Set_UseInstancedShadowCache(bool enabled) { _useCachedInstancedShadow = enabled; }
    bool    Is_UseInstancedShadowCache() const { return _useCachedInstancedShadow; }
    void    Request_BakeCachedInstancedShadow() { _hasInstancedShadowCache = false; }
    bool    Has_InstancedShadowCache() const { return _hasInstancedShadowCache; }
    const Matrix& Get_CachedShadowViewProj() const { return _cachedShadowViewProj; }

    const Shared<PostProcessPipeline>& Get_PostProcessPipeline() const
    {
        return _ppPipeline;
    }
    const Shared<PostProcessPipeline>& Get_PostProcessPipeline()
    {
        return _ppPipeline;
    }
    const Shared<LightingPipeline>& Get_LightingPipeline() const
    {
        return _lightingPipeline;
    }
    const Shared<LightingPipeline>& Get_LightingPipeline()
    {
        return _lightingPipeline;
    }
    HRESULT Reload_RendererShaders();
    HRESULT Reload_LightingPipelineShaders();
    HRESULT Begin_DOFOverrideScope(const string& ownerTag);
    HRESULT Set_DOFOverridePreset(const string& ownerTag, const DOFPreset& preset, const DOFTransitionDesc& transition);
    HRESULT Set_DOFOverrideFocus(const string& ownerTag, float focusDistance, float focusRange);
    HRESULT Clear_DOFOverrideToBaseline(const string& ownerTag, const DOFTransitionDesc& transition);
    HRESULT End_DOFOverrideScope(const string& ownerTag, const DOFTransitionDesc& transition);
    RenderGroup Get_CurrentGroup() const
    {
        return _currentGroup;
    }
    
    void Set_LetterboxEnabled(bool setLetterboxEnabled)
    {
        const bool wasActive = _letterboxEnabled || _previewLetterboxEnabled;
        _letterboxEnabled = setLetterboxEnabled;
        const bool isActive = _letterboxEnabled || _previewLetterboxEnabled;
        if (!wasActive && isActive)
            _letterboxCurrentRatio = 0.f;
        _letterboxTargetRatio = isActive ? 0.12f : 0.f;
    }
    void Set_PreviewLetterboxEnabled(bool enabled)
    {
        const bool wasActive = _letterboxEnabled || _previewLetterboxEnabled;
        _previewLetterboxEnabled = enabled;
        const bool isActive = _letterboxEnabled || _previewLetterboxEnabled;
        if (!wasActive && isActive)
            _letterboxCurrentRatio = 0.f;
        _letterboxTargetRatio = isActive ? 0.12f : 0.f;
    }
    bool Is_PreviewLetterboxEnabled() const { return _previewLetterboxEnabled; }

#ifdef _DEBUG
    bool Try_ConsumePerfSummary(float windowSec, RenderPerfSummary& outSummary);
#endif

public:
    void PostDraw();
    void Render_Letterbox();

private:
	void Render_Priority();
    void Render_Shadow();

	void Render_NonBlend();
    void Render_SSAO();
    void Render_Lights();
    void Render_AmbientVisibility();
    void Render_NonLight();
    void Render_EffectMasked(); // alpha cutout effect를 HDR에 depth-write opaque-like로 출력한다.
    void Render_Distortion(); // HDRColor snapshot과 distortion field를 사용해 fullscreen 왜곡 resolve를 수행한다.
	void Render_Blend();
    void Render_Glow();
    void Render_ScreenFx();
    void Render_UIEffect();

    void Render_CombinedHDR();
    void Render_Fog();
    void Render_VolumetricLightShaft();
    void Render_PostProcess();
    

    HRESULT Ready_Shaders();
    void Change_ViewportDesc(uint32 width, uint32 height);

    HRESULT Bind_ScreenSpaceCB(Shared<ShaderCom> shader, uint32 width = 0, uint32 height = 0) const;

private :
    HRESULT Render_BloomPass(const wchar_t* dstMrt, const wchar_t* srcRt, PostProcessPass pass);
    HRESULT Render_BloomUpsamplePass(const wchar_t* dstMrt, const wchar_t* smallRt, const wchar_t* baseRt);
    HRESULT Render_LightShaftPass(const wchar_t* dstMrt, const char* srcResourceName, const wchar_t* srcRt, PostProcessPass pass);
    HRESULT Render_FroxelCompositePass();
    HRESULT Render_LightShaftCompositePass();
    HRESULT Render_DOFPreFilterPass();
    HRESULT Render_DOFBlurPass(const wchar_t* dstMrt, const char* srcResourceName, const wchar_t* srcRt, PostProcessPass pass);
    HRESULT Render_DOFCompositePass();
    HRESULT Render_PointLightGlow();
    bool Try_BuildLightShaftParams(LightShaftCB& outParams) const;
    HRESULT Render_DistortionResolve();

private: /* 그림자 */
    void Update_DynamicShadowCamera();  // 카메라가 플레이어 위치 추적
    void Render_DynamicShadow();        // 기존 Render Shadow 로직 이동
    bool Should_UpdateDynamicShadowCascade(uint32 cascadeIndex) const;
    bool Should_RenderDynamicShadowObject(const Shared<GameObject>& gameObject, uint32 cascadeIndex) const;
    void Bake_CachedInstancedShadow();
#ifdef _DEBUG
    void Sync_ShadowDebugTargets();
#endif

private:
    ComPtr<Device>                  _device;
    ComPtr<Context>                 _context;
    Shared<PostProcessPipeline>     _ppPipeline;
    Shared<LightingPipeline>        _lightingPipeline;

    /* Layer가 소유하는 GameObject를 렌더 프레임 동안만 참조하기 위한 약한 참조 목록. */
	list<Weak<GameObject>>		_renderGroups[ETOI(RenderGroup::END)];
    list<Weak<GameObject>>      _renderGroupBackup[ETOI(RenderGroup::END)];
    bool                        _hasRenderGroupBackup = false;

    // shadow
    D3D11_VIEWPORT                  _shadowViewport = {};

    Shared<ShaderCom>               _postprocessShader;
    Shared<ShaderCom>               _screenFxShader;
    Shared<ShaderCom>               _deferredShader;
    Shared<VIBuffer_Rect>           _viBuffer;
    enum class IBLDebugMode : uint8 { Default = 0, SpecularOnly, DiffuseOnly };
    IBLDebugMode                    _iblDebugMode = IBLDebugMode::Default;

    bool                            _shadowEnabled = true;
    bool                            _renderOnlyPreviewMode = false;
    bool                            _renderEnvironment = false;

    bool                            _useCachedInstancedShadow = false;
    bool                            _hasInstancedShadowCache = false;
    Matrix                          _cachedShadowViewProj = {};

    Matrix                          _pipelineWorldMatrix;
    Matrix                          _PipelineViewMatrix;
    Matrix                          _pipelineProjMatrix;
    uint32                          iDrawCall = 0;

    bool                            _useRTVDump = false;

    /* letterbox용 */
    float             _letterboxCurrentRatio = 0.f;
    float             _letterboxTargetRatio = 0.12f;
    Shared<ShaderCom> _letterboxShader;
    bool              _letterboxEnabled = false;
    bool              _previewLetterboxEnabled = false;

private:
    uint64                          _shadowFrameIndex = 0;

    ViewportSize                    _screenSize{};
    ViewportSize                    _shadowMapSize = { 2048, 2048 };

    RenderGroup                     _currentGroup = RenderGroup::END;

private:
    void Reset_RenderCall();
    void Add_RenderCall(uint32 count = 1);
    void Capture_RenderViewHotspotCandidate(RenderGroup renderGroup, const Shared<GameObject>& gameObject);
    void Publish_RenderViewHotspotSnapshot();

#ifdef _DEBUG
private:
    struct PerfCounter
    {
        size_t sampleCount{};
        double accumulatedMs{};
        double maxMs{};
    };

    RenderPerfWindow _renderPerfWindow{};
    chrono::steady_clock::time_point _lastPerfSampleTime{};
    bool _hasLastPerfSampleTime{ false };

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

    void DumpCurrentOMState(const wchar_t* stageTag) const;

#endif

private:
    RenderViewHotspotSnapshot _hotspotPendingSnapshot = {};
    RenderViewHotspotSnapshot _hotspotLastSnapshot = {};

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


