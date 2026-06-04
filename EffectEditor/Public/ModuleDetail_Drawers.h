#pragma once

#include "EffectAuthoring_Types.h"
#include "DetailPropertyContext.h"
#include "EffectMaterialPreviewRenderer.h"

NS_BEGIN(EffectEditor)

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

class SpawnModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        SpawnModuleData& data,
        DetailPropertyContext& detailContext);
};

class LifetimeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        LifetimeModuleData& data,
        DetailPropertyContext& detailContext);
};

class InitialSizeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialSizeModuleData& data,
        DetailPropertyContext& detailContext);
};

class InitialMeshSizeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialMeshSizeModuleData& data,
        DetailPropertyContext& detailContext);
};

class InitialLocationModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialLocationModuleData& data,
        DetailPropertyContext& detailContext);
};

class SphereLocationModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        SphereLocationModuleData& data,
        DetailPropertyContext& detailContext);
};

class PlaneRadialLocationModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        PlaneRadialLocationModuleData& data,
        DetailPropertyContext& detailContext);
};

class InitialVelocityModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialVelocityModuleData& data,
        DetailPropertyContext& detailContext);
};

class InitialRadialVelocityModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialRadialVelocityModuleData& data,
        DetailPropertyContext& detailContext);
};

class VelocityConeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        VelocityConeModuleData& data,
        DetailPropertyContext& detailContext);
};

class AccelerationModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        AccelerationModuleData& data,
        DetailPropertyContext& detailContext);
};

class DragModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        DragModuleData& data,
        DetailPropertyContext& detailContext);
};

class VelocityOverLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        VelocityOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class InitialRotationModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialRotationModuleData& data,
        DetailPropertyContext& detailContext);
};

class OrbitOverLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        OrbitOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class PlaneRadialOrientationModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        AuthoringModule& module,
        PlaneRadialOrientationModuleData& data,
        DetailPropertyContext& detailContext);
};

class SphereRadialOrientationModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        AuthoringModule& module,
        SphereRadialOrientationModuleData& data,
        DetailPropertyContext& detailContext);
};

class RotationOverLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        RotationOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class InitialRotationRateModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialRotationRateModuleData& data,
        DetailPropertyContext& detailContext);
};

class RotationRateScaleByLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        RotationRateScaleByLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class InitialMeshRotationModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialMeshRotationModuleData& data,
        DetailPropertyContext& detailContext);
};

class MeshRotationOverLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        MeshRotationOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class InitialMeshRotationRateModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialMeshRotationRateModuleData& data,
        DetailPropertyContext& detailContext);
};

class MeshRotationRateScaleByLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        MeshRotationRateScaleByLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class InitialColorModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialColorModuleData& data,
        DetailPropertyContext& detailContext);
};

class ColorOverLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        ColorOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class SubUVFrameOverLifeModuleDetail
{
public:
    bool Draw(
        const AuthoringEmitter& emitter,
        AuthoringModule& module,
        SubUVFrameOverLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class SizeByLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        SizeByLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class MeshSizeByLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        MeshSizeByLifeModuleData& data,
        DetailPropertyContext& detailContext);
};

class SpawnPerUnitModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        SpawnPerUnitModuleData& data,
        DetailPropertyContext& detailContext);
};

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
