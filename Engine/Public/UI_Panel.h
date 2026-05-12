#pragma once

#include "UI_Base.h"

NS_BEGIN(Engine)

class ShaderCom;
class VIBuffer_Rect;

class ENGINE_DLL UI_Panel : public UI_Base
{
public:
    struct UI_PANEL_DESC : public UI_Base::UI_DESC
    {
        Vec4 color{ 1.f, 1.f, 1.f, 1.f };
        uint32 shaderPrototypeLevelIndex{ 0 };
        wstring shaderPrototypeTag{ L"Shader_UI" };
        uint32 viBufferPrototypeLevelIndex{ 0 };
        wstring viBufferPrototypeTag{ L"VIBuffer_Rect" };
    };

public:
    UI_Panel();
    UI_Panel(const UI_Panel& prototype);
    ~UI_Panel() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    HRESULT Render() override;
    void Set_Color(const Vec4& color) override { _color = color; }
    Vec4 Get_Color() const override { return _color; }

protected:
    virtual Vec4 Resolve_RenderColor() const;

private:
    HRESULT Ready_RenderResources();
    HRESULT Ready_WhiteTexture();

protected:
    Vec4 _color{ 1.f, 1.f, 1.f, 1.f };

private:
    uint32 _shaderPrototypeLevelIndex{ 0 };
    wstring _shaderPrototypeTag{ L"Shader_UI" };
    uint32 _viBufferPrototypeLevelIndex{ 0 };
    wstring _viBufferPrototypeTag{ L"VIBuffer_Rect" };

    ComPtr<ShaderResourceView> _whiteSrv{};
    Shared<ShaderCom> _shader{};
    Shared<VIBuffer_Rect> _viBuffer{};

public:
    static Shared<UI_Panel> Create();
    Shared<UI_Base> Clone(void* arg) override;
    void Free() override;
};

NS_END
