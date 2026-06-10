#pragma once

#include "EffectAuthoring_Types.h"
#include "EffectInstancePool.h"
#include "EffectRuntime_Types.h"
#include "Level.h"

NS_BEGIN(EffectEditor)

class TrailPreviewController;

class Level_EffectEditor final : public Level
{
public:
    Level_EffectEditor(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ~Level_EffectEditor() override;

public:
    HRESULT Initialize() override;
    void Update(float timeDelta) override;
    HRESULT Render() override;

public:
    // 현재 authoring emitter 데이터를 다시 읽어 preview runtime object를 전부 재생성한다.
    HRESULT Restart_PreviewFromAuthoring();
    // 마지막으로 빌드된 preview definition으로 runtime preview만 initial 상태로 되돌린다.
    HRESULT Reset_PreviewRuntime();

private:
    static constexpr auto kPreviewLayerTag{ L"Layer_Preview" };

private:
    struct PreviewTrailStrokeRuntime
    {
        EffectInstancePool::AttachedHandle poolHandle{};
        Shared<bool> sampleEnabled{};
        Shared<IEffectTrailSampleProvider> sampleProvider{};
        bool isDraining{ false };
        float drainElapsed{ 0.f };
        float drainDuration{ 0.25f };
    };

    EffectInstancePool::AttachedHandle _previewPoolHandle{};
    Shared<const EffectDefinition> _currentPreviewDefinition{};
    Shared<const EffectDefinition> _currentPreviewTrailDefinition{};
    vector<PreviewTrailStrokeRuntime> _previewTrailStrokes{};
    bool _wasTrailPreviewGateOpen{ false };
    float _lastTrailPreviewSourceRatio{ 0.f };
    bool _hasTrailPreviewSourceRatio{ false };
    // Trail TypeData preview가 읽을 editor-local sample provider다.
    Weak<IEffectTrailSampleProvider> _trailPreviewSampleProvider{};
    Weak<TrailPreviewController> _trailPreviewController{};

private:
    HRESULT Restart_PreviewFromDefinition(const Shared<const EffectDefinition>& previewDefinition);
    // 현재 authoring emitters를 runtime preview용 EffectDefinition으로 묶는다.
    Shared<const EffectDefinition> Build_PreviewEffectDefinition(const vector<AuthoringEmitter>& emitters, bool includeTrail) const;
    Shared<const EffectDefinition> Build_PreviewTrailEffectDefinition(const vector<AuthoringEmitter>& emitters) const;
    // authoring emitter 1개를 runtime preview용 emitter definition으로 번역한다.
    EffectEmitterDefinition Build_PreviewEmitterDefinition(const AuthoringEmitter& emitter) const;
    EffectEmitterDefinition Build_PreviewTrailEmitterDefinition(const AuthoringEmitter& emitter) const;
    EffectEmitterDefinition Build_PreviewSourceHistoryRibbonEmitterDefinition(const AuthoringEmitter& emitter) const;
    EffectEmitterDefinition Build_PreviewMeshEmitterDefinition(const AuthoringEmitter& emitter) const;
    // preview에 필요한 최소 directional light만 준비한다.
    HRESULT Ready_Lights();
    // EffectEditor preview용 orbit camera를 만들고 active camera로 지정한다.
    HRESULT Ready_Layer_Camera(const wstring& layerTag);
    // EffectEditor world grid를 준비한다.
    HRESULT Ready_Layer_Grid(const wstring& layerTag);
    // EffectEditor preview 전용 skybox를 준비한다.
    HRESULT Ready_Layer_Sky(const wstring& layerTag);
    // preview layer는 authoring emitter 기반 runtime preview만 담는다.
    HRESULT Ready_Layer_Preview(const wstring& layerTag);
    void Update_PreviewTrailStrokes(float timeDelta);
    HRESULT Start_PreviewTrailStroke();
    void Begin_PreviewTrailStrokeDrain(PreviewTrailStrokeRuntime& runtime);
    void Drain_ActivePreviewTrailStrokes();
    void Clear_PreviewTrailStrokes();
    void Clear_PreviewObjects();

public:
    static Shared<Level_EffectEditor> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    void Free() override;
};

NS_END
