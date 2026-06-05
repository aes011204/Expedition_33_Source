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
    Vec4 emitterBasisRight{};          // xyz: emitter local X axis in world space.
    Vec4 emitterBasisUp{};             // xyz: emitter local Y axis in world space.
    Vec4 emitterBasisLook{};           // xyz: emitter local Z axis in world space.
    Vec4 spawnRange{};                 // xyz: spawn range, w: unused.
    Vec4 initialLocationMin{};         // xyz: InitialLocation min offset, w: enabled.
    Vec4 initialLocationMax{};         // xyz: InitialLocation max offset, w: unused.
    Vec4 sphereLocationOffsetRadius{}; // xyz: sphere center offset, w: radius.
    Vec4 sphereLocationParams{};       // x: enabled, y: mode, z/w: unused.
    Vec4 planeRadialLocationParams{};          // x: enabled, y: plane, z: shape, w: placement mode.
    Vec4 planeRadialLocationOffsetThickness{}; // xyz: offset, w: thickness.
    Vec4 planeRadialLocationRectRange{};       // x/y: U range, z/w: V range.
    Vec4 planeRadialLocationPolarRange{};      // x/y: radius range, z/w: angle degree range.
    DirectX::XMUINT4 planeRadialLocationSeedSalts{}; // x: U/radius, y: V/angle, z/w: unused.
    Vec4 planeRadialOrientationParams{};       // x: enabled, y: target kind, z: orientation mode, w: unused.
    Vec4 sizeMin{};                    // x/y: width/height min, z/w: unused.
    Vec4 sizeMax{};                    // x/y: width/height max, z/w: unused.
    Vec4 lifeTime{};                   // x/y: lifetime min/max, z/w: unused.
    Vec4 startColorMin{};
    Vec4 startColorMax{};
    Vec4 endColorMin{};
    Vec4 endColorMax{};
    Vec4 colorOverLifeCurveParams{};  // x: color key count, y: alpha key count, z: color enabled, w: alpha enabled.
    Vec4 colorOverLifeColorCurveTimes{};
    Vec4 colorOverLifeColorCurveValuesR{};
    Vec4 colorOverLifeColorCurveValuesG{};
    Vec4 colorOverLifeColorCurveValuesB{};
    Vec4 colorOverLifeColorCurveArriveR{};
    Vec4 colorOverLifeColorCurveArriveG{};
    Vec4 colorOverLifeColorCurveArriveB{};
    Vec4 colorOverLifeColorCurveLeaveR{};
    Vec4 colorOverLifeColorCurveLeaveG{};
    Vec4 colorOverLifeColorCurveLeaveB{};
    Vec4 colorOverLifeColorCurveModes{};
    Vec4 colorOverLifeAlphaCurveTimes{};
    Vec4 colorOverLifeAlphaCurveValues{};
    Vec4 colorOverLifeAlphaCurveArrive{};
    Vec4 colorOverLifeAlphaCurveLeave{};
    Vec4 colorOverLifeAlphaCurveModes{};
    Vec4 subUVParams{};           // x/y: rows/cols, z: playback mode, w: frames per second.
    Vec4 subUVFrameParams{};      // x/y: start/end frame, z: loop, w: enabled.
    Vec4 subUVFrameCurveTimes{};  // 후속 Curve Editor용 frame curve key times.
    Vec4 subUVFrameCurveValues{}; // 후속 Curve Editor용 frame curve frame values.
    Vec4 subUVFrameCurveParams{}; // x: key count, y/z/w: unused.
    DirectX::XMUINT4 spawnSeedSalts{}; // x: Lifetime, y: InitialLocation, z: SphereLocation, w: PlaneRadialLocation.
    DirectX::XMUINT4 subUVSeedSalts{}; // x: SubUV RandomFrame, y/z/w: unused.
    DirectX::XMUINT4 appearanceSeedSalts0{}; // x: InitialSize, y: InitialColor RGB, z: InitialColor Alpha, w: ColorOverLife RGB.
    DirectX::XMUINT4 appearanceSeedSalts1{}; // x: ColorOverLife Alpha, y/z/w: unused.
    Vec4 sizeByLifeParams{};      // x/y: width start/end, z/w: height start/end.
    Vec4 sizeByLifeCurveTimes{};  // SizeByLife scale curve key times.
    Vec4 sizeByLifeCurveValuesX{}; // SizeByLife X multiplier key values.
    Vec4 sizeByLifeCurveValuesY{}; // SizeByLife Y multiplier key values.
    Vec4 sizeByLifeCurveParams{}; // x: key count, y: multiplyX, z: multiplyY, w: axisLock.
    Vec4 motionFlags{};           // x: motion enabled, y: vector enabled, z: vector world, w: radial enabled.
    Vec4 motionSpaceFlags{};      // x: radial world, y: cone enabled, z: cone world, w: unused.
    Vec4 initialVelocityMin{};    // xyz: vector velocity min, w: unused.
    Vec4 initialVelocityMax{};    // xyz: vector velocity max, w: unused.
    Vec4 motionRadialPivot{};     // xyz: radial pivot, w: unused.
    Vec4 motionRadialSpeedDrag{}; // x/y: radial speed min/max, z/w: drag min/max.
    Vec4 velocityConeAxisAngle{}; // xyz: cone axis, w: half-angle degrees.
    Vec4 velocityConeSpeed{};     // x/y: cone speed min/max, z/w: unused.
    Vec4 accelerationMin{};       // xyz: acceleration min, w: unused.
    Vec4 accelerationMax{};       // xyz: acceleration max, w: unused.
    Vec4 accelerationCurveTimes{};   // Acceleration key times.
    Vec4 accelerationCurveValuesX{}; // Acceleration X key values.
    Vec4 accelerationCurveValuesY{}; // Acceleration Y key values.
    Vec4 accelerationCurveValuesZ{}; // Acceleration Z key values.
    Vec4 accelerationCurveParams{};  // x: key count, y: enabled, z/w: unused.
    Vec4 velocityScaleByLife{};   // x/y: scale start/end, z/w: unused.
    Vec4 velocityScaleByLifeCurveTimes{};  // VelocityOverLife key times.
    Vec4 velocityScaleByLifeCurveValues{}; // VelocityOverLife multiplier values.
    Vec4 velocityScaleByLifeCurveParams{}; // x: key count, y/z/w: unused.
    Vec4 orbitParams{};           // x: enabled, y: plane, z/w: unused.
    Vec4 orbitAngleRange{};       // x/y: angle start/end degrees, z/w: unused.
    Vec4 orbitAngleCurveTimes{};  // OrbitOverLife angle key times.
    Vec4 orbitAngleCurveValues{}; // OrbitOverLife angle degree key values.
    Vec4 orbitAngleCurveParams{}; // x: key count, y/z/w: unused.
    Vec4 orbitRadiusRange{};      // x/y: radius scale start/end, z/w: unused.
    Vec4 orbitRadiusCurveTimes{}; // OrbitOverLife radius scale key times.
    Vec4 orbitRadiusCurveValues{}; // OrbitOverLife radius scale key values.
    Vec4 orbitRadiusCurveParams{}; // x: key count, y/z/w: unused.
    DirectX::XMUINT4 motionSeedSalts0{}; // x: InitialVelocity, y: InitialRadialVelocity, z: VelocityCone, w: Acceleration.
    DirectX::XMUINT4 motionSeedSalts1{}; // x: Drag, y/z/w: unused.
    Vec4 rotationInitialRate{};   // x/y: initial rotation min/max, z/w: rate min/max.
    Vec4 rotationByLife{};        // x/y: legacy rotation start/end, z/w: rate scale start/end.
    Vec4 rotationOverLifeCurveTimes{};  // RotationOverLife key times.
    Vec4 rotationOverLifeCurveValues{}; // RotationOverLife degree key values.
    Vec4 rotationOverLifeCurveParams{}; // x: key count, y/z/w: unused.
    Vec4 rotationRateScaleByLifeCurveTimes{};  // RotationRateScaleByLife key times.
    Vec4 rotationRateScaleByLifeCurveValues{}; // RotationRateScaleByLife multiplier values.
    Vec4 rotationRateScaleByLifeCurveParams{}; // x: key count, y/z/w: unused.
    DirectX::XMUINT4 rotationSeedSalts{}; // x: InitialRotation, y: InitialRotationRate, z/w: unused.
};

static uint32 Resolve_SeedSalt(const PointParticleRandomSeedRuntimeDesc& seed)
{
    return seed.manualSeedEnabled ? seed.seed : 0u;
}

static float Hash01(uint32 seed)
{
    seed ^= 2747636419u;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    return static_cast<float>(seed & 0x00FFFFFFu) / 16777215.f;
}

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

    CHECK_FAILED(Ready_ResetBaseline(), E_FAIL);
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

    _hasActiveParticles = _hasActiveParticles || spawnRequest > 0u;
    if (killActiveParticles)
        _hasActiveParticles = false;

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

void VIBufferCom_ComputePointParticle::Update_RuntimeDesc(const ComputePointParticleDesc& desc)
{
    if (desc.instanceCount != _instanceCount)
        return;

    _desc = desc;
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

HRESULT VIBufferCom_ComputePointParticle::Reset_ForEffectReplay()
{
    if (nullptr == _computeOutput || nullptr == _lifecycleState || nullptr == _instanceVB)
        return E_FAIL;
    if (_initialLifecycleState.size() != _instanceCount || _emptyInstancePayload.size() != _instanceCount)
        return E_FAIL;

    _elapsedTime = 0.f;
    _deltaTime = 0.f;
    _spawnRequest = 0u;
    _killActiveParticles = 0u;
    _hasActiveParticles = false;

    CHECK_FAILED(_lifecycleState->Update_Data(_initialLifecycleState.data(), _instanceCount), E_FAIL);
    CHECK_FAILED(_computeOutput->Update_Data(_emptyInstancePayload.data(), _instanceCount), E_FAIL);
    CHECK_FAILED(Update_ConstantBuffer(), E_FAIL);
    CHECK_FAILED(Copy_OutputToInstanceVB(), E_FAIL);
    CHECK_FAILED(Reset_IndirectArgs(), E_FAIL);
    CHECK_FAILED(Copy_OutputToIndirectArgs(), E_FAIL);

    return S_OK;
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
        _instanceCount,
        _emptyInstancePayload.data()
    );
    CHECK_NULL(_computeOutput, E_FAIL);

    return S_OK;
}

HRESULT VIBufferCom_ComputePointParticle::Ready_LifecycleState()
{
    _lifecycleState = ComputeStructuredBuffer::Create(
        _device,
        _context,
        sizeof(ParticleLifecycleState),
        _instanceCount,
        _initialLifecycleState.data()
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
            static_cast<float>(ETOI(_desc.sphereLocation.placementMode)),
            0.f
        },
        .planeRadialLocationParams = Vec4{
            _desc.planeRadialLocation.enabled ? 1.f : 0.f,
            static_cast<float>(ETOI(_desc.planeRadialLocation.plane)),
            static_cast<float>(ETOI(_desc.planeRadialLocation.shape)),
            static_cast<float>(ETOI(_desc.planeRadialLocation.placementMode))
        },
        .planeRadialLocationOffsetThickness = Vec4{
            _desc.planeRadialLocation.offset.x,
            _desc.planeRadialLocation.offset.y,
            _desc.planeRadialLocation.offset.z,
            max(0.f, _desc.planeRadialLocation.thickness)
        },
        .planeRadialLocationRectRange = Vec4{
            _desc.planeRadialLocation.uRange.x,
            _desc.planeRadialLocation.uRange.y,
            _desc.planeRadialLocation.vRange.x,
            _desc.planeRadialLocation.vRange.y
        },
        .planeRadialLocationPolarRange = Vec4{
            _desc.planeRadialLocation.radiusRange.x,
            _desc.planeRadialLocation.radiusRange.y,
            _desc.planeRadialLocation.angleDegreesRange.x,
            _desc.planeRadialLocation.angleDegreesRange.y
        },
        .planeRadialLocationSeedSalts = DirectX::XMUINT4{
            _desc.planeRadialLocation.shape == PointParticlePlaneRadialLocationShape::Rectangle
            ? Resolve_SeedSalt(_desc.planeRadialLocation.uSeed)
            : Resolve_SeedSalt(_desc.planeRadialLocation.radiusSeed),
            _desc.planeRadialLocation.shape == PointParticlePlaneRadialLocationShape::Rectangle
            ? Resolve_SeedSalt(_desc.planeRadialLocation.vSeed)
            : Resolve_SeedSalt(_desc.planeRadialLocation.angleDegreesSeed),
            0u,
            0u
        },
        .planeRadialOrientationParams = Vec4{
            _desc.planeRadialOrientation.enabled ? 1.f : 0.f,
            static_cast<float>(ETOI(_desc.planeRadialOrientation.targetKind)),
            static_cast<float>(ETOI(_desc.planeRadialOrientation.orientationMode)),
            0.f
        },
        .sizeMin = Vec4{ _desc.sizeMin.x, _desc.sizeMin.y, 0.f, 0.f },
        .sizeMax = Vec4{ _desc.sizeMax.x, _desc.sizeMax.y, 0.f, 0.f },
        .lifeTime = Vec4{ _desc.lifeTime.x, _desc.lifeTime.y, 0.f, 0.f },
        .startColorMin = _desc.startColorMin,
        .startColorMax = _desc.startColorMax,
        .endColorMin = _desc.endColorMin,
        .endColorMax = _desc.endColorMax,
        .colorOverLifeCurveParams = Vec4{
            static_cast<float>(max(1u, min(4u, _desc.colorOverLifeCurve.colorCurveKeyCount))),
            static_cast<float>(max(1u, min(4u, _desc.colorOverLifeCurve.alphaCurveKeyCount))),
            _desc.colorOverLifeCurve.colorCurveEnabled ? 1.f : 0.f,
            _desc.colorOverLifeCurve.alphaCurveEnabled ? 1.f : 0.f
        },
        .colorOverLifeColorCurveTimes = _desc.colorOverLifeCurve.colorCurveTimes,
        .colorOverLifeColorCurveValuesR = _desc.colorOverLifeCurve.colorCurveValuesR,
        .colorOverLifeColorCurveValuesG = _desc.colorOverLifeCurve.colorCurveValuesG,
        .colorOverLifeColorCurveValuesB = _desc.colorOverLifeCurve.colorCurveValuesB,
        .colorOverLifeColorCurveArriveR = _desc.colorOverLifeCurve.colorCurveArriveR,
        .colorOverLifeColorCurveArriveG = _desc.colorOverLifeCurve.colorCurveArriveG,
        .colorOverLifeColorCurveArriveB = _desc.colorOverLifeCurve.colorCurveArriveB,
        .colorOverLifeColorCurveLeaveR = _desc.colorOverLifeCurve.colorCurveLeaveR,
        .colorOverLifeColorCurveLeaveG = _desc.colorOverLifeCurve.colorCurveLeaveG,
        .colorOverLifeColorCurveLeaveB = _desc.colorOverLifeCurve.colorCurveLeaveB,
        .colorOverLifeColorCurveModes = _desc.colorOverLifeCurve.colorCurveModes,
        .colorOverLifeAlphaCurveTimes = _desc.colorOverLifeCurve.alphaCurveTimes,
        .colorOverLifeAlphaCurveValues = _desc.colorOverLifeCurve.alphaCurveValues,
        .colorOverLifeAlphaCurveArrive = _desc.colorOverLifeCurve.alphaCurveArrive,
        .colorOverLifeAlphaCurveLeave = _desc.colorOverLifeCurve.alphaCurveLeave,
        .colorOverLifeAlphaCurveModes = _desc.colorOverLifeCurve.alphaCurveModes,
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
        .spawnSeedSalts = DirectX::XMUINT4{
            Resolve_SeedSalt(_desc.lifetimeSeed),
            Resolve_SeedSalt(_desc.initialLocationSeed),
            Resolve_SeedSalt(_desc.sphereLocationSeed),
            0u
        },
        .subUVSeedSalts = DirectX::XMUINT4{
            Resolve_SeedSalt(_desc.subUVRandomFrameSeed),
            0u,
            0u,
            0u
        },
        .appearanceSeedSalts0 = DirectX::XMUINT4{
            Resolve_SeedSalt(_desc.initialSizeSeed),
            Resolve_SeedSalt(_desc.initialColorSeed),
            Resolve_SeedSalt(_desc.initialAlphaSeed),
            Resolve_SeedSalt(_desc.colorOverLifeSeed)
        },
        .appearanceSeedSalts1 = DirectX::XMUINT4{
            Resolve_SeedSalt(_desc.alphaOverLifeSeed),
            0u,
            0u,
            0u
        },
        .sizeByLifeParams = Vec4{
            _desc.sizeByLife.multiplyXStart,
            _desc.sizeByLife.multiplyXEnd,
            _desc.sizeByLife.multiplyYStart,
            _desc.sizeByLife.multiplyYEnd
        },
        .sizeByLifeCurveTimes = _desc.sizeByLife.curveKeyTimes,
        .sizeByLifeCurveValuesX = _desc.sizeByLife.curveKeyValuesX,
        .sizeByLifeCurveValuesY = _desc.sizeByLife.curveKeyValuesY,
        .sizeByLifeCurveParams = Vec4{
            static_cast<float>(max(1u, min(4u, _desc.sizeByLife.curveKeyCount))),
            _desc.sizeByLife.multiplyX ? 1.f : 0.f,
            _desc.sizeByLife.multiplyY ? 1.f : 0.f,
            static_cast<float>(ETOI(_desc.sizeByLife.axisLock))
        },
        .motionFlags = Vec4{
            _desc.motion.enabled ? 1.f : 0.f,
            _desc.motion.initialVelocityEnabled ? 1.f : 0.f,
            _desc.motion.initialVelocityInWorldSpace ? 1.f : 0.f,
            _desc.motion.initialRadialVelocityEnabled ? 1.f : 0.f
        },
        .motionSpaceFlags = Vec4{
            _desc.motion.initialRadialVelocityInWorldSpace ? 1.f : 0.f,
            _desc.motion.velocityConeEnabled ? 1.f : 0.f,
            _desc.motion.velocityConeInWorldSpace ? 1.f : 0.f,
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
        .velocityConeAxisAngle = Vec4{
            _desc.motion.velocityConeAxis.x,
            _desc.motion.velocityConeAxis.y,
            _desc.motion.velocityConeAxis.z,
            clamp(_desc.motion.velocityConeAngleDegrees, 0.f, 180.f)
        },
        .velocityConeSpeed = Vec4{
            _desc.motion.velocityConeSpeed.x,
            _desc.motion.velocityConeSpeed.y,
            0.f,
            0.f
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
        .accelerationCurveTimes = _desc.motion.accelerationCurveTimes,
        .accelerationCurveValuesX = _desc.motion.accelerationCurveValuesX,
        .accelerationCurveValuesY = _desc.motion.accelerationCurveValuesY,
        .accelerationCurveValuesZ = _desc.motion.accelerationCurveValuesZ,
        .accelerationCurveParams = Vec4{
            static_cast<float>(_desc.motion.accelerationCurveKeyCount),
            _desc.motion.accelerationCurveEnabled ? 1.f : 0.f,
            0.f,
            0.f
        },
        .velocityScaleByLife = Vec4{ _desc.motion.velocityScaleByLife.x, _desc.motion.velocityScaleByLife.y, 0.f, 0.f },
        .velocityScaleByLifeCurveTimes = _desc.motion.velocityScaleByLifeCurveTimes,
        .velocityScaleByLifeCurveValues = _desc.motion.velocityScaleByLifeCurveValues,
        .velocityScaleByLifeCurveParams = Vec4{
            static_cast<float>(_desc.motion.velocityScaleByLifeCurveKeyCount),
            0.f,
            0.f,
            0.f
        },
        .orbitParams = Vec4{
            _desc.orbitOverLife.enabled ? 1.f : 0.f,
            static_cast<float>(ETOI(_desc.orbitOverLife.plane)),
            0.f,
            0.f
        },
        .orbitAngleRange = Vec4{ _desc.orbitOverLife.angleDegreesOverLife.x, _desc.orbitOverLife.angleDegreesOverLife.y, 0.f, 0.f },
        .orbitAngleCurveTimes = _desc.orbitOverLife.angleCurveTimes,
        .orbitAngleCurveValues = _desc.orbitOverLife.angleCurveValues,
        .orbitAngleCurveParams = Vec4{
            static_cast<float>(_desc.orbitOverLife.angleCurveKeyCount),
            0.f,
            0.f,
            0.f
        },
        .orbitRadiusRange = Vec4{ _desc.orbitOverLife.radiusScaleOverLife.x, _desc.orbitOverLife.radiusScaleOverLife.y, 0.f, 0.f },
        .orbitRadiusCurveTimes = _desc.orbitOverLife.radiusScaleCurveTimes,
        .orbitRadiusCurveValues = _desc.orbitOverLife.radiusScaleCurveValues,
        .orbitRadiusCurveParams = Vec4{
            static_cast<float>(_desc.orbitOverLife.radiusScaleCurveKeyCount),
            0.f,
            0.f,
            0.f
        },
        .motionSeedSalts0 = DirectX::XMUINT4{
            Resolve_SeedSalt(_desc.motion.initialVelocitySeed),
            Resolve_SeedSalt(_desc.motion.initialRadialVelocitySeed),
            Resolve_SeedSalt(_desc.motion.velocityConeSeed),
            Resolve_SeedSalt(_desc.motion.accelerationSeed)
        },
        .motionSeedSalts1 = DirectX::XMUINT4{
            Resolve_SeedSalt(_desc.motion.dragSeed),
            0u,
            0u,
            0u
        },
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
        },
        .rotationOverLifeCurveTimes = _desc.rotation.rotationOverLifeCurveTimes,
        .rotationOverLifeCurveValues = _desc.rotation.rotationOverLifeCurveValues,
        .rotationOverLifeCurveParams = Vec4{
            static_cast<float>(_desc.rotation.rotationOverLifeCurveKeyCount),
            0.f,
            0.f,
            0.f
        },
        .rotationRateScaleByLifeCurveTimes = _desc.rotation.rotationRateScaleByLifeCurveTimes,
        .rotationRateScaleByLifeCurveValues = _desc.rotation.rotationRateScaleByLifeCurveValues,
        .rotationRateScaleByLifeCurveParams = Vec4{
            static_cast<float>(_desc.rotation.rotationRateScaleByLifeCurveKeyCount),
            0.f,
            0.f,
            0.f
        },
        .rotationSeedSalts = DirectX::XMUINT4{
            Resolve_SeedSalt(_desc.rotation.initialRotationSeed),
            Resolve_SeedSalt(_desc.rotation.initialRotationRateSeed),
            0u,
            0u
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

HRESULT VIBufferCom_ComputePointParticle::Ready_ResetBaseline()
{
    _initialLifecycleState.clear();
    _emptyInstancePayload.clear();

    _initialLifecycleState.reserve(_instanceCount);
    for (uint32 index = 0; index < _instanceCount; ++index)
        _initialLifecycleState.push_back(Make_InitialLifecycleState(index));

    _emptyInstancePayload.resize(_instanceCount);
    return S_OK;
}

VIBufferCom_ComputePointParticle::ParticleLifecycleState VIBufferCom_ComputePointParticle::Make_InitialLifecycleState(uint32 index) const
{
    const float ratio = (static_cast<float>(index) + 0.5f) / max(1.f, static_cast<float>(_instanceCount));
    const float lifeMin = max(0.0001f, min(_desc.lifeTime.x, _desc.lifeTime.y));
    const float lifeMax = max(lifeMin, max(_desc.lifeTime.x, _desc.lifeTime.y));
    const uint32 respawnSeed = index * 9781u + 1u;
    const float lifeSample = _desc.lifetimeSeed.manualSeedEnabled
                             ? Hash01(respawnSeed + _desc.lifetimeSeed.seed + 43u)
                             : ratio;
    const float sampledLifeMax = lerp(lifeMin, lifeMax, lifeSample);

    return ParticleLifecycleState{
        .lifeAge = 0.f,
        .lifeMax = sampledLifeMax,
        .respawnSeed = respawnSeed,
        .active = 0u,
        .placementIndex = 0u,
        .placementCount = 1u,
        .reserved0 = 0u,
        .reserved1 = 0u
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
    _emptyInstancePayload.clear();
    _initialLifecycleState.clear();

    __super::Free();
}
