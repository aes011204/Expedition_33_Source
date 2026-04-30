#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Font_Manager final : public Base
{
private:
	Font_Manager(ComPtr<Device> device, ComPtr<Context> context);

public:
	virtual ~Font_Manager() = default;

public:
	HRESULT Add_Font(const wstring& strFontTag, const wchar_t* pFontFilePath);
	void Draw(const wstring& strFontTag, const wchar_t* pText, const Vec2& position, FXMVECTOR vColor);

private:
	ComPtr<Device> _device = { nullptr };
	ComPtr<Context> _context = { nullptr };
	map<const wstring, Shared<class CustomFont>>		_fonts;

private:
	Shared<class CustomFont> Find_Font(const wstring& strFontTag);

public:
	static Unique<Font_Manager> Create(ComPtr<Device> device, ComPtr<Context> context);
	virtual void Free() override;
};

NS_END

