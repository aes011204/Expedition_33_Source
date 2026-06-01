#include "UI_Panel.h"

#include "GameInstance.h"
#include "ShaderCom.h"
#include "VIBuffer_Rect.h"

UI_Panel::UI_Panel()
{
}

UI_Panel::UI_Panel(const UI_Panel& prototype)
    : UI_Base{ prototype }
    , _color{ prototype._color }
    , _shaderPrototypeLevelIndex{ prototype._shaderPrototypeLevelIndex }
    , _shaderPrototypeTag{ prototype._shaderPrototypeTag }
    , _viBufferPrototypeLevelIndex{ prototype._viBufferPrototypeLevelIndex }
    , _viBufferPrototypeTag{ prototype._viBufferPrototypeTag }
{
}

HRESULT UI_Panel::Initialize_Prototype()
{
    return __super::Initialize_Prototype();
}

HRESULT UI_Panel::Initialize(void* arg)
{
    if (FAILED(__super::Initialize(arg)))
        return E_FAIL;

    if (nullptr != arg)
    {
        const UI_PANEL_DESC* desc = static_cast<const UI_PANEL_DESC*>(arg);

        _color = desc->color;
        _shaderPrototypeLevelIndex = desc->shaderPrototypeLevelIndex;
        _shaderPrototypeTag = desc->shaderPrototypeTag;
        _viBufferPrototypeLevelIndex = desc->viBufferPrototypeLevelIndex;
        _viBufferPrototypeTag = desc->viBufferPrototypeTag;
    }

    if (FAILED(Ready_RenderResources()))
        LOG_WARN("UI_Panel render resources are not ready. Retrying during Render.");

    return S_OK;
}

HRESULT UI_Panel::Render()
{
    if (!Is_EffectivelyOpen())
        return S_OK;

    if (FAILED(__super::Render()))
        return E_FAIL;

    if (nullptr == _transform)
        return E_FAIL;

    if (nullptr == _whiteSrv || nullptr == _shader || nullptr == _viBuffer)
    {
        if (FAILED(Ready_RenderResources()))
            return S_OK;
    }

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

    const Vec4 color = Resolve_RenderColor();

    if (FAILED(_shader->Bind_Matrix("g_WorldMatrix", &worldMatrix)))
        return E_FAIL;

    if (FAILED(_shader->Bind_Matrix("g_ViewMatrix", &viewMatrix)))
        return E_FAIL;

    if (FAILED(_shader->Bind_Matrix("g_ProjMatrix", &projMatrix)))
        return E_FAIL;

    if (FAILED(_shader->Bind_RawValue("g_Color", &color, sizeof(Vec4))))
        return E_FAIL;

    if (FAILED(_shader->Bind_SRV("g_Texture", _whiteSrv.Get())))
        return E_FAIL;

    if (FAILED(_shader->Begin(0)))
        return E_FAIL;

    if (FAILED(_viBuffer->Bind_Resources()))
        return E_FAIL;

    if (FAILED(_viBuffer->Render()))
        return E_FAIL;

    return S_OK;
}

Vec4 UI_Panel::Resolve_RenderColor() const
{
    return Get_EffectiveRenderColor(_color);
}

HRESULT UI_Panel::Ready_RenderResources()
{
    if (FAILED(Ready_WhiteTexture()))
        return E_FAIL;

    if (nullptr == _shader)
    {
        _shader = dynamic_pointer_cast<ShaderCom>(
            GAME->Clone_Prototype(
                Prototype::Comopnent,
                _shaderPrototypeLevelIndex,
                _shaderPrototypeTag));
    }
    CHECK_NULL(_shader, E_FAIL);

    if (nullptr == _viBuffer)
    {
        _viBuffer = dynamic_pointer_cast<VIBuffer_Rect>(
            GAME->Clone_Prototype(
                Prototype::Comopnent,
                _viBufferPrototypeLevelIndex,
                _viBufferPrototypeTag));
    }
    CHECK_NULL(_viBuffer, E_FAIL);

    return S_OK;
}

HRESULT UI_Panel::Ready_WhiteTexture()
{
    if (nullptr != _whiteSrv)
        return S_OK;

    ComPtr<Device> device = GAME->Get_Device();
    CHECK_NULL(device, E_FAIL);

    const uint32 whitePixel = 0xffffffffu;

    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = 1;
    textureDesc.Height = 1;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initialData = {};
    initialData.pSysMem = &whitePixel;
    initialData.SysMemPitch = sizeof(whitePixel);

    ComPtr<ID3D11Texture2D> texture = nullptr;
    if (FAILED(device->CreateTexture2D(&textureDesc, &initialData, texture.GetAddressOf())))
        return E_FAIL;

    if (FAILED(device->CreateShaderResourceView(texture.Get(), nullptr, _whiteSrv.GetAddressOf())))
        return E_FAIL;

    return S_OK;
}

Shared<UI_Panel> UI_Panel::Create()
{
    auto instance = make_shared<UI_Panel>();

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : UI_Panel");
        return nullptr;
    }

    return instance;
}

Shared<UI_Base> UI_Panel::Clone(void* arg)
{
    auto instance = make_shared<UI_Panel>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : UI_Panel");
        return nullptr;
    }

    return instance;
}

void UI_Panel::Free()
{
    _viBuffer.reset();
    _shader.reset();
    _whiteSrv.Reset();

    __super::Free();
}
