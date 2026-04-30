#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class CustomFont final : public Base
{
private:
	CustomFont(ComPtr<Device> device, ComPtr<Context> context);
	virtual ~CustomFont() = default;

public:
	HRESULT Initialize(const wchar_t* pFontFilePath);
	void Draw(const wchar_t* pText, const Vec2& position, FXMVECTOR vColor);

private:
	ComPtr<Device> _device = { nullptr };
	ComPtr<Context> _context = { nullptr };

	SpriteBatch*				_spriteBatch = { nullptr };
	SpriteFont*					_spriteFont = { nullptr };

public:
	static Shared<CustomFont> Create(ComPtr<Device> device, ComPtr<Context> context, const wchar_t* pFontFilePath);
	virtual void Free() override;
};

NS_END

