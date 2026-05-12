#pragma once

#include "Editor_Window.h"
#include "EffectAuthoring_Types.h"
#include "ModuleDetail_Drawers.h"

NS_BEGIN(EffectEditor)

class Detail_View final : public Editor_Window
{
public:
    // 선택된 오브젝트와 effect authoring 속성을 보여줄 인스펙터 창을 생성한다.
    Detail_View();
    ~Detail_View() override;

public:
    void Update(float timeDelta) override;
    void Render() override;

private: //## Helper::EffectAuthoring
    void Render_EffectAuthoringDetail();
    void Render_ParticleSystemDetail(ParticleSystemAuthoringData& data);
    void Render_EmitterDetail(AuthoringEmitter& emitter);
    void Render_ModuleDetail(AuthoringEmitter& emitter, AuthoringModule& module);

    void Mark_Changed(AuthoringEmitter& emitter, bool changed);
    static RequiredModuleData* Find_RequiredModuleData(AuthoringEmitter& emitter);

private:
    RequiredModuleDetail _requiredModuleDetail{};
    SpawnModuleDetail _spawnModuleDetail{};
    LifetimeModuleDetail _lifetimeModuleDetail{};
    InitialSizeModuleDetail _initialSizeModuleDetail{};
    InitialVelocityModuleDetail _initialVelocityModuleDetail{};
    InitialColorModuleDetail _initialColorModuleDetail{};
    ColorOverLifeModuleDetail _colorOverLifeModuleDetail{};

public:
    static Shared<Detail_View> Create();
    void Free() override;
};

NS_END
