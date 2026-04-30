#pragma once

#include "Component.h"

NS_BEGIN(Engine)

class ENGINE_DLL Shader final : public Component
{
private:
	Shader(ComPtr<Device> device, ComPtr<Context> context);
	Shader(const Shader& Prototype);
	virtual ~Shader() = default;

public:
	virtual HRESULT Initialize_Prototype(const wchar_t* pShaderFilePath, const D3D11_INPUT_ELEMENT_DESC* pElements, uint32 iNumElements);
	virtual HRESULT Initialize(void* pArg);

public:
	
	HRESULT Begin(uint32 iPassIndex);
	HRESULT Bind_SRV(const char* pConstantName, ID3D11ShaderResourceView* pSRV);
	HRESULT Bind_SRVs(const char* pConstantName, ID3D11ShaderResourceView** ppSRVs, uint32 iNumSRVs);
	HRESULT Bind_Matrix(const char* pConstantName, const XMFLOAT4X4* pMatrix);
	HRESULT Bind_Matrices(const char* pConstantName, const XMFLOAT4X4* pMatrices, uint32 iNumMatrices);
	HRESULT Bind_RawValue(const char* pConstantName, const void* pData, uint32 iLength);

private:
	ID3DX11Effect*					_effect = { nullptr };
	uint32							_numPasses = {};

	vector<ID3D11InputLayout*>		_inputLayouts;

public:
	static Shared<Shader> Create(ComPtr<Device> device, ComPtr<Context> context, const wchar_t* pShaderFilePath, const D3D11_INPUT_ELEMENT_DESC* pElements, uint32 iNumElements);
	virtual Shared<Component> Clone(void* pArg) override;
	virtual void Free() override;
};

NS_END
