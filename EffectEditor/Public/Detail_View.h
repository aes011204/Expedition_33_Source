#pragma once
#include "Editor_Window.h"

#include "DetailPropertyContext.h"
#include "EffectAuthoring_Types.h"
#include "Emitter_View.h"
#include "ModuleDetail_Drawers.h"

NS_BEGIN(EffectEditor)

// 선택된 particle system, emitter, TypeData, module의 detail inspector를 그린다.
class Detail_View final : public Editor_Window
{
public:
    Detail_View();
    ~Detail_View() override;

public:
    void Update(float timeDelta) override;
    void Render() override;

private: //## Helper::EffectAuthoring
    // 현재 selection kind에 맞는 authoring detail UI를 분기해서 그린다.
    void Render_EffectAuthoringDetail();
    void Render_ParticleSystemDetail(
        const Shared<Emitter_View>& emitterView,
        const EffectAuthoringSelection& selection,
        ParticleSystemAuthoringData& data);
    void Render_TrailPreviewDetail();
    void Render_PostProcessShaderDetail();
    void Render_EmitterDetail(
        const Shared<Emitter_View>& emitterView,
        const EffectAuthoringSelection& selection,
        AuthoringEmitter& emitter);
    void Render_TypeDataDetail(
        const Shared<Emitter_View>& emitterView,
        const EffectAuthoringSelection& selection,
        AuthoringEmitter& emitter);
    void Render_ModuleDetail(
        const Shared<Emitter_View>& emitterView,
        const EffectAuthoringSelection& selection,
        AuthoringEmitter& emitter,
        AuthoringModule& module);

    // emitter 편집 변경을 preview dirty와 authoring document dirty 상태에 반영한다.
    void Mark_Changed(const Shared<Emitter_View>& emitterView, AuthoringEmitter& emitter, bool changed);
    // 다른 selection으로 이동하기 전에 열려 있던 pending edit을 먼저 history command로 확정한다.
    void Commit_PendingAuthoringEditOnTargetChange(const Shared<Emitter_View>& emitterView, const EffectAuthoringSelection& selection);
    // 첫 변경 시점의 selection/before snapshot/description을 잡아 한 번의 authoring edit으로 묶기 시작한다.
    void Begin_PendingAuthoringEdit(
        const Shared<Emitter_View>& emitterView,
        const EffectAuthoringSelection& selection,
        const Emitter_View::AuthoringSnapshot& beforeSnapshot,
        const string& description);
    // 현재 ImGui item 입력이 끝난 idle 시점에만 pending edit을 commit한다.
    void Commit_PendingAuthoringEditIfIdle(const Shared<Emitter_View>& emitterView);
    // 저장해 둔 before/after snapshot으로 undo 가능한 authoring command를 만든다.
    void Commit_PendingAuthoringEdit(const Shared<Emitter_View>& emitterView);
    static bool Is_SameSelection(const EffectAuthoringSelection& lhs, const EffectAuthoringSelection& rhs);

private: //## Data::ModuleDetail
    DetailPropertyContext _detailPropertyContext{};
    // Detail view의 draw helper가 update tick 시간을 명시적으로 소비할 때 사용한다.
    float _detailTimeDelta{};

    // module data 자체가 아니라 module type별 detail row UI를 그리는 drawer 상태다.
    RequiredModuleDetail _requiredModuleDetail{};
    SpawnModuleDetail _spawnModuleDetail{};
    LifetimeModuleDetail _lifetimeModuleDetail{};
    InitialLocationModuleDetail _initialLocationModuleDetail{};
    SphereLocationModuleDetail _sphereLocationModuleDetail{};
    PlaneRadialLocationModuleDetail _planeRadialLocationModuleDetail{};
    InitialSizeModuleDetail _initialSizeModuleDetail{};
    InitialMeshSizeModuleDetail _initialMeshSizeModuleDetail{};
    InitialVelocityModuleDetail _initialVelocityModuleDetail{};
    InitialRadialVelocityModuleDetail _initialRadialVelocityModuleDetail{};
    VelocityConeModuleDetail _velocityConeModuleDetail{};
    SourceMotionVelocityModuleDetail _sourceMotionVelocityModuleDetail{};
    AccelerationModuleDetail _accelerationModuleDetail{};
    DragModuleDetail _dragModuleDetail{};
    VelocityOverLifeModuleDetail _velocityOverLifeModuleDetail{};
    OrbitOverLifeModuleDetail _orbitOverLifeModuleDetail{};
    InitialRotationModuleDetail _initialRotationModuleDetail{};
    SphereRadialOrientationModuleDetail _sphereRadialOrientationModuleDetail{};
    PlaneRadialOrientationModuleDetail _planeRadialOrientationModuleDetail{};
    RotationOverLifeModuleDetail _rotationOverLifeModuleDetail{};
    SpriteTiltModuleDetail _spriteTiltModuleDetail{};
    SpriteTiltOverLifeModuleDetail _spriteTiltOverLifeModuleDetail{};
    InitialRotationRateModuleDetail _initialRotationRateModuleDetail{};
    RotationRateScaleByLifeModuleDetail _rotationRateScaleByLifeModuleDetail{};
    InitialMeshRotationModuleDetail _initialMeshRotationModuleDetail{};
    MeshRotationOverLifeModuleDetail _meshRotationOverLifeModuleDetail{};
    InitialMeshRotationRateModuleDetail _initialMeshRotationRateModuleDetail{};
    MeshRotationRateScaleByLifeModuleDetail _meshRotationRateScaleByLifeModuleDetail{};
    InitialColorModuleDetail _initialColorModuleDetail{};
    ColorOverLifeModuleDetail _colorOverLifeModuleDetail{};
    SubUVFrameOverLifeModuleDetail _subUvFrameOverLifeModuleDetail{};
    SizeByLifeModuleDetail _sizeByLifeModuleDetail{};
    MeshSizeByLifeModuleDetail _meshSizeByLifeModuleDetail{};
    SpawnPerUnitModuleDetail _spawnPerUnitModuleDetail{};
    SourceHistorySpriteTrailPathFollowModuleDetail _sourceHistorySpriteTrailPathFollowModuleDetail{};
    SourceHistorySpriteTrailPathReplayModuleDetail _sourceHistorySpriteTrailPathReplayModuleDetail{};
    MaterialScalarModulationModuleDetail _materialScalarModulationModuleDetail{};

private: //## Data::AuthoringHistory
    // selection 전환이나 입력 종료 시점까지 하나의 authoring edit command를 지연 관리한다.
    bool _hasPendingAuthoringEdit{ false };
    EffectAuthoringSelection _pendingAuthoringSelection{};
    // 편집 시작 전 authoring 상태 snapshot이다.
    Emitter_View::AuthoringSnapshot _pendingAuthoringSnapshot{};
    string _pendingAuthoringDescription{};

public:
    static Shared<Detail_View> Create();
    void Free() override;
};

NS_END
