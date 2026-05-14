#pragma once

#include "Editor_Window.h"
#include "Emitter_View.h"
#include "EffectAuthoring_Types.h"
#include "ModuleDetail_Drawers.h"

NS_BEGIN(EffectEditor)

class Detail_View final : public Editor_Window
{
public:
    Detail_View();
    ~Detail_View() override;

public:
    void Update(float timeDelta) override;
    void Render() override;

private: //## Helper::EffectAuthoring
    void Render_EffectAuthoringDetail();
    void Render_ParticleSystemDetail(const Shared<Emitter_View>& emitterView, const EffectAuthoringSelection& selection, ParticleSystemAuthoringData& data);
    void Render_EmitterDetail(const Shared<Emitter_View>& emitterView, const EffectAuthoringSelection& selection, AuthoringEmitter& emitter);
    void Render_ModuleDetail(const Shared<Emitter_View>& emitterView, const EffectAuthoringSelection& selection, AuthoringEmitter& emitter, AuthoringModule& module);

    void Mark_Changed(AuthoringEmitter& emitter, bool changed);
    static RequiredModuleData* Find_RequiredModuleData(AuthoringEmitter& emitter);
    void Commit_PendingAuthoringEditOnTargetChange(const Shared<Emitter_View>& emitterView, const EffectAuthoringSelection& selection);
    void Begin_PendingAuthoringEdit(
        const Shared<Emitter_View>& emitterView,
        const EffectAuthoringSelection& selection,
        const Emitter_View::AuthoringSnapshot& beforeSnapshot,
        const string& description);
    void Commit_PendingAuthoringEditIfIdle(const Shared<Emitter_View>& emitterView);
    void Commit_PendingAuthoringEdit(const Shared<Emitter_View>& emitterView);
    static bool Is_SameSelection(const EffectAuthoringSelection& lhs, const EffectAuthoringSelection& rhs);

private: //## Data::ModuleDetail
    RequiredModuleDetail _requiredModuleDetail{};
    SpawnModuleDetail _spawnModuleDetail{};
    LifetimeModuleDetail _lifetimeModuleDetail{};
    InitialSizeModuleDetail _initialSizeModuleDetail{};
    InitialVelocityModuleDetail _initialVelocityModuleDetail{};
    InitialColorModuleDetail _initialColorModuleDetail{};
    ColorOverLifeModuleDetail _colorOverLifeModuleDetail{};

private: //## Data::AuthoringHistory
    bool _hasPendingAuthoringEdit{ false };
    EffectAuthoringSelection _pendingAuthoringSelection{};
    Emitter_View::AuthoringSnapshot _pendingAuthoringSnapshot{};
    string _pendingAuthoringDescription{};

public:
    static Shared<Detail_View> Create();
    void Free() override;
};

NS_END
