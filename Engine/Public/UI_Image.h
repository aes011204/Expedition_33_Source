#pragma once

#include "UI_Base.h"

NS_BEGIN(Engine)

class ShaderCom;
class VIBuffer_Rect;

class ENGINE_DLL UI_Image final : public UI_Base
{
public:
    struct UI_IMAGE_DESC : public UI_Base::UI_DESC
    {
        wstring texturePath{};
        wstring maskTexturePath{};
        Vec4 color{ 1.f, 1.f, 1.f, 1.f };
        uint32 shaderPrototypeLevelIndex{ 0 };
        wstring shaderPrototypeTag{ L"Shader_UI" };
        uint32 viBufferPrototypeLevelIndex{ 0 };
        wstring viBufferPrototypeTag{ L"VIBuffer_Rect" };
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

private:
    HRESULT Ready_RenderResources();

private:
    wstring _texturePath{};
    wstring _maskTexturePath{};
    Vec4 _color{ 1.f, 1.f, 1.f, 1.f };

    uint32 _shaderPrototypeLevelIndex{ 0 };
    wstring _shaderPrototypeTag{ L"Shader_UI" };
    uint32 _viBufferPrototypeLevelIndex{ 0 };
    wstring _viBufferPrototypeTag{ L"VIBuffer_Rect" };

    ComPtr<ShaderResourceView> _srv{};
    ComPtr<ShaderResourceView> _maskSrv{};

    Shared<ShaderCom> _shader{};
    Shared<VIBuffer_Rect> _viBuffer{};

public:
    static Shared<UI_Image> Create();
    Shared<UI_Base> Clone(void* arg) override;
    void Free() override;
};

NS_END
