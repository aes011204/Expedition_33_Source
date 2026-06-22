#pragma once

#include "EffectAuthoring_Types.h"
#include "Level.h"

NS_BEGIN(EffectEditor)

class EffectEditorPreviewRuntime;

// EffectEditor level host로 preview camera, grid, sky, runtime preview layer를 준비한다.
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
    bool Is_PreviewFinished() const;
    void Apply_PreviewEmitterTransformOverrides(const vector<AuthoringEmitter>& emitters);
    bool Try_GetPreviewEmitterWorldMatrix(uint32 emitterId, Matrix& outWorldMatrix) const;
    // Trail preview 전용 fixture를 필요 시점에만 준비한다.
    HRESULT Ensure_TrailPreviewFixture();

private: //## Static Values
    static constexpr auto kPreviewLayerTag{ L"Layer_Preview" };

private: //## Data::PreviewRuntime
    Unique<EffectEditorPreviewRuntime> _previewRuntime{};
    bool _trailPreviewFixtureReady{ false };

private: //## Helper::Setup
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

public:
    static Shared<Level_EffectEditor> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    void Free() override;
};

NS_END
