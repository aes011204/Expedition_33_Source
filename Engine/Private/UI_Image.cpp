#include "UI_Image.h"
#include "GameInstance.h"
#include "ShaderCom.h"
#include "VIBuffer_Rect.h"

UI_Image::UI_Image()
{
}

UI_Image::UI_Image(const UI_Image& prototype)
    : UI_Base{ prototype }
    , _texturePath{ prototype._texturePath }
    , _maskTexturePath{ prototype._maskTexturePath }
    , _color{ prototype._color }
    , _shaderPrototypeLevelIndex{ prototype._shaderPrototypeLevelIndex }
    , _shaderPrototypeTag{ prototype._shaderPrototypeTag }
    , _viBufferPrototypeLevelIndex{ prototype._viBufferPrototypeLevelIndex }
    , _viBufferPrototypeTag{ prototype._viBufferPrototypeTag }
{
}

HRESULT UI_Image::Initialize_Prototype()
{
    return __super::Initialize_Prototype();
}

HRESULT UI_Image::Initialize(void* arg)
{
    if (FAILED(__super::Initialize(arg)))
        return E_FAIL;

    if (nullptr != arg)
    {
        const UI_IMAGE_DESC* desc = static_cast<const UI_IMAGE_DESC*>(arg);

        _texturePath = desc->texturePath;
        _maskTexturePath = desc->maskTexturePath;
        _color = desc->color;
        _shaderPrototypeLevelIndex = desc->shaderPrototypeLevelIndex;
        _shaderPrototypeTag = desc->shaderPrototypeTag;
        _viBufferPrototypeLevelIndex = desc->viBufferPrototypeLevelIndex;
        _viBufferPrototypeTag = desc->viBufferPrototypeTag;
    }

    if (_texturePath.empty())
        return S_OK;

    ComPtr<Device> device = GAME->Get_Device();
    CHECK_NULL(device, E_FAIL);

    if (FAILED(CreateWICTextureFromFile(device.Get(), _texturePath.c_str(), nullptr, _srv.GetAddressOf())))
        return E_FAIL;

    if (!_maskTexturePath.empty())
    {
        if (FAILED(CreateWICTextureFromFile(device.Get(), _maskTexturePath.c_str(), nullptr, _maskSrv.GetAddressOf())))
        {
            LOG_WARN("UI_Image mask texture load failed. path='{}'", String::ToString(_maskTexturePath));
            _maskSrv.Reset();
        }
    }

    _shader = dynamic_pointer_cast<ShaderCom>(
        GAME->Clone_Prototype(
            Prototype::Comopnent,
            _shaderPrototypeLevelIndex,
            _shaderPrototypeTag));
    CHECK_NULL(_shader, E_FAIL);

    _viBuffer = dynamic_pointer_cast<VIBuffer_Rect>(
        GAME->Clone_Prototype(
            Prototype::Comopnent,
            _viBufferPrototypeLevelIndex,
            _viBufferPrototypeTag));
    CHECK_NULL(_viBuffer, E_FAIL);

    return S_OK;
}

HRESULT UI_Image::Render()
{
    if (!Is_Open())
        return S_OK;

    if (FAILED(__super::Render()))
        return E_FAIL;

    if (nullptr == _transform)
        return E_FAIL;

    if (nullptr == _srv || nullptr == _shader || nullptr == _viBuffer)
        return S_OK;

    const Vec2& position = _transform->Get_Position();
    const Vec2& size     = _transform->Get_Size();
    const Vec2& scale    = _transform->Get_Scale();

    const float width = size.x * scale.x;
    const float height = size.y * scale.y;

    if (width <= 0.f || height <= 0.f)
        return S_OK;

    const float viewportWidth = static_cast<float>(max(1u, GAME->Get_ViewportWidth()));
    const float viewportHeight = static_cast<float>(max(1u, GAME->Get_ViewportHeight()));
    const Vec2 center(position.x + width * 0.5f, position.y + height * 0.5f);

    Matrix worldMatrix =
        Matrix::CreateScale(width, height, 1.f) *
        Matrix::CreateRotationZ(-_transform->Get_RotationRadians()) *
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

    if (FAILED(_shader->Bind_RawValue("g_Color", &_color, sizeof(Vec4))))
        return E_FAIL;

    if (FAILED(_shader->Bind_SRV("g_Texture", _srv.Get())))
        return E_FAIL;

    const uint32 passIndex = (nullptr != _maskSrv) ? 1u : 0u;

    if (nullptr != _maskSrv)
    {
        if (FAILED(_shader->Bind_SRV("g_MaskTexture", _maskSrv.Get())))
            return E_FAIL;
    }

    if (FAILED(_shader->Begin(passIndex)))
        return E_FAIL;

    if (FAILED(_viBuffer->Bind_Resources()))
        return E_FAIL;

    if (FAILED(_viBuffer->Render()))
        return E_FAIL;

    return S_OK;
}

Shared<UI_Image> UI_Image::Create()
{
    auto instance = make_shared<UI_Image>();

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : UI_Image");
        return nullptr;
    }

    return instance;
}

Shared<UI_Base> UI_Image::Clone(void* arg)
{
    auto instance = make_shared<UI_Image>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : UI_Image");
        return nullptr;
    }

    return instance;
}

void UI_Image::Free()
{
    _viBuffer.reset();
    _shader.reset();
    _maskSrv.Reset();
    _srv.Reset();

    __super::Free();
}
