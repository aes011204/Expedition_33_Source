#include "UI_Image.h"
#include "DebugMemoryTracker.h"
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
    , _materialVfxMode{ prototype._materialVfxMode }
    , _materialVfxNoiseTexturePath{ prototype._materialVfxNoiseTexturePath }
    , _materialVfxColor{ prototype._materialVfxColor }
    , _materialVfxCoreColor{ prototype._materialVfxCoreColor }
    , _materialVfxFlowColor{ prototype._materialVfxFlowColor }
    , _materialVfxGlow{ prototype._materialVfxGlow }
    , _materialVfxAlpha{ prototype._materialVfxAlpha }
    , _materialVfxCoreStrength{ prototype._materialVfxCoreStrength }
    , _materialVfxCoreWidth{ prototype._materialVfxCoreWidth }
    , _materialVfxCoreSoftness{ prototype._materialVfxCoreSoftness }
    , _materialVfxNoiseStrength{ prototype._materialVfxNoiseStrength }
    , _materialVfxUvScroll{ prototype._materialVfxUvScroll }
    , _materialVfxFlowSpeed{ prototype._materialVfxFlowSpeed }
    , _materialVfxPreserveColor{ prototype._materialVfxPreserveColor }
    , _materialVfxEdgeWidth{ prototype._materialVfxEdgeWidth }
    , _materialVfxEdgeSoftness{ prototype._materialVfxEdgeSoftness }
    , _materialVfxErosion{ prototype._materialVfxErosion }
    , _materialVfxFlicker{ prototype._materialVfxFlicker }
    , _materialVfxDistortion{ prototype._materialVfxDistortion }
    , _materialVfxEdgeWobbleSpeed{ prototype._materialVfxEdgeWobbleSpeed }
    , _materialVfxTime{ prototype._materialVfxTime }
    , _buttonSheenColor{ prototype._buttonSheenColor }
    , _buttonSheenSpeed{ prototype._buttonSheenSpeed }
    , _buttonSheenWidth{ prototype._buttonSheenWidth }
    , _buttonSheenSoftness{ prototype._buttonSheenSoftness }
    , _buttonSheenAngle{ prototype._buttonSheenAngle }
    , _buttonSheenNoiseStrength{ prototype._buttonSheenNoiseStrength }
    , _buttonSheenTime{ prototype._buttonSheenTime }
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
        _materialVfxMode = desc->materialVfxMode;
        _materialVfxNoiseTexturePath = desc->materialVfxNoiseTexturePath;
        _materialVfxColor = desc->materialVfxColor;
        _materialVfxCoreColor = desc->materialVfxCoreColor;
        _materialVfxFlowColor = desc->materialVfxFlowColor;
        _materialVfxGlow = max(0.f, desc->materialVfxGlow);
        _materialVfxAlpha = max(0.f, desc->materialVfxAlpha);
        _materialVfxCoreStrength = max(0.f, desc->materialVfxCoreStrength);
        _materialVfxCoreWidth = max(0.001f, desc->materialVfxCoreWidth);
        _materialVfxCoreSoftness = max(0.001f, desc->materialVfxCoreSoftness);
        _materialVfxNoiseStrength = min(max(desc->materialVfxNoiseStrength, 0.f), 1.f);
        _materialVfxUvScroll = desc->materialVfxUvScroll;
        _materialVfxFlowSpeed = min(max(desc->materialVfxFlowSpeed, 0.f), 4.f);
        _materialVfxPreserveColor = min(max(desc->materialVfxPreserveColor, 0.f), 1.f);
        _materialVfxEdgeWidth = max(0.001f, desc->materialVfxEdgeWidth);
        _materialVfxEdgeSoftness = max(0.001f, desc->materialVfxEdgeSoftness);
        _materialVfxErosion = min(max(desc->materialVfxErosion, 0.f), 1.f);
        _materialVfxFlicker = min(max(desc->materialVfxFlicker, 0.f), 1.f);
        _materialVfxDistortion = min(max(desc->materialVfxDistortion, 0.f), 0.1f);
        _materialVfxEdgeWobbleSpeed = min(max(desc->materialVfxEdgeWobbleSpeed, 0.f), 4.f);
        _buttonSheenColor = desc->buttonSheenColor;
        _buttonSheenSpeed = min(max(desc->buttonSheenSpeed, 0.f), 4.f);
        _buttonSheenWidth = min(max(desc->buttonSheenWidth, 0.001f), 1.f);
        _buttonSheenSoftness = min(max(desc->buttonSheenSoftness, 0.001f), 1.f);
        _buttonSheenAngle = min(max(desc->buttonSheenAngle, -2.f), 2.f);
        _buttonSheenNoiseStrength = min(max(desc->buttonSheenNoiseStrength, 0.f), 1.f);
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

    if (!_maskTexturePath.empty() || !_patternMaskTexturePath.empty() || !_materialVfxNoiseTexturePath.empty())
    {
        ComPtr<Device> device = GAME->Get_Device();
        CHECK_NULL(device, E_FAIL);

        if (!_maskTexturePath.empty() &&
            FAILED(CreateWICTextureFromFile(device.Get(), _maskTexturePath.c_str(), nullptr, _maskSrv.GetAddressOf())))
        {
            LOG_WARN("UI_Image mask texture load failed. path='{}'", String::ToString(_maskTexturePath));
            _maskSrv.Reset();
        }
        else if (!_maskTexturePath.empty() && _maskSrv != nullptr)
        {
            DebugMemoryTracker::Record_TextureSRV(
                "ui.texture",
                "UI_Image.Mask",
                _maskTexturePath,
                _maskSrv.Get());
        }

        if (!_patternMaskTexturePath.empty() &&
            FAILED(CreateWICTextureFromFile(device.Get(), _patternMaskTexturePath.c_str(), nullptr, _patternMaskSrv.GetAddressOf())))
        {
            LOG_WARN("UI_Image pattern mask texture load failed. path='{}'", String::ToString(_patternMaskTexturePath));
            _patternMaskSrv.Reset();
        }
        else if (!_patternMaskTexturePath.empty() && _patternMaskSrv != nullptr)
        {
            DebugMemoryTracker::Record_TextureSRV(
                "ui.texture",
                "UI_Image.PatternMask",
                _patternMaskTexturePath,
                _patternMaskSrv.Get());
        }

        if (!_materialVfxNoiseTexturePath.empty() &&
            FAILED(CreateWICTextureFromFile(device.Get(), _materialVfxNoiseTexturePath.c_str(), nullptr, _materialVfxNoiseSrv.GetAddressOf())))
        {
            LOG_WARN("UI_Image material vfx noise texture load failed. path='{}'", String::ToString(_materialVfxNoiseTexturePath));
            _materialVfxNoiseSrv.Reset();
        }
        else if (!_materialVfxNoiseTexturePath.empty() && _materialVfxNoiseSrv != nullptr)
        {
            DebugMemoryTracker::Record_TextureSRV(
                "ui.texture",
                "UI_Image.MaterialVfxNoise",
                _materialVfxNoiseTexturePath,
                _materialVfxNoiseSrv.Get());
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

    if (_imageRenderMode == ImageRenderMode::MaterialVfx)
    {
        _materialVfxTime = fmodf(_materialVfxTime + max(0.f, GAME->Get_UnscaledTimeDelta()), 32.f);

        if (FAILED(_shader->Bind_SRV("g_MaskTexture", nullptr != _maskSrv ? _maskSrv.Get() : _srv.Get())))
            return E_FAIL;

        if (FAILED(_shader->Bind_SRV("g_PatternMaskTexture", nullptr != _materialVfxNoiseSrv ? _materialVfxNoiseSrv.Get() : _srv.Get())))
            return E_FAIL;

        const float materialUseBaseMask = nullptr == _maskSrv ? 1.f : 0.f;
        const float materialHasNoise = nullptr != _materialVfxNoiseSrv ? 1.f : 0.f;
        const uint32 materialVfxMode = static_cast<uint32>(_materialVfxMode);
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxMode", &materialVfxMode, sizeof(uint32))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxColor", &_materialVfxColor, sizeof(Vec4))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxCoreColor", &_materialVfxCoreColor, sizeof(Vec4))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxFlowColor", &_materialVfxFlowColor, sizeof(Vec4))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxGlow", &_materialVfxGlow, sizeof(float))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxAlpha", &_materialVfxAlpha, sizeof(float))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxCoreStrength", &_materialVfxCoreStrength, sizeof(float))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxCoreWidth", &_materialVfxCoreWidth, sizeof(float))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxCoreSoftness", &_materialVfxCoreSoftness, sizeof(float))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxNoiseStrength", &_materialVfxNoiseStrength, sizeof(float))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxUvScroll", &_materialVfxUvScroll, sizeof(Vec2))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxFlowSpeed", &_materialVfxFlowSpeed, sizeof(float))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxPreserveColor", &_materialVfxPreserveColor, sizeof(float))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxEdgeWidth", &_materialVfxEdgeWidth, sizeof(float))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxEdgeSoftness", &_materialVfxEdgeSoftness, sizeof(float))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxErosion", &_materialVfxErosion, sizeof(float))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxFlicker", &_materialVfxFlicker, sizeof(float))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxDistortion", &_materialVfxDistortion, sizeof(float))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxEdgeWobbleSpeed", &_materialVfxEdgeWobbleSpeed, sizeof(float))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxTime", &_materialVfxTime, sizeof(float))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxUseBaseMask", &materialUseBaseMask, sizeof(float))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_MaterialVfxHasNoise", &materialHasNoise, sizeof(float))))
            return E_FAIL;

        passIndex = 7u;
    }
    else if (_imageRenderMode == ImageRenderMode::ButtonSheen)
    {
        _buttonSheenTime = fmodf(_buttonSheenTime + max(0.f, GAME->Get_UnscaledTimeDelta()), 32.f);

        if (FAILED(_shader->Bind_SRV("g_MaskTexture", nullptr != _maskSrv ? _maskSrv.Get() : _srv.Get())))
            return E_FAIL;

        if (FAILED(_shader->Bind_RawValue("g_ButtonSheenColor", &_buttonSheenColor, sizeof(Vec4))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_ButtonSheenSpeed", &_buttonSheenSpeed, sizeof(float))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_ButtonSheenWidth", &_buttonSheenWidth, sizeof(float))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_ButtonSheenSoftness", &_buttonSheenSoftness, sizeof(float))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_ButtonSheenAngle", &_buttonSheenAngle, sizeof(float))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_ButtonSheenNoiseStrength", &_buttonSheenNoiseStrength, sizeof(float))))
            return E_FAIL;
        if (FAILED(_shader->Bind_RawValue("g_ButtonSheenTime", &_buttonSheenTime, sizeof(float))))
            return E_FAIL;

        passIndex = 8u;
    }
    else if (_imageRenderMode == ImageRenderMode::PatternOverlay)
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

HRESULT UI_Image::Set_ExternalShaderResource(ShaderResourceView* srv)
{
    if (srv == nullptr)
        return E_FAIL;

    _srv.Reset();
    srv->AddRef();
    _srv.Attach(srv);

    return S_OK;
}

void UI_Image::Clear_ExternalShaderResource()
{
    if (FAILED(Load_TextureResource()))
        LOG_WARN("UI_Image failed to restore file texture after external SRV clear. path='{}'", String::ToString(_texturePath));
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

    DebugMemoryTracker::Record_TextureSRV(
        "ui.texture",
        "UI_Image.Texture",
        _texturePath,
        _srv.Get());

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
    _materialVfxNoiseSrv.Reset();
    _patternMaskSrv.Reset();
    _maskSrv.Reset();
    _srv.Reset();

    __super::Free();
}
