#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Font_Manager final : public Base
{
public:
	Font_Manager(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual ~Font_Manager();

public:
	HRESULT Add_Font(const wstring& fontTag, const wchar_t* fontFilePath);
    Vec2 Measure(const wstring& fontTag, const wchar_t* text);
	void Draw(
        const wstring& fontTag,
        const wchar_t* text,
        const Vec2& position,
        FXMVECTOR color,
        float rotationRadians = 0.f,
        const Vec2& pivot = Vec2(0.f, 0.f),
        float scale = 1.f);

private:
	ComPtr<Device> _device = { nullptr };
	ComPtr<Context> _context = { nullptr };
	map<const wstring, Shared<class CustomFont>>		_fonts;

private:
	Shared<class CustomFont> Find_Font(const wstring& fontTag);

public:
	static Unique<Font_Manager> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual void Free() override;
};

NS_END

