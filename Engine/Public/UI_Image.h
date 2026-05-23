#pragma once

#include "UI_Base.h"

NS_BEGIN(Engine)

class ShaderCom;
class VIBuffer_Rect;

class ENGINE_DLL UI_Image final : public UI_Base
{
public:
    enum class MaskMode : uint8
    {
        None,
        TextureMask,
        CircularHole,
    };

    enum class ImageRenderMode : uint8
    {
        Normal,
        PatternOverlay,
    };

    enum class FillOrigin : uint8
    {
        LeftToRight,
        RightToLeft,
        TopToBottom,
        BottomToTop,
    };

    struct UI_IMAGE_DESC : public UI_Base::UI_DESC
    {
        wstring texturePath{};
        wstring maskTexturePath{};
        wstring patternMaskTexturePath{};
        Vec4 color{ 1.f, 1.f, 1.f, 1.f };
        uint32 shaderPrototypeLevelIndex{ 0 };
        wstring shaderPrototypeTag{ L"Shader_UI" };
        uint32 viBufferPrototypeLevelIndex{ 0 };
        wstring viBufferPrototypeTag{ L"VIBuffer_Rect" };
        ImageRenderMode imageRenderMode = ImageRenderMode::Normal;

        // 인수: 원형 마스크 추가
        MaskMode maskMode = MaskMode::None;
        Vec2 holeCenterUV{ 0.5f, 0.5f }; // 화면 중앙
        float holeRadius = 0.f;
        float holeFeather = 0.05f;

        bool useFillRatio = false;
        float fillRatio = 1.f;
        FillOrigin fillOrigin = FillOrigin::LeftToRight;
    };

public:
    UI_Image();
    UI_Image(const UI_Image& prototype);
    ~UI_Image() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    HRESULT Render() override;
    void Set_Color(const Vec4& color) override { _color = color; }
    Vec4 Get_Color() const override { return _color; }
    void Set_OverlayEnabled(bool boolean) {_isOverlay = boolean;}
    void Set_GrayScaleEnabled(bool boolean) {_useGrayScale = boolean;}
    void Set_GrayAmount(float amount) {_grayAmount = amount;}

public:
    void Set_MaskMode(MaskMode mode) { _maskMode = mode; }
    void Set_HoleCenterUV(const Vec2& uv) { _holeCenterUV = uv; }
    void Set_HoleRadius(float radius) { _holeRadius = radius; }
    void Set_HoleFeather(float feather) { _holeFeather = feather; }
    void Set_UseFillRatio(bool useFillRatio) { _useFillRatio = useFillRatio; }
    void Set_FillRatio(float fillRatio);
    void Set_FillOrigin(FillOrigin fillOrigin) { _fillOrigin = fillOrigin; }

    HRESULT Set_TexturePath(const wstring& texturePath);
    HRESULT Set_TextureGuid(const string& textureGuid);

private:
    HRESULT Ready_RenderResources();
    HRESULT Load_TextureResource();

private:
    bool    _isOverlay = false;
    bool    _useGrayScale = false;
    float   _grayAmount = 0.f;
    wstring _texturePath{};
    wstring _maskTexturePath{};
    wstring _patternMaskTexturePath{};
    Vec4    _color{ 1.f, 1.f, 1.f, 1.f };

    uint32 _shaderPrototypeLevelIndex{ 0 };
    wstring _shaderPrototypeTag{ L"Shader_UI" };
    uint32 _viBufferPrototypeLevelIndex{ 0 };
    wstring _viBufferPrototypeTag{ L"VIBuffer_Rect" };

    ComPtr<ShaderResourceView> _srv{};
    ComPtr<ShaderResourceView> _maskSrv{};
    ComPtr<ShaderResourceView> _patternMaskSrv{};

    Shared<ShaderCom> _shader{};
    Shared<VIBuffer_Rect> _viBuffer{};

    // 인수: 원형 마스크 추가
    ImageRenderMode _imageRenderMode = ImageRenderMode::Normal;
    MaskMode _maskMode = MaskMode::None;
    Vec2 _holeCenterUV{ 0.5f, 0.5f };
    float _holeRadius = 0.f;
    float _holeFeather = 0.05f;
    bool _useFillRatio = false;
    float _fillRatio = 1.f;
    FillOrigin _fillOrigin = FillOrigin::LeftToRight;

public:
    static Shared<UI_Image> Create();
    Shared<UI_Base> Clone(void* arg) override;
    void Free() override;
};

NS_END
