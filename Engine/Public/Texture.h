#pragma once

#include "Component.h"

NS_BEGIN(Engine)

class ENGINE_DLL Texture final : public Component
{
public:
	Texture(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Texture(const Texture& prototype);
	virtual ~Texture() = default;

public:
	virtual HRESULT Initialize_Prototype(const wchar_t* texturePathPattern, uint32 numSRVs);
	virtual HRESULT Initialize(void* arg) override;

public:
	HRESULT Bind_ShaderResourceView(class ShaderCom* shader, const char* constantName, uint32 index);
	HRESULT Bind_ShaderResourceViews(class ShaderCom* shader, const char* constantName);
	const vector<ComPtr<ID3D11ShaderResourceView>>& Get_SRVs() const;

private:
	uint32										_numSRVs = {};
	vector<ComPtr<ID3D11ShaderResourceView>>	_srvs;

public:
	static Shared<Texture> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const wchar_t* texturePathPattern, uint32 numSRVs);
	virtual Shared<Component> Clone(void* arg) override;
	virtual void Free() override;

};

NS_END
