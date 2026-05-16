#pragma once

#include "EffectAuthoring_Types.h"
#include "DetailPropertyContext.h"

NS_BEGIN(EffectEditor)

class RequiredModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        const AuthoringModule& module,
        RequiredModuleData& data,
        DetailPropertyContext& detailContext);
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

class InitialVelocityModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialVelocityModuleData& data,
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

NS_END
