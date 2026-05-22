#pragma once

#include "EffectAuthoring_Types.h"
#include "EffectRuntime_Types.h"
#include "Level.h"

NS_BEGIN(Engine)
class GameObject;
NS_END

NS_BEGIN(EffectEditor)

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
    vector<Shared<GameObject>> _previewObjects{};
    Shared<const EffectDefinition> _currentPreviewDefinition{};
    // Trail TypeData preview가 읽을 editor-local sample provider다.
    Weak<IEffectTrailSampleProvider> _trailPreviewSampleProvider{};

private:
    HRESULT Restart_PreviewFromDefinition(const Shared<const EffectDefinition>& previewDefinition);
    // 현재 authoring emitters를 runtime preview용 EffectDefinition으로 묶는다.
    Shared<const EffectDefinition> Build_PreviewEffectDefinition(const vector<AuthoringEmitter>& emitters) const;
    // authoring emitter 1개를 runtime preview용 emitter definition으로 번역한다.
    EffectEmitterDefinition Build_PreviewEmitterDefinition(const AuthoringEmitter& emitter) const;
    EffectEmitterDefinition Build_PreviewTrailEmitterDefinition(const AuthoringEmitter& emitter) const;
    // preview에 필요한 최소 directional light만 준비한다.
    HRESULT Ready_Lights();
    // EffectEditor preview용 orbit camera를 만들고 active camera로 지정한다.
    HRESULT Ready_Layer_Camera(const wstring& layerTag);
    // EffectEditor preview 전용 skybox를 준비한다.
    HRESULT Ready_Layer_Sky(const wstring& layerTag);
    // preview layer는 authoring emitter 기반 runtime preview만 담는다.
    HRESULT Ready_Layer_Preview(const wstring& layerTag);
    void Clear_PreviewObjects();

public:
    static Shared<Level_EffectEditor> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    void Free() override;
};

NS_END
