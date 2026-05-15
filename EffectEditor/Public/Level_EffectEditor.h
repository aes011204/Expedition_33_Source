#pragma once

#include "ComputePointParticleEffect.h"
#include "EffectAuthoring_Types.h"
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

private:
    static constexpr const wchar_t* kPreviewLayerTag{ L"Layer_Preview" };

private:
    vector<Shared<GameObject>> _previewObjects{};

private:
    // authoring emitter를 compute sprite preview runtime desc로 변환한다.
    ComputePointParticleEffect::ComputePointParticleEffectDesc Build_ComputePointParticleEffectDesc(const AuthoringEmitter& emitter) const;
    // preview에 필요한 최소 directional light만 준비한다.
    HRESULT Ready_Lights();
    // EffectEditor preview용 orbit camera를 만들고 active camera로 지정한다.
    HRESULT Ready_Layer_Camera(const wstring& layerTag);
    // preview layer는 authoring emitter 기반 runtime preview만 담는다.
    HRESULT Ready_Layer_Preview(const wstring& layerTag);
    void Clear_PreviewObjects();

public:
    static Shared<Level_EffectEditor> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    void Free() override;
};

NS_END
