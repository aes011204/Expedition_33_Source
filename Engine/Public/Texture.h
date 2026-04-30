#pragma once

#include "Component.h"

NS_BEGIN(Engine)

class ENGINE_DLL Texture final : public Component
{
public:
	Texture(ComPtr<Device> device, ComPtr<Context> context);
	Texture(const Texture& Prototype);
	virtual ~Texture() { Free(); }

public:
	virtual HRESULT Initialize_Prototype(const wchar_t* pTextureFilePath, uint32 iNumSRVs);
	virtual HRESULT Initialize(void* pArg) override;

public:
	HRESULT Bind_ShaderResourceView(class Shader* pShader, const char* pConstantName, uint32 iIndex);
	HRESULT Bind_ShaderResourceViews(class Shader* pShader, const char* pConstantName);

private:
	uint32										_numSRVs = {};
	vector<ID3D11ShaderResourceView*>			_srvs;

public:
	static Shared<Texture> Create(ComPtr<Device> device, ComPtr<Context> context, const wchar_t* pTextureFilePath, uint32 iNumSRVs);
	virtual Shared<Component> Clone(void* pArg) override;
	virtual void Free() override;

};

NS_END
