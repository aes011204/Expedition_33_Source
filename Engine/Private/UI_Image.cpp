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
    , _patternMaskTexturePath{ prototype._patternMaskTexturePath }
    , _color{ prototype._color }
    , _shaderPrototypeLevelIndex{ prototype._shaderPrototypeLevelIndex }
    , _shaderPrototypeTag{ prototype._shaderPrototypeTag }
    , _viBufferPrototypeLevelIndex{ prototype._viBufferPrototypeLevelIndex }
    , _viBufferPrototypeTag{ prototype._viBufferPrototypeTag }
    , _imageRenderMode{ prototype._imageRenderMode }
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
        _patternMaskTexturePath = desc->patternMaskTexturePath;
        _color = desc->color;
        _shaderPrototypeLevelIndex = desc->shaderPrototypeLevelIndex;
        _shaderPrototypeTag = desc->shaderPrototypeTag;
        _viBufferPrototypeLevelIndex = desc->viBufferPrototypeLevelIndex;
        _viBufferPrototypeTag = desc->viBufferPrototypeTag;
        _imageRenderMode = desc->imageRenderMode;

        _maskMode = desc->maskMode;
        _holeCenterUV = desc->holeCenterUV;
        _holeRadius = desc->holeRadius;
        _holeFeather = desc->holeFeather;
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

    if (!_patternMaskTexturePath.empty())
    {
        if (FAILED(CreateWICTextureFromFile(device.Get(), _patternMaskTexturePath.c_str(), nullptr, _patternMaskSrv.GetAddressOf())))
        {
            LOG_WARN("UI_Image pattern mask texture load failed. path='{}'", String::ToString(_patternMaskTexturePath));
            _patternMaskSrv.Reset();
        }
    }

    if (FAILED(Ready_RenderResources()))
        LOG_WARN("UI_Image render resources are not ready. Retrying during Render.");

    return S_OK;
}

HRESULT UI_Image::Render()
{
    if (!Is_EffectivelyOpen())
        return S_OK;

    if (FAILED(__super::Render()))
        return E_FAIL;

    if (nullptr == _transform)
        return E_FAIL;

    if (nullptr == _srv)
        return S_OK;

    if (nullptr == _shader || nullptr == _viBuffer)
    {
        if (FAILED(Ready_RenderResources()))
            return S_OK;
    }

    if (nullptr == _shader || nullptr == _viBuffer)
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
    const float viewportAspect = viewportWidth / viewportHeight;
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

    if (FAILED(_shader->Bind_RawValue("g_Color", &_color, sizeof(Vec4))))
        return E_FAIL;

    if (FAILED(_shader->Bind_RawValue("g_UseGrayScale", &_useGrayScale, sizeof(bool))))
        return E_FAIL;

    if (FAILED(_shader->Bind_RawValue("g_GrayAmount", &_grayAmount, sizeof(float))))
        return E_FAIL;

    if (FAILED(_shader->Bind_RawValue("g_IsOverlay", &_isOverlay, sizeof(bool))))
        return E_FAIL;

    if (FAILED(_shader->Bind_SRV("g_Texture", _srv.Get())))
        return E_FAIL;

    if (FAILED(_shader->Bind_RawValue("g_HoleCenterUV", &_holeCenterUV, sizeof(Vec2))))
        return E_FAIL;

    if (FAILED(_shader->Bind_RawValue("g_HoleRadius", &_holeRadius, sizeof(float))))
        return E_FAIL;

    if (FAILED(_shader->Bind_RawValue("g_HoleFeather", &_holeFeather, sizeof(float))))
        return E_FAIL;

    if (FAILED(_shader->Bind_RawValue("g_ViewportAspect", &viewportAspect, sizeof(float))))
        return E_FAIL;

    uint32 passIndex = 0u;

    if (_imageRenderMode == ImageRenderMode::PatternOverlay)
    {
        if (nullptr == _maskSrv || nullptr == _patternMaskSrv)
            return S_OK;

        if (FAILED(_shader->Bind_SRV("g_MaskTexture", _maskSrv.Get())))
            return E_FAIL;

        if (FAILED(_shader->Bind_SRV("g_PatternMaskTexture", _patternMaskSrv.Get())))
            return E_FAIL;

        passIndex = 3u;
    }
    else if (_maskMode == MaskMode::CircularHole)
    {
        passIndex = 2u;
    }
    else if (nullptr != _maskSrv)
    {
        // 기존 maskTexturePath가 있는 UI는 예전처럼 자동으로 MaskPass
        if (FAILED(_shader->Bind_SRV("g_MaskTexture", _maskSrv.Get())))
            return E_FAIL;

        passIndex = 1u;
    }

    if (FAILED(_shader->Begin(passIndex)))
        return E_FAIL;

    if (FAILED(_viBuffer->Bind_Resources()))
        return E_FAIL;

    if (FAILED(_viBuffer->Render()))
        return E_FAIL;

    return S_OK;
}

HRESULT UI_Image::Ready_RenderResources()
{
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
    _patternMaskSrv.Reset();
    _maskSrv.Reset();
    _srv.Reset();

    __super::Free();
}
