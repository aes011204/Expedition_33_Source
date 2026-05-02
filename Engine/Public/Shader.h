#pragma once

#include "Component.h"

NS_BEGIN(Engine)

class ENGINE_DLL Shader final : public Component
{
public:
	Shader(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Shader(const Shader& prototype);
	virtual ~Shader() = default;

public:
	virtual HRESULT Initialize_Prototype(const wchar_t* shaderFilePath, const D3D11_INPUT_ELEMENT_DESC* elements, uint32 numElements);
	virtual HRESULT Initialize(void* arg);

public:
	
	HRESULT Begin(uint32 passIndex);
	HRESULT Bind_SRV(const char* constantName, ID3D11ShaderResourceView* srv);
	HRESULT Bind_SRVs(const char* constantName, ID3D11ShaderResourceView** ppSRVs, uint32 numSRVs);
	HRESULT Bind_Matrix(const char* constantName, const XMFLOAT4X4* matrix);
	HRESULT Bind_Matrices(const char* constantName, const XMFLOAT4X4* matrices, uint32 numMatrices);
	HRESULT Bind_RawValue(const char* constantName, const void* data, uint32 length);

private:
	ID3DX11Effect*					_effect = { nullptr };
	uint32							_numPasses = {};

	vector<ID3D11InputLayout*>		_inputLayouts;

public:
	static Shared<Shader> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const wchar_t* shaderFilePath, const D3D11_INPUT_ELEMENT_DESC* elements, uint32 numElements);
	virtual Shared<Component> Clone(void* arg) override;
	virtual void Free() override;
};

NS_END
