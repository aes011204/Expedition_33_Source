#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class CustomFont final : public Base
{
public:
    CustomFont(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ~CustomFont() override = default;

public:
    HRESULT Initialize(const wchar_t* fontFilePath);
    Vec2 Measure(const wchar_t* text) const;
    void Draw(
        const wchar_t* text,
        const Vec2& position,
        FXMVECTOR color,
        float rotationRadians = 0.f,
        const Vec2& pivot = Vec2(0.f, 0.f),
        float scale = 1.f);

private:
    ComPtr<Device> _device = { nullptr };
    ComPtr<Context> _context = { nullptr };

    Unique<SpriteBatch> _spriteBatch = { nullptr };
    Unique<SpriteFont>  _spriteFont = { nullptr };

public:
    static Shared<CustomFont> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const wchar_t* fontFilePath);
    void Free() override;
};

NS_END
