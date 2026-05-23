#include "VIBufferCom_ComputePointParticle.h"

#include "ComputeShaderCom.h"
#include "ComputeStructuredBuffer.h"
#include "GameInstance.h"

struct ComputePointParticleParams
{
    float elapsedTime{};
    float deltaTime{};
    float instanceCount{};
    uint32 drawMode{};
    uint32 spawnRequest{};
    uint32 maxActiveCount{};
    uint32 maxDrawCount{};
    uint32 killActiveParticles{};
    Vec4 center{};
    Vec4 emitterBasisRight{}; // xyz: emitter local X axis in world space.
    Vec4 emitterBasisUp{};    // xyz: emitter local Y axis in world space.
    Vec4 emitterBasisLook{};  // xyz: emitter local Z axis in world space.
    Vec4 spawnRange{};    // xyz: spawn range, w: unused.
    Vec4 initialLocationMin{}; // xyz: InitialLocation min offset, w: enabled.
    Vec4 initialLocationMax{}; // xyz: InitialLocation max offset, w: unused.
    Vec4 sphereLocationOffsetRadius{}; // xyz: sphere center offset, w: radius.
    Vec4 sphereLocationParams{};       // x: enabled, y: mode, z/w: unused.
    Vec4 sizeMin{};       // x/y: width/height min, z/w: unused.
    Vec4 sizeMax{};       // x/y: width/height max, z/w: unused.
    Vec4 lifeTime{};      // x/y: lifetime min/max, z/w: unused.
    Vec4 startColorMin{};
    Vec4 startColorMax{};
    Vec4 endColorMin{};
    Vec4 endColorMax{};
    Vec4 subUVParams{};         // x/y: rows/cols, z: playback mode, w: frames per second.
    Vec4 subUVFrameParams{};    // x/y: start/end frame, z: loop, w: enabled.
    Vec4 subUVFrameCurveTimes{};  // 후속 Curve Editor용 frame curve key times.
    Vec4 subUVFrameCurveValues{}; // 후속 Curve Editor용 frame curve frame values.
    Vec4 subUVFrameCurveParams{}; // x: key count, y/z/w: unused.
    Vec4 sizeByLifeParams{};    // x/y: width start/end, z/w: height start/end.
    Vec4 motionFlags{};         // x: motion enabled, y: vector enabled, z: vector world, w: radial enabled.
    Vec4 motionSpaceFlags{};    // x: radial world, y/z/w: unused.
    Vec4 initialVelocityMin{};  // xyz: vector velocity min, w: unused.
    Vec4 initialVelocityMax{};  // xyz: vector velocity max, w: unused.
    Vec4 motionRadialPivot{};   // xyz: radial pivot, w: unused.
    Vec4 motionRadialSpeedDrag{}; // x/y: radial speed min/max, z/w: drag min/max.
    Vec4 accelerationMin{};     // xyz: acceleration min, w: unused.
    Vec4 accelerationMax{};     // xyz: acceleration max, w: unused.
    Vec4 velocityScaleByLife{}; // x/y: scale start/end, z/w: unused.
    Vec4 rotationInitialRate{}; // x/y: initial rotation min/max, z/w: rate min/max.
    Vec4 rotationByLife{};      // x/y: rotation start/end, z/w: rate scale start/end.
};

VIBufferCom_ComputePointParticle::VIBufferCom_ComputePointParticle(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : VIBufferCom_Instance{ device, context }
{
}

VIBufferCom_ComputePointParticle::VIBufferCom_ComputePointParticle(const VIBufferCom_ComputePointParticle& prototype)
    : VIBufferCom_Instance{ prototype }
{
}

VIBufferCom_ComputePointParticle::~VIBufferCom_ComputePointParticle()
{
    Free();
}

HRESULT VIBufferCom_ComputePointParticle::Initialize_Prototype()
{
    _numVertexBuffers = 2;
    _numVertices = 1;
    _vertexStride = sizeof(Vec3);
    _numIndices = 0;
    _indexStride = 0;
    _primitiveType = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;

    _instanceStride = sizeof(ParticleInstanceVertex);
    _verticesPerInstance = 1;

    return Ready_VertexBuffer();
}

HRESULT VIBufferCom_ComputePointParticle::Initialize(void* arg)
{
    if (nullptr == arg)
    {
        LOG_ERROR("VIBufferCom_ComputePointParticle requires ComputePointParticleDesc at clone time.");
        return E_FAIL;
    }

    _desc = *static_cast<ComputePointParticleDesc*>(arg);

    if (_desc.instanceCount == 0)
    {
        LOG_ERROR("VIBufferCom_ComputePointParticle requires at least one instance.");
        return E_FAIL;
    }

    _instanceCount = _desc.instanceCount;

    _instanceBufferDesc = {
        .ByteWidth = _instanceStride * _instanceCount,
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        .CPUAccessFlags = 0,
        .StructureByteStride = _instanceStride
    };

    CHECK_FAILED(Ready_ComputeShader(), E_FAIL);
    CHECK_FAILED(Ready_ComputeOutput(), E_FAIL);
    CHECK_FAILED(Ready_LifecycleState(), E_FAIL);
    CHECK_FAILED(Ready_InstanceBuffer(), E_FAIL);
    CHECK_FAILED(Ready_IndexedDrawBuffer(), E_FAIL);
    CHECK_FAILED(Ready_IndirectArgsBuffers(), E_FAIL);
    CHECK_FAILED(Ready_ConstantBuffer(), E_FAIL);
    CHECK_FAILED(Update_ConstantBuffer(), E_FAIL);

    CHECK_FAILED(_computeShader->Bind_UAV(0, _computeOutput->Get_UAV()), E_FAIL);
    if (nullptr != _computeArgsOutput)
        CHECK_FAILED(_computeShader->Bind_UAV(1, _computeArgsOutput->Get_UAV()), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_UAV(2, _lifecycleState->Get_UAV()), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_ConstantBuffer(0, _computeConstantBuffer.Get()), E_FAIL);
    CHECK_FAILED(Reset_IndirectArgs(), E_FAIL);
    CHECK_FAILED(_computeShader->Dispatch((_instanceCount + kThreadCountX - 1) / kThreadCountX, 1, 1), E_FAIL);
    CHECK_FAILED(Copy_OutputToInstanceVB(), E_FAIL);
    CHECK_FAILED(Copy_OutputToIndirectArgs(), E_FAIL);

    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Render()
{
    if (_instanceCount == 0)
        return E_FAIL;

    switch (_desc.drawMode)
    {
    case PointParticleDrawMode::Direct:
        return __super::Render();

    case PointParticleDrawMode::DrawInstancedIndirect:
        if (nullptr == _indirectArgsBuffer)
            return E_FAIL;

        _context->DrawInstancedIndirect(_indirectArgsBuffer.Get(), 0);
        return S_OK;

    case PointParticleDrawMode::DrawIndexedInstancedIndirect:
        if (nullptr == _indirectArgsBuffer || nullptr == _ib)
            return E_FAIL;

        _context->DrawIndexedInstancedIndirect(_indirectArgsBuffer.Get(), 0);
        return S_OK;

    default:
        return E_FAIL;
    }
}

void VIBufferCom_ComputePointParticle::Dispatch_Compute(float timeDelta, uint32 spawnRequest, bool killActiveParticles)
{
    if (nullptr == _computeShader || nullptr == _computeOutput || nullptr == _lifecycleState || nullptr == _instanceVB)
        return;

    _deltaTime = max(0.f, timeDelta);
    _elapsedTime += _deltaTime;
    _spawnRequest = spawnRequest;
    _killActiveParticles = killActiveParticles ? 1u : 0u;

    CHECK_FAILED(Update_ConstantBuffer());
    CHECK_FAILED(_computeShader->Bind_UAV(0, _computeOutput->Get_UAV()));
    if (nullptr != _computeArgsOutput)
        CHECK_FAILED(_computeShader->Bind_UAV(1, _computeArgsOutput->Get_UAV()));
    CHECK_FAILED(_computeShader->Bind_UAV(2, _lifecycleState->Get_UAV()));
    CHECK_FAILED(_computeShader->Bind_ConstantBuffer(0, _computeConstantBuffer.Get()));
    CHECK_FAILED(Reset_IndirectArgs());
    CHECK_FAILED(_computeShader->Dispatch((_instanceCount + kThreadCountX - 1) / kThreadCountX, 1, 1));
    CHECK_FAILED(Copy_OutputToInstanceVB());
    CHECK_FAILED(Copy_OutputToIndirectArgs());
}

void VIBufferCom_ComputePointParticle::Set_Center(const Vec3& center)
{
    _desc.center = center;
}

void VIBufferCom_ComputePointParticle::Set_EmitterBasis(const Vec3& right, const Vec3& up, const Vec3& look)
{
    _desc.emitterRight = right;
    _desc.emitterUp = up;
    _desc.emitterLook = look;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_VertexBuffer()
{
    const D3D11_BUFFER_DESC vertexBufferDesc = {
        .ByteWidth = _vertexStride * _numVertices,
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        .StructureByteStride = _vertexStride
    };

    const Vec3 vertex{ 0.f, 0.f, 0.f };

    const D3D11_SUBRESOURCE_DATA initialData = {
        .pSysMem = &vertex
    };

    CHECK_FAILED(_device->CreateBuffer(&vertexBufferDesc, &initialData, _vb.GetAddressOf()), E_FAIL);
    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_ComputeShader()
{
    if (_desc.computeShaderPrototypeTag.empty())
        return E_FAIL;

    _computeShader = dynamic_pointer_cast<ComputeShaderCom>(
        GAME->Clone_Prototype(
            Prototype::Comopnent,
            _desc.computeShaderLevelIndex,
            _desc.computeShaderPrototypeTag
        )
    );
    CHECK_NULL(_computeShader, E_FAIL);

    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_ComputeOutput()
{
    _computeOutput = ComputeStructuredBuffer::Create(
        _device,
        _context,
        sizeof(ParticleInstanceVertex),
        _instanceCount
    );
    CHECK_NULL(_computeOutput, E_FAIL);

    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_LifecycleState()
{
    vector<ParticleLifecycleState> states{};
    states.reserve(_instanceCount);

    for (uint32 index = 0; index < _instanceCount; ++index)
        states.push_back(Make_InitialLifecycleState(index));

    _lifecycleState = ComputeStructuredBuffer::Create(
        _device,
        _context,
        sizeof(ParticleLifecycleState),
        _instanceCount,
        states.data()
    );
    CHECK_NULL(_lifecycleState, E_FAIL);

    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_InstanceBuffer()
{
    CHECK_FAILED(_device->CreateBuffer(&_instanceBufferDesc, nullptr, _instanceVB.GetAddressOf()), E_FAIL);
    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_IndexedDrawBuffer()
{
    if (PointParticleDrawMode::DrawIndexedInstancedIndirect != _desc.drawMode)
        return S_OK;

    _numIndices = 1;
    _indexStride = sizeof(uint16);

    const D3D11_BUFFER_DESC indexBufferDesc = {
        .ByteWidth = _indexStride * _numIndices,
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_INDEX_BUFFER,
        .StructureByteStride = _indexStride
    };

    const uint16 index = 0;
    const D3D11_SUBRESOURCE_DATA initialData = {
        .pSysMem = &index
    };

    CHECK_FAILED(_device->CreateBuffer(&indexBufferDesc, &initialData, _ib.GetAddressOf()), E_FAIL);
    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_IndirectArgsBuffers()
{
    _computeArgsOutput = ComputeStructuredBuffer::Create(
        _device,
        _context,
        sizeof(ComputeIndirectArgs),
        1
    );
    CHECK_NULL(_computeArgsOutput, E_FAIL);

    const D3D11_BUFFER_DESC argsBufferDesc = {
        .ByteWidth = sizeof(ComputeIndirectArgs),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = 0,
        .CPUAccessFlags = 0,
        .MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS
    };

    CHECK_FAILED(_device->CreateBuffer(&argsBufferDesc, nullptr, _indirectArgsBuffer.GetAddressOf()), E_FAIL);

    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_ConstantBuffer()
{
    const D3D11_BUFFER_DESC bufferDesc = {
        .ByteWidth = sizeof(ComputePointParticleParams),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER
    };

    CHECK_FAILED(_device->CreateBuffer(&bufferDesc, nullptr, _computeConstantBuffer.GetAddressOf()), E_FAIL);
    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Update_ConstantBuffer()
{
    if (nullptr == _computeConstantBuffer)
        return E_FAIL;

    const ComputePointParticleParams params = {
        .elapsedTime = _elapsedTime,
        .deltaTime = _deltaTime,
        .instanceCount = static_cast<float>(_instanceCount),
        .drawMode = ETOI(_desc.drawMode),
        .spawnRequest = _spawnRequest,
        .maxActiveCount = min(_instanceCount, max(1u, _desc.spawn.maxActiveCount)),
        .maxDrawCount = _desc.useMaxDrawCount ? min(_instanceCount, max(1u, _desc.maxDrawCount)) : _instanceCount,
        .killActiveParticles = _killActiveParticles,
        .center = Vec4{ _desc.center.x, _desc.center.y, _desc.center.z, 1.f },
        .emitterBasisRight = Vec4{ _desc.emitterRight.x, _desc.emitterRight.y, _desc.emitterRight.z, 0.f },
        .emitterBasisUp = Vec4{ _desc.emitterUp.x, _desc.emitterUp.y, _desc.emitterUp.z, 0.f },
        .emitterBasisLook = Vec4{ _desc.emitterLook.x, _desc.emitterLook.y, _desc.emitterLook.z, 0.f },
        .spawnRange = Vec4{ _desc.range.x, _desc.range.y, _desc.range.z, 0.f },
        .initialLocationMin = Vec4{
            _desc.initialLocation.minOffset.x,
            _desc.initialLocation.minOffset.y,
            _desc.initialLocation.minOffset.z,
            _desc.initialLocation.enabled ? 1.f : 0.f
        },
        .initialLocationMax = Vec4{
            _desc.initialLocation.maxOffset.x,
            _desc.initialLocation.maxOffset.y,
            _desc.initialLocation.maxOffset.z,
            0.f
        },
        .sphereLocationOffsetRadius = Vec4{
            _desc.sphereLocation.offset.x,
            _desc.sphereLocation.offset.y,
            _desc.sphereLocation.offset.z,
            max(0.f, _desc.sphereLocation.radius)
        },
        .sphereLocationParams = Vec4{
            _desc.sphereLocation.enabled ? 1.f : 0.f,
            static_cast<float>(ETOI(_desc.sphereLocation.mode)),
            0.f,
            0.f
        },
        .sizeMin = Vec4{ _desc.sizeMin.x, _desc.sizeMin.y, 0.f, 0.f },
        .sizeMax = Vec4{ _desc.sizeMax.x, _desc.sizeMax.y, 0.f, 0.f },
        .lifeTime = Vec4{ _desc.lifeTime.x, _desc.lifeTime.y, 0.f, 0.f },
        .startColorMin = _desc.startColorMin,
        .startColorMax = _desc.startColorMax,
        .endColorMin = _desc.endColorMin,
        .endColorMax = _desc.endColorMax,
        .subUVParams = Vec4{
            static_cast<float>(max(1u, _desc.subUVRows)),
            static_cast<float>(max(1u, _desc.subUVCols)),
            static_cast<float>(ETOI(_desc.subUvFrameOverLife.playbackMode)),
            max(0.f, _desc.subUvFrameOverLife.framesPerSecond)
        },
        .subUVFrameParams = Vec4{
            static_cast<float>(_desc.subUvFrameOverLife.startFrame),
            static_cast<float>(_desc.subUvFrameOverLife.endFrame),
            _desc.subUvFrameOverLife.loop ? 1.f : 0.f,
            _desc.subUvFrameOverLife.enabled ? 1.f : 0.f
        },
        .subUVFrameCurveTimes = _desc.subUvFrameOverLife.frameCurveTimes,
        .subUVFrameCurveValues = _desc.subUvFrameOverLife.frameCurveValues,
        .subUVFrameCurveParams = Vec4{
            static_cast<float>(max(1u, min(4u, _desc.subUvFrameOverLife.frameCurveKeyCount))),
            0.f,
            0.f,
            0.f
        },
        .sizeByLifeParams = Vec4{
            _desc.sizeByLife.multiplyXStart,
            _desc.sizeByLife.multiplyXEnd,
            _desc.sizeByLife.multiplyYStart,
            _desc.sizeByLife.multiplyYEnd
        },
        .motionFlags = Vec4{
            _desc.motion.enabled ? 1.f : 0.f,
            _desc.motion.initialVelocityEnabled ? 1.f : 0.f,
            _desc.motion.initialVelocityInWorldSpace ? 1.f : 0.f,
            _desc.motion.initialRadialVelocityEnabled ? 1.f : 0.f
        },
        .motionSpaceFlags = Vec4{
            _desc.motion.initialRadialVelocityInWorldSpace ? 1.f : 0.f,
            0.f,
            0.f,
            0.f
        },
        .initialVelocityMin = Vec4{
            _desc.motion.initialVelocityMin.x,
            _desc.motion.initialVelocityMin.y,
            _desc.motion.initialVelocityMin.z,
            0.f
        },
        .initialVelocityMax = Vec4{
            _desc.motion.initialVelocityMax.x,
            _desc.motion.initialVelocityMax.y,
            _desc.motion.initialVelocityMax.z,
            0.f
        },
        .motionRadialPivot = Vec4{
            _desc.motion.radialPivot.x,
            _desc.motion.radialPivot.y,
            _desc.motion.radialPivot.z,
            0.f
        },
        .motionRadialSpeedDrag = Vec4{
            _desc.motion.radialSpeed.x,
            _desc.motion.radialSpeed.y,
            _desc.motion.drag.x,
            _desc.motion.drag.y
        },
        .accelerationMin = Vec4{
            _desc.motion.accelerationMin.x,
            _desc.motion.accelerationMin.y,
            _desc.motion.accelerationMin.z,
            0.f
        },
        .accelerationMax = Vec4{
            _desc.motion.accelerationMax.x,
            _desc.motion.accelerationMax.y,
            _desc.motion.accelerationMax.z,
            0.f
        },
        .velocityScaleByLife = Vec4{ _desc.motion.velocityScaleByLife.x, _desc.motion.velocityScaleByLife.y, 0.f, 0.f },
        .rotationInitialRate = Vec4{
            _desc.rotation.initialRotationDegrees.x,
            _desc.rotation.initialRotationDegrees.y,
            _desc.rotation.initialRotationRateDegrees.x,
            _desc.rotation.initialRotationRateDegrees.y
        },
        .rotationByLife = Vec4{
            _desc.rotation.rotationOverLifeDegrees.x,
            _desc.rotation.rotationOverLifeDegrees.y,
            _desc.rotation.rotationRateScaleByLife.x,
            _desc.rotation.rotationRateScaleByLife.y
        }
    };

    _context->UpdateSubresource(_computeConstantBuffer.Get(), 0, nullptr, &params, 0, 0);
    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Reset_IndirectArgs()
{
    if (nullptr == _computeArgsOutput)
        return E_FAIL;

    const ComputeIndirectArgs args{
        .value0 = 1u,
        .instanceCount = 0u,
        .value2 = 0u,
        .value3 = 0,
        .startInstanceLocation = 0u
    };

    return _computeArgsOutput->Update_Data(&args, 1);
}

HRESULT VIBufferCom_ComputePointParticle::Copy_OutputToInstanceVB()
{
    if (nullptr == _computeOutput || nullptr == _instanceVB)
        return E_FAIL;

    _context->CopyResource(_instanceVB.Get(), _computeOutput->Get_Buffer());
    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Copy_OutputToIndirectArgs()
{
    if (PointParticleDrawMode::Direct == _desc.drawMode)
        return S_OK;

    if (nullptr == _computeArgsOutput || nullptr == _indirectArgsBuffer)
        return E_FAIL;

    _context->CopyResource(_indirectArgsBuffer.Get(), _computeArgsOutput->Get_Buffer());

    return S_OK;
}

VIBufferCom_ComputePointParticle::ParticleLifecycleState VIBufferCom_ComputePointParticle::Make_InitialLifecycleState(uint32 index) const
{
    const float ratio = (static_cast<float>(index) + 0.5f) / max(1.f, static_cast<float>(_instanceCount));
    const float lifeMin = max(0.0001f, min(_desc.lifeTime.x, _desc.lifeTime.y));
    const float lifeMax = max(lifeMin, max(_desc.lifeTime.x, _desc.lifeTime.y));
    const float sampledLifeMax = lerp(lifeMin, lifeMax, ratio);

    return ParticleLifecycleState{
        .lifeAge = 0.f,
        .lifeMax = sampledLifeMax,
        .respawnSeed = index * 9781u + 1u,
        .active = 0u
    };
}

Shared<VIBufferCom_ComputePointParticle> VIBufferCom_ComputePointParticle::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<VIBufferCom_ComputePointParticle>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : VIBufferCom_ComputePointParticle");
        return nullptr;
    }

    return instance;
}

Shared<Component> VIBufferCom_ComputePointParticle::Clone(void* arg)
{
    auto instance = make_shared<VIBufferCom_ComputePointParticle>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : VIBufferCom_ComputePointParticle");
        MSG_BOX("Failed to Clone : VIBufferCom_ComputePointParticle");
        return nullptr;
    }

    return instance;
}

void VIBufferCom_ComputePointParticle::Free()
{
    _indirectArgsBuffer.Reset();
    _computeConstantBuffer.Reset();
    _lifecycleState.reset();
    _computeArgsOutput.reset();
    _computeOutput.reset();
    _computeShader.reset();

    __super::Free();
}
