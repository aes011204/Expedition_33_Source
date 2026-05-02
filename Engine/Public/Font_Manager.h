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
	void Draw(const wstring& fontTag, const wchar_t* text, const Vec2& position, FXMVECTOR color);

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

