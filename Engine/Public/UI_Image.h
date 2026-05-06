#pragma once

#include "UI_Base.h"

NS_BEGIN(Engine)

class ENGINE_DLL UI_Image final : public UI_Base
{
public:
    struct UI_IMAGE_DESC : public UI_Base::UI_DESC
    {
        wstring texturePath{};
        Vec4 color{ 1.f, 1.f, 1.f, 1.f };
    };

public:
    UI_Image();
    UI_Image(const UI_Image& prototype);
    ~UI_Image() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    HRESULT Render() override;

private:
    wstring _texturePath{};
    Vec4 _color{ 1.f, 1.f, 1.f, 1.f };
    Vec2 _textureSize{ 1.f, 1.f };
    ComPtr<ShaderResourceView> _srv{};
    Unique<SpriteBatch> _spriteBatch{};

public:
    static Shared<UI_Image> Create();
    Shared<UI_Base> Clone(void* arg) override;
    void Free() override;
};

NS_END
