#pragma once

#include "DetailPropertyContext.h"
#include "EffectAuthoring_Types.h"
#include "EffectMaterialPreviewRenderer.h"

NS_BEGIN(EffectEditor)

// Required module의 renderer/material/common authoring row를 그린다.
class RequiredModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        const AuthoringModule& module,
        RequiredModuleData& data,
        DetailPropertyContext& detailContext,
        float timeDelta);

private:
    Unique<EffectMaterialPreviewRenderer> _materialSlotPreview{};
};

// Spawn module의 rate/burst authoring row를 그린다.
class SpawnModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        const AuthoringModule& module,
        SpawnModuleData& data,
        DetailPropertyContext& detailContext);
};

// Lifetime module의 particle lifetime authoring row를 그린다.
class LifetimeModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        const AuthoringModule& module,
        LifetimeModuleData& data,
        DetailPropertyContext& detailContext);
};

// InitialSize module의 sprite initial size authoring row를 그린다.
class InitialSizeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialSizeModuleData& data,
        DetailPropertyContext& detailContext);
};

// InitialMeshSize module의 mesh initial size authoring row를 그린다.
class InitialMeshSizeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialMeshSizeModuleData& data,
        DetailPropertyContext& detailContext);
};

// InitialLocation module의 emitter-local offset authoring row를 그린다.
class InitialLocationModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialLocationModuleData& data,
        DetailPropertyContext& detailContext);
};

// RibbonOrientation module의 ribbon 폭 방향 기준축 authoring row를 그린다.
class RibbonOrientationModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        RibbonOrientationModuleData& data,
        DetailPropertyContext& detailContext);
};

// SphereLocation module의 sphere spawn distribution authoring row를 그린다.
class SphereLocationModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        const AuthoringModule& module,
        SphereLocationModuleData& data,
        DetailPropertyContext& detailContext);
};

// PlaneRadialLocation module의 plane radial spawn authoring row를 그린다.
class PlaneRadialLocationModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        const AuthoringModule& module,
        PlaneRadialLocationModuleData& data,
        DetailPropertyContext& detailContext);
};

// CylinderLocation module의 원통 spawn 위치 authoring row를 그린다.
class CylinderLocationModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        const AuthoringModule& module,
        CylinderLocationModuleData& data,
        DetailPropertyContext& detailContext);
};

// InitialVelocity module의 initial velocity authoring row를 그린다.
class InitialVelocityModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialVelocityModuleData& data,
        DetailPropertyContext& detailContext);
};

// InitialRadialVelocity module의 radial velocity authoring row를 그린다.
class InitialRadialVelocityModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialRadialVelocityModuleData& data,
        DetailPropertyContext& detailContext);
};

// VelocityCone module의 cone velocity authoring row를 그린다.
class VelocityConeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        VelocityConeModuleData& data,
        DetailPropertyContext& detailContext);
};

// SourceMotionVelocity module의 source/trail motion 기반 초기 속도 row를 그린다.
class SourceMotionVelocityModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        AuthoringModule& module,
        SourceMotionVelocityModuleData& data,
        DetailPropertyContext& detailContext);
};

// Acceleration module의 motion acceleration authoring row를 그린다.
class AccelerationModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        AuthoringModule& module,
        AccelerationModuleData& data,
        DetailPropertyContext& detailContext);
};

// Drag module의 motion drag authoring row를 그린다.
class DragModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        const AuthoringModule& module,
        DragModuleData& data,
        DetailPropertyContext& detailContext);
};

// VelocityOverLife module의 life-based velocity scaling authoring row를 그린다.
class VelocityOverLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        VelocityOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

// InitialRotation module의 sprite initial rotation authoring row를 그린다.
class InitialRotationModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialRotationModuleData& data,
        DetailPropertyContext& detailContext);
};

// OrbitOverLife module의 orbit offset authoring row를 그린다.
class OrbitOverLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        OrbitOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

// PlaneRadialOrientation module의 plane radial orientation authoring row를 그린다.
class PlaneRadialOrientationModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        AuthoringModule& module,
        PlaneRadialOrientationModuleData& data,
        DetailPropertyContext& detailContext);
};

// CylinderOrientation module의 cylinder radial orientation authoring row를 그린다.
class CylinderOrientationModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        AuthoringModule& module,
        CylinderOrientationModuleData& data,
        DetailPropertyContext& detailContext);
};

// SphereRadialOrientation module의 sphere radial orientation authoring row를 그린다.
class SphereRadialOrientationModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        AuthoringModule& module,
        SphereRadialOrientationModuleData& data,
        DetailPropertyContext& detailContext);
};

// RotationOverLife module의 sprite rotation curve authoring row를 그린다.
class RotationOverLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        RotationOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

// SpriteTilt module의 sprite card plane tilt authoring row를 그린다.
class SpriteTiltModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        SpriteTiltModuleData& data,
        DetailPropertyContext& detailContext);
};

// SpriteTiltOverLife module의 life-based sprite card plane tilt authoring row를 그린다.
class SpriteTiltOverLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        SpriteTiltOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

// InitialRotationRate module의 sprite angular velocity authoring row를 그린다.
class InitialRotationRateModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialRotationRateModuleData& data,
        DetailPropertyContext& detailContext);
};

// RotationRateScaleByLife module의 sprite angular velocity scale authoring row를 그린다.
class RotationRateScaleByLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        RotationRateScaleByLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

// InitialMeshRotation module의 mesh initial rotation authoring row를 그린다.
class InitialMeshRotationModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialMeshRotationModuleData& data,
        DetailPropertyContext& detailContext);
};

// MeshRotationOverLife module의 mesh rotation curve authoring row를 그린다.
class MeshRotationOverLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        MeshRotationOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

// InitialMeshRotationRate module의 mesh angular velocity authoring row를 그린다.
class InitialMeshRotationRateModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialMeshRotationRateModuleData& data,
        DetailPropertyContext& detailContext);
};

// MeshRotationRateScaleByLife module의 mesh angular velocity scale authoring row를 그린다.
class MeshRotationRateScaleByLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        MeshRotationRateScaleByLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

// InitialColor module의 start color/alpha authoring row를 그린다.
class InitialColorModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        const AuthoringModule& module,
        InitialColorModuleData& data,
        DetailPropertyContext& detailContext);
};

// ColorOverLife module의 life-based color/alpha authoring row를 그린다.
class ColorOverLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        const AuthoringModule& module,
        ColorOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

// SubUVFrameOverLife module의 frame selection authoring row를 그린다.
class SubUVFrameOverLifeModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        AuthoringModule& module,
        SubUVFrameOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

// SizeByLife module의 sprite scale-over-life authoring row를 그린다.
class SizeByLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        SizeByLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

// BeamEnvelopeOverLife module의 beam visible length/width envelope row를 그린다.
class BeamEnvelopeOverLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        BeamEnvelopeOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

// MeshSizeByLife module의 mesh scale-over-life authoring row를 그린다.
class MeshSizeByLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        MeshSizeByLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

// SpawnPerUnit module의 distance-based spawn authoring row를 그린다.
class SpawnPerUnitModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        SpawnPerUnitModuleData& data,
        DetailPropertyContext& detailContext);
};

// SourceHistorySpriteTrail PathFollow module의 path-follow stamp motion row를 그린다.
class SourceHistorySpriteTrailPathFollowModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        SourceHistorySpriteTrailPathFollowModuleData& data,
        DetailPropertyContext& detailContext);
};

// SourceHistorySpriteTrail PathReplay module의 path-replay stamp motion row를 그린다.
class SourceHistorySpriteTrailPathReplayModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        SourceHistorySpriteTrailPathReplayModuleData& data,
        DetailPropertyContext& detailContext);
};

// MaterialScalarModulation module의 material parameter modulation authoring row를 그린다.
class MaterialScalarModulationModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        AuthoringModule& module,
        MaterialScalarModulationModuleData& data,
        DetailPropertyContext& detailContext);
};

NS_END
