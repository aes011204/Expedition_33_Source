#include "pch.h"
#include "ColliderCom.h"

#include "GameInstance.h"
#include "Bounding.h"
#include "Bounding_AABB.h"
#include "Bounding_OBB.h"
#include "Bounding_Sphere.h"

ColliderCom::ColliderCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component(device, context)
{
}

ColliderCom::ColliderCom(const ColliderCom& Prototype)
    : Component(Prototype)
    , _shape(Prototype._shape)
    , _channel(Prototype._channel)
    , _overlapMask(Prototype._overlapMask)
    , _blockMask(Prototype._blockMask)
    , _preset(Prototype._preset)
    , _isActive(Prototype._isActive)
    , _isColl(false)
{
}

HRESULT ColliderCom::Initialize_Prototype()
{
    CHECK_FAILED(Component::Initialize_Prototype(), E_FAIL);

#ifdef _DEBUG
    CHECK_FAILED(Ready_DebugRenderResources(), E_FAIL);
#endif

    return S_OK;
}

HRESULT ColliderCom::Initialize_Prototype(CollisionShape shape)
{
    CHECK_FAILED(Initialize_Prototype(), E_FAIL);

    _shape = shape;

    return S_OK;
}

HRESULT ColliderCom::Initialize(void* arg)
{
    CHECK_FAILED(Component::Initialize(arg), E_FAIL);

#ifdef _DEBUG
    if (nullptr == _batch || nullptr == _effect || nullptr == _inputLayout)
        CHECK_FAILED(Ready_DebugRenderResources(), E_FAIL);
#endif

    if (nullptr == arg)
        return E_FAIL;

    switch (_shape)
    {
    case CollisionShape::AABB:
        _bounding = Bounding_AABB::Create(
            _device,
            _context,
            *static_cast<Bounding_AABB::BoundingAABBDesc*>(arg));
        break;

    case CollisionShape::OBB:
        _bounding = Bounding_OBB::Create(
            _device,
            _context,
            *static_cast<Bounding_OBB::BoundingOBBDesc*>(arg));
        break;

    case CollisionShape::Sphere:
        _bounding = Bounding_Sphere::Create(
            _device,
            _context,
            *static_cast<Bounding_Sphere::FBoundingSphereDesc*>(arg));
        break;

    default:
        return E_FAIL;
    }

    if (nullptr == _bounding)
        return E_FAIL;

    _overlapSet.clear();
    _isColl = false;

    return S_OK;
}

void ColliderCom::Update_Collider(const Matrix& worldMatrix)
{
    if (nullptr == _bounding)
        return;

    _bounding->Update(worldMatrix);
}

bool ColliderCom::Intersect(Shared<ColliderCom> target)
{
    if (nullptr == _bounding || nullptr == target)
        return false;

    Shared<Bounding> targetBounding = target->Get_Bounding();
    if (nullptr == targetBounding)
        return false;

    return _bounding->Intersect(targetBounding.get());
}

void ColliderCom::Add_Overlap(Shared<ColliderCom> other)
{
    if (nullptr == other)
        return;

    _overlapSet.emplace(other);
}

void ColliderCom::Remove_Overlap(Shared<ColliderCom> other)
{
    if (nullptr == other)
        return;

    auto iter = _overlapSet.begin();
    while (iter != _overlapSet.end())
    {
        Shared<ColliderCom> overlap = iter->lock();

        if (nullptr == overlap || overlap == other)
            iter = _overlapSet.erase(iter);
        else
            ++iter;
    }
}

bool ColliderCom::Is_Overlapping(Shared<ColliderCom> other) const
{
    if (nullptr == other)
        return false;

    for (const auto& weakCollider : _overlapSet)
    {
        Shared<ColliderCom> overlap = weakCollider.lock();

        if (overlap == other)
            return true;
    }

    return false;
}

void ColliderCom::Clear_Overlap()
{
    _overlapSet.clear();
}

void ColliderCom::Set_CollisionPreset(Collision_Preset preset)
{
    _preset = preset;

    const Collision_Preset_Data& data = Get_PresetData(preset);

    _channel = data.channel;
    _overlapMask = data.overlapMask;
    _blockMask = data.blockMask;
}

#ifdef _DEBUG
HRESULT ColliderCom::Ready_DebugRenderResources()
{
    static constexpr D3D11_INPUT_ELEMENT_DESC kDebugInputElements[] =
    {
        { "SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",       0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    _batch = make_shared<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>>(_context.Get());
    _effect = make_shared<DirectX::BasicEffect>(_device.Get());

    if (nullptr == _batch || nullptr == _effect)
        return E_FAIL;

    _effect->SetVertexColorEnabled(true);

    const void* shaderByteCode = nullptr;
    size_t byteCodeLength = 0;

    _effect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

    if (FAILED(_device->CreateInputLayout(
        kDebugInputElements,
        static_cast<UINT>(_countof(kDebugInputElements)),
        shaderByteCode,
        byteCodeLength,
        &_inputLayout)))
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT ColliderCom::Render_Debug()
{
    if (nullptr == _bounding || nullptr == _batch || nullptr == _effect || nullptr == _inputLayout)
        return S_OK;

    // 이펙트/파티클 렌더가 남긴 파이프라인 상태 정리하기
    // GS + AlphaBlend + SRV 때문에 문제생기네
    {
        ID3D11GeometryShader* nullGeometryShader = nullptr;
        _context->GSSetShader(nullGeometryShader, nullptr, 0);

        ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
        _context->PSSetShaderResources(0, 1, nullSRV);

        _context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
        _context->OMSetDepthStencilState(nullptr, 0);
        _context->RSSetState(nullptr);
    }

    _effect->SetWorld(Matrix::Identity);
    _effect->SetView(*GAME->Get_Transform(D3DTS::View));
    _effect->SetProjection(*GAME->Get_Transform(D3DTS::Proj));

    _context->IASetInputLayout(_inputLayout.Get());
    _effect->Apply(_context.Get());

    _batch->Begin();

    Color color = Color(0.f, 1.f, 0.f, 1.f);

    if (_isActive == false)
        color = Color(0.5f, 0.5f, 0.5f, 1.f);
    else if (_isColl)
        color = Color(1.f, 0.f, 0.f, 1.f);

    _bounding->Render_Debug(_batch.get(), color);

    _batch->End();

    return S_OK;
}
#endif

Shared<ColliderCom> ColliderCom::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context,
    CollisionShape shape)
{
    auto instance = make_shared<ColliderCom>(device, context);

    if (FAILED(instance->Initialize_Prototype(shape)))
    {
        LOG_WARN("Failed to create ColliderCom: shape={}", static_cast<int>(shape));
        return nullptr;
    }

    return instance;
}

Shared<Component> ColliderCom::Clone(void* arg)
{
    auto instance = make_shared<ColliderCom>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Clone : ColliderCom");
        return nullptr;
    }

    return instance;
}

void ColliderCom::Free()
{
    Component::Free();

    _bounding = nullptr;
    _overlapSet.clear();

#ifdef _DEBUG
    _batch.reset();
    _effect.reset();
    _inputLayout.Reset();
#endif
}
