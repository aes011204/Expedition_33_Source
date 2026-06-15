#include "UI_Image.h"
#include "GameInstance.h"
#include "ShaderCom.h"
#include "VIBuffer_Rect.h"

UI_Image::UI_Image()
{
}

UI_Image::UI_Image(const UI_Image& prototype)
    : UI_Base{ prototype }
    , _isOverlay{ prototype._isOverlay }
    , _useGrayScale{ prototype._useGrayScale }
    , _grayAmount{ prototype._grayAmount }
    , _texturePath{ prototype._texturePath }
    , _maskTexturePath{ prototype._maskTexturePath }
    , _patternMaskTexturePath{ prototype._patternMaskTexturePath }
    , _patternUvOffset{ prototype._patternUvOffset }
    , _patternUvScale{ prototype._patternUvScale }
    , _color{ prototype._color }
    , _shaderPrototypeLevelIndex{ prototype._shaderPrototypeLevelIndex }
    , _shaderPrototypeTag{ prototype._shaderPrototypeTag }
    , _viBufferPrototypeLevelIndex{ prototype._viBufferPrototypeLevelIndex }
    , _viBufferPrototypeTag{ prototype._viBufferPrototypeTag }
    , _imageRenderMode{ prototype._imageRenderMode }
    , _maskTextureMode{ prototype._maskTextureMode }
    , _maskMode{ prototype._maskMode }
    , _holeCenterUV{ prototype._holeCenterUV }
    , _holeRadius{ prototype._holeRadius }
    , _holeFeather{ prototype._holeFeather }
    , _useFillRatio{ prototype._useFillRatio }
    , _fillRatio{ prototype._fillRatio }
    , _fillOrigin{ prototype._fillOrigin }
{
}

Vec2 UI_Image::Get_DefaultPatternUvOffset()
{
    return Vec2(0.f, 0.f);
}

Vec2 UI_Image::Get_DefaultPatternUvScale()
{
    return Vec2(1.f, 1.f);
}

Vec2 UI_Image::Clamp_PatternUvOffset(const Vec2& value)
{
    return Vec2(
        min(max(value.x, -1.f), 1.f),
        min(max(value.y, -1.f), 1.f));
}

Vec2 UI_Image::Clamp_PatternUvScale(const Vec2& value)
{
    return Vec2(
        min(max(value.x, 0.1f), 8.f),
        min(max(value.y, 0.1f), 8.f));
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
        _patternUvOffset = Clamp_PatternUvOffset(desc->patternUvOffset);
        _patternUvScale = Clamp_PatternUvScale(desc->patternUvScale);
        _color = desc->color;
        _shaderPrototypeLevelIndex = desc->shaderPrototypeLevelIndex;
        _shaderPrototypeTag = desc->shaderPrototypeTag;
        _viBufferPrototypeLevelIndex = desc->viBufferPrototypeLevelIndex;
        _viBufferPrototypeTag = desc->viBufferPrototypeTag;
        _imageRenderMode = desc->imageRenderMode;
        _maskTextureMode = desc->maskTextureMode;

        _maskMode = desc->maskMode;
        _holeCenterUV = desc->holeCenterUV;
        _holeRadius = desc->holeRadius;
        _holeFeather = desc->holeFeather;
        _useFillRatio = desc->useFillRatio;
        _fillRatio = ::clamp(desc->fillRatio, 0.f, 1.f);
        _fillOrigin = desc->fillOrigin;
    }

    if (FAILED(Load_TextureResource()))
        return E_FAIL;

    if (!_maskTexturePath.empty() || !_patternMaskTexturePath.empty())
    {
        ComPtr<Device> device = GAME->Get_Device();
        CHECK_NULL(device, E_FAIL);

        if (!_maskTexturePath.empty() &&
            FAILED(CreateWICTextureFromFile(device.Get(), _maskTexturePath.c_str(), nullptr, _maskSrv.GetAddressOf())))
        {
            LOG_WARN("UI_Image mask texture load failed. path='{}'", String::ToString(_maskTexturePath));
            _maskSrv.Reset();
        }

        if (!_patternMaskTexturePath.empty() &&
            FAILED(CreateWICTextureFromFile(device.Get(), _patternMaskTexturePath.c_str(), nullptr, _patternMaskSrv.GetAddressOf())))
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

    const Vec4 renderColor = Get_EffectiveRenderColor(_color);

    if (FAILED(_shader->Bind_RawValue("g_Color", &renderColor, sizeof(Vec4))))
        return E_FAIL;

    if (FAILED(_shader->Bind_RawValue("g_PatternUvOffset", &_patternUvOffset, sizeof(Vec2))))
        return E_FAIL;

    if (FAILED(_shader->Bind_RawValue("g_PatternUvScale", &_patternUvScale, sizeof(Vec2))))
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

    if (FAILED(_shader->Bind_RawValue("g_UseFillRatio", &_useFillRatio, sizeof(bool))))
        return E_FAIL;

    if (FAILED(_shader->Bind_RawValue("g_FillRatio", &_fillRatio, sizeof(float))))
        return E_FAIL;

    const uint32 fillOriginValue = static_cast<uint32>(_fillOrigin);
    if (FAILED(_shader->Bind_RawValue("g_FillOrigin", &fillOriginValue, sizeof(uint32))))
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

        passIndex = _useFillRatio ? 6u : 3u;
    }
    else if (_maskMode == MaskMode::CircularHole)
    {
        passIndex = 2u;
    }
    else if (_useFillRatio)
    {
        passIndex = 4u; // HP 바처럼 UV crop 채움이 필요한 이미지는 전용 fill pass를 탄다.
    }
    else if (nullptr != _maskSrv)
    {
        // 기존 maskTexturePath가 있는 UI는 예전처럼 자동으로 MaskPass
        if (FAILED(_shader->Bind_SRV("g_MaskTexture", _maskSrv.Get())))
            return E_FAIL;

        passIndex = (_maskTextureMode == MaskTextureMode::BlackWhite) ? 5u : 1u;
    }

    if (FAILED(_shader->Begin(passIndex)))
        return E_FAIL;

    const bool isBackgroundUI = Get_RenderGroup() == UI_RenderGroup::Background;
    if (isBackgroundUI)
    {
        // Background UI는 플레이어/무기 stencil=1 영역을 피해 그린다.
        CHECK_FAILED(GAME->Bind_BackgroundUIBehindPlayerStencilState(), E_FAIL);
    }

    if (FAILED(_viBuffer->Bind_Resources()))
        return E_FAIL;

    if (FAILED(_viBuffer->Render()))
        return E_FAIL;

    if (isBackgroundUI)
    {
        // 이후 Font/Sprite 기반 Foreground UI가 stencil state를 물려받지 않도록 복구한다.
        CHECK_FAILED(GAME->Bind_UINoDepthStencilState(), E_FAIL);
    }

    return S_OK;
}

void UI_Image::Set_FillRatio(float fillRatio)
{
    _fillRatio = ::clamp(fillRatio, 0.f, 1.f);
}

HRESULT UI_Image::Set_TexturePath(const wstring& texturePath)
{
    if (_texturePath == texturePath)
        return S_OK;

    _texturePath = texturePath;
    return Load_TextureResource();
}

HRESULT UI_Image::Set_TextureGuid(const string& textureGuid)
{
    if (textureGuid.empty())
        return E_FAIL;

    const wstring resolvedPath = GAME->Resolve_AssetPath(textureGuid);
    if (resolvedPath.empty())
        return E_FAIL;

    return Set_TexturePath(resolvedPath);
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

HRESULT UI_Image::Load_TextureResource()
{
    _srv.Reset();

    if (_texturePath.empty())
        return S_OK;

    ComPtr<Device> device = GAME->Get_Device();
    CHECK_NULL(device, E_FAIL);

    if (FAILED(CreateWICTextureFromFile(device.Get(), _texturePath.c_str(), nullptr, _srv.GetAddressOf())))
    {
        LOG_WARN("UI_Image texture load failed. path='{}'", String::ToString(_texturePath));
        return E_FAIL;
    }

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
