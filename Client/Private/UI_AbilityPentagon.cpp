#include "UI_AbilityPentagon.h"

#include "GameInstance.h"
#include "ResourceLoader.h"
#include "ShaderCom.h"

#include <cmath>
#include <filesystem>

namespace
{
    constexpr float kHalfExtent = 0.5f;
    constexpr float kOuterInsetRatio = 0.92f;
    constexpr float kAxisRotationOffsetRadians = 0.f; // 같은 값이면 위 꼭짓점 기준의 정확한 정오각형이 되게 축 보정을 끈다.
    constexpr std::array<Vec2, 5> kAbilityAxisDirections =
    {
        Vec2(0.f, 1.f),                 // 활력 : 위
        Vec2(0.9510565f, 0.3090169f),   // 무력 : 오른쪽 위
        Vec2(0.5877852f, -0.8090169f),  // 민첩 : 오른쪽 아래
        Vec2(-0.5877852f, -0.8090169f), // 방어 : 왼쪽 아래
        Vec2(-0.9510565f, 0.3090169f),  // 행운 : 정규 오각형의 왼쪽 위 축으로 되돌린다.
    };
}

UI_AbilityPentagon::UI_AbilityPentagon(const UI_AbilityPentagon& prototype)
    : Engine::UI_Base(prototype)
    , _texturePath(prototype._texturePath)
    , _color(prototype._color)
    , _shaderPrototypeLevelIndex(prototype._shaderPrototypeLevelIndex)
    , _shaderPrototypeTag(prototype._shaderPrototypeTag)
    , _maxStatValue(prototype._maxStatValue)
    , _baseInnerRatio(prototype._baseInnerRatio)
    , _normalizedRatios(prototype._normalizedRatios)
    , _vertices(prototype._vertices)
    , _indices(prototype._indices)
{
}

HRESULT UI_AbilityPentagon::Initialize_Prototype()
{
    return __super::Initialize_Prototype();
}

HRESULT UI_AbilityPentagon::Initialize(void* arg)
{
    if (FAILED(__super::Initialize(arg)))
    {
        LOG_ERROR("UI_AbilityPentagon::Initialize - UI_Base initialize failed");
        return E_FAIL;
    }

    if (arg != nullptr)
    {
        const UI_ABILITY_PENTAGON_DESC* desc = static_cast<const UI_ABILITY_PENTAGON_DESC*>(arg);

        _texturePath = desc->texturePath;
        _color = desc->color;
        _shaderPrototypeLevelIndex = desc->shaderPrototypeLevelIndex;
        _shaderPrototypeTag = desc->shaderPrototypeTag;
        _maxStatValue = desc->maxStatValue;
        _baseInnerRatio = ::clamp(desc->baseInnerRatio, 0.f, 1.f);
    }

    if (FAILED(Load_TextureResource()))
    {
        LOG_ERROR("UI_AbilityPentagon::Initialize - texture load failed. path='{}'", String::ToString(_texturePath));
        return E_FAIL;
    }

    Rebuild_Geometry();

    if (FAILED(Create_GeometryBuffers()))
    {
        LOG_ERROR("UI_AbilityPentagon::Initialize - geometry buffer create failed");
        return E_FAIL;
    }

    if (FAILED(Ready_RenderResources()))
    {
        // UI_Image 와 동일하게 생성 시점 shader clone 실패는 경고만 남기고,
        // 실제 Render 시점에 한번 더 준비를 시도한다.
        LOG_WARN("UI_AbilityPentagon: render resources are not ready. Retrying during Render.");
    }

    return S_OK;
}

void UI_AbilityPentagon::Update(float timeDelta)
{
    __super::Update(timeDelta);
}

HRESULT UI_AbilityPentagon::Render()
{
    if (!Is_EffectivelyOpen())
        return S_OK;

    if (FAILED(__super::Render()))
        return E_FAIL;

    if (_transform == nullptr)
        return E_FAIL;

    if (_srv == nullptr)
        return S_OK;

    if (_shader == nullptr)
    {
        if (FAILED(Ready_RenderResources()))
            return S_OK;
    }

    if (_shader == nullptr || _vertexBuffer == nullptr || _indexBuffer == nullptr)
        return S_OK;

    const Vec2 size = Get_ResolvedLocalSize();
    const Vec2& localScale = _transform->Get_Scale();
    const Vec2 worldScale = Get_WorldScale();

    const float width = size.x * worldScale.x;
    const float height = size.y * worldScale.y;

    if (width <= 0.f || height <= 0.f)
        return S_OK;

    const float viewportWidth = static_cast<float>(max(1u, GAME->Get_ViewportWidth()));
    const float viewportHeight = static_cast<float>(max(1u, GAME->Get_ViewportHeight()));
    const Vec2 center = Transform_LocalPointToWorld(Get_ScaledLocalCenter(localScale));

    Matrix worldMatrix =
        Matrix::CreateScale(width, height, 1.f) *
        Matrix::CreateRotationZ(-Get_WorldRotationRadians()) *
        Matrix::CreateTranslation(
            center.x - viewportWidth * 0.5f,
            -center.y + viewportHeight * 0.5f,
            0.f);

    Matrix viewMatrix = Matrix::Identity;
    Matrix projMatrix;
    XMStoreFloat4x4(&projMatrix, XMMatrixOrthographicLH(viewportWidth, viewportHeight, 0.f, 1.f));

    if (FAILED(_shader->Bind_Matrix("g_WorldMatrix", &worldMatrix)))
        return E_FAIL;

    if (FAILED(_shader->Bind_Matrix("g_ViewMatrix", &viewMatrix)))
        return E_FAIL;

    if (FAILED(_shader->Bind_Matrix("g_ProjMatrix", &projMatrix)))
        return E_FAIL;

    const Vec4 renderColor = Get_EffectiveRenderColor(_color);
    if (FAILED(_shader->Bind_RawValue("g_Color", &renderColor, sizeof(Vec4))))
        return E_FAIL;

    if (FAILED(_shader->Bind_SRV("g_Texture", _srv.Get())))
        return E_FAIL;

    if (FAILED(_shader->Begin(0u)))
        return E_FAIL;

    const bool isBackgroundUI = Get_RenderGroup() == Engine::UI_RenderGroup::Background;
    if (isBackgroundUI)
        CHECK_FAILED(GAME->Bind_BackgroundUIBehindPlayerStencilState(), E_FAIL);

    ComPtr<Context> context = GAME->Get_Context();
    CHECK_NULL(context, E_FAIL);

    UINT stride = sizeof(Engine::VTXTEX);
    UINT offset = 0;
    ID3D11Buffer* vertexBuffer = _vertexBuffer.Get();
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    context->IASetIndexBuffer(_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->DrawIndexed(static_cast<UINT>(_indices.size()), 0, 0);

    if (isBackgroundUI)
        CHECK_FAILED(GAME->Bind_UINoDepthStencilState(), E_FAIL);

    return S_OK;
}

void UI_AbilityPentagon::Set_StatValues(const array<int32, 5>& statValues)
{
    array<float, 5> ratios{};

    for (size_t i = 0; i < statValues.size(); ++i)
    {
        const float linearRatio = (_maxStatValue > 0.f)
            ? ::clamp(static_cast<float>(statValues[i]) / _maxStatValue, 0.f, 1.f)
            : 0.f;
        ratios[i] = sqrtf(linearRatio); // 낮은 스탯끼리 한 점처럼 뭉쳐 보이지 않게 표시 비율만 벌린다.
    }

    Set_NormalizedRatios(ratios);
}

void UI_AbilityPentagon::Set_NormalizedRatios(const array<float, 5>& ratios)
{
    for (size_t i = 0; i < ratios.size(); ++i)
        _normalizedRatios[i] = ::clamp(ratios[i], 0.f, 1.f);

    Rebuild_Geometry();

    if (_vertexBuffer == nullptr)
        return;

    ComPtr<Context> context = GAME->Get_Context();
    if (context == nullptr)
        return;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(context->Map(_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, _vertices.data(), sizeof(Engine::VTXTEX) * _vertices.size());
        context->Unmap(_vertexBuffer.Get(), 0);
    }
}

HRESULT UI_AbilityPentagon::Ready_RenderResources()
{
    if (_shader == nullptr)
    {
        _shader = dynamic_pointer_cast<Engine::ShaderCom>(
            GAME->Clone_Prototype(
                Prototype::Comopnent,
                _shaderPrototypeLevelIndex,
                _shaderPrototypeTag));
    }

    if (_shader == nullptr)
    {
        LOG_ERROR(
            "UI_AbilityPentagon::Ready_RenderResources - shader clone failed. level={}, tag='{}'",
            _shaderPrototypeLevelIndex,
            String::ToString(_shaderPrototypeTag));
        return E_FAIL;
    }

    return S_OK;
}

HRESULT UI_AbilityPentagon::Load_TextureResource()
{
    _srv.Reset();

    if (_texturePath.empty())
        return S_OK;

    ComPtr<Device> device = GAME->Get_Device();
    CHECK_NULL(device, E_FAIL);

    if (SUCCEEDED(CreateWICTextureFromFile(device.Get(), _texturePath.c_str(), nullptr, _srv.GetAddressOf())))
        return S_OK;

    const wstring resolvedPath = ResourceLoader::Resolve_RuntimePath(_texturePath);
    if (!resolvedPath.empty() &&
        SUCCEEDED(CreateWICTextureFromFile(device.Get(), resolvedPath.c_str(), nullptr, _srv.GetAddressOf())))
    {
        _texturePath = resolvedPath; // 이후 재시도/로그도 실제 성공 경로 기준으로 맞춘다.
        return S_OK;
    }

    const std::filesystem::path cwdFallbackPath =
        std::filesystem::absolute(std::filesystem::path(_texturePath)).lexically_normal();
    if (SUCCEEDED(CreateWICTextureFromFile(device.Get(), cwdFallbackPath.c_str(), nullptr, _srv.GetAddressOf())))
    {
        _texturePath = cwdFallbackPath.wstring();
        return S_OK;
    }

    return E_FAIL;
}

HRESULT UI_AbilityPentagon::Create_GeometryBuffers()
{
    ComPtr<Device> device = GAME->Get_Device();
    CHECK_NULL(device, E_FAIL);

    _vertexBuffer.Reset();
    _indexBuffer.Reset();

    D3D11_BUFFER_DESC vertexDesc{};
    vertexDesc.ByteWidth = static_cast<UINT>(sizeof(Engine::VTXTEX) * _vertices.size());
    vertexDesc.Usage = D3D11_USAGE_DYNAMIC;
    vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA vertexData{};
    vertexData.pSysMem = _vertices.data();

    if (FAILED(device->CreateBuffer(&vertexDesc, &vertexData, _vertexBuffer.GetAddressOf())))
    {
        LOG_ERROR("UI_AbilityPentagon::Create_GeometryBuffers - vertex buffer create failed");
        return E_FAIL;
    }

    D3D11_BUFFER_DESC indexDesc{};
    indexDesc.ByteWidth = static_cast<UINT>(sizeof(uint16) * _indices.size());
    indexDesc.Usage = D3D11_USAGE_IMMUTABLE;
    indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexData{};
    indexData.pSysMem = _indices.data();

    if (FAILED(device->CreateBuffer(&indexDesc, &indexData, _indexBuffer.GetAddressOf())))
    {
        LOG_ERROR("UI_AbilityPentagon::Create_GeometryBuffers - index buffer create failed");
        return E_FAIL;
    }

    return S_OK;
}

void UI_AbilityPentagon::Rebuild_Geometry()
{
    _vertices[0].position = Vec3(0.f, 0.f, 0.f);
    _vertices[0].texcoord = Vec2(0.5f, 0.5f);

    for (size_t i = 0; i < 5; ++i)
    {
        const float finalRatio = (_baseInnerRatio + (_normalizedRatios[i] * (1.f - _baseInnerRatio))) * kOuterInsetRatio;
        const Vec2 baseAxisDirection = kAbilityAxisDirections[i];
        const float cosAngle = cosf(kAxisRotationOffsetRadians);
        const float sinAngle = sinf(kAxisRotationOffsetRadians);
        const Vec2 rotatedAxisDirection(
            baseAxisDirection.x * cosAngle - baseAxisDirection.y * sinAngle,
            baseAxisDirection.x * sinAngle + baseAxisDirection.y * cosAngle);
        const Vec2 axisDirection = rotatedAxisDirection * kHalfExtent * finalRatio;
        const float x = axisDirection.x;
        const float y = axisDirection.y;

        _vertices[i + 1].position = Vec3(x, y, 0.f);
        _vertices[i + 1].texcoord = Vec2(
            ::clamp(x + 0.5f, 0.f, 1.f),
            ::clamp(0.5f - y, 0.f, 1.f));
    }
}

Shared<UI_AbilityPentagon> UI_AbilityPentagon::Create()
{
    auto instance = make_shared<UI_AbilityPentagon>();

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : UI_AbilityPentagon");
        return nullptr;
    }

    return instance;
}

Shared<Engine::UI_Base> UI_AbilityPentagon::Clone(void* arg)
{
    auto instance = make_shared<UI_AbilityPentagon>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : UI_AbilityPentagon");
        return nullptr;
    }

    return instance;
}

void UI_AbilityPentagon::Free()
{
    _indexBuffer.Reset();
    _vertexBuffer.Reset();
    _srv.Reset();
    _shader.reset();

    __super::Free();
}
