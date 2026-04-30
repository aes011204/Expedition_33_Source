#include "Shader.h"

Shader::Shader(ComPtr<Device> device, ComPtr<Context> context)
	: Component { device, context }
{

}

Shader::Shader(const Shader& Prototype)
	: Component{ Prototype }
	, _numPasses { Prototype._numPasses }
	, _effect { Prototype._effect }
	, _inputLayouts { Prototype._inputLayouts }
{
	Safe_AddRef(_effect);

	for (auto& pInputLayout : _inputLayouts)
		Safe_AddRef(pInputLayout);
	{

	}

}

HRESULT Shader::Initialize_Prototype(const wchar_t* pShaderFilePath, const D3D11_INPUT_ELEMENT_DESC* pElements, uint32 iNumElements)
{
	uint32			iHlslFlag = {};

#ifdef _DEBUG
	iHlslFlag |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
#else
	iHlslFlag |= D3DCOMPILE_OPTIMIZATION_LEVEL1;
#endif	

	if (FAILED(D3DX11CompileEffectFromFile(pShaderFilePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, iHlslFlag, 0,
		_device.Get(), &_effect, nullptr)))
		return E_FAIL;

	ID3DX11EffectTechnique*		pTechnique = _effect->GetTechniqueByIndex(0);
	if (nullptr == pTechnique)
		return E_FAIL;

	D3DX11_TECHNIQUE_DESC		TechniqueDesc{};

	pTechnique->GetDesc(&TechniqueDesc);

	_numPasses = TechniqueDesc.Passes;

	_inputLayouts.reserve(_numPasses);

	for (size_t i = 0; i < _numPasses; i++)
	{
		ID3D11InputLayout* pInputLayout = { nullptr };

		ID3DX11EffectPass*		pPass = pTechnique->GetPassByIndex(i);
		if (nullptr == pPass)
			return E_FAIL;

		D3DX11_PASS_DESC		PassDesc{};
		pPass->GetDesc(&PassDesc);

		if (FAILED(_device->CreateInputLayout(pElements, iNumElements, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &pInputLayout)))
			return E_FAIL;

		_inputLayouts.push_back(pInputLayout);
	}

    return S_OK;
}

HRESULT Shader::Initialize(void* pArg)
{
    return S_OK;
}

HRESULT Shader::Begin(uint32 iPassIndex)
{
	if (iPassIndex >= _numPasses || 
		nullptr == _inputLayouts[iPassIndex])
		return E_FAIL;

	_effect->GetTechniqueByIndex(0)->GetPassByIndex(iPassIndex)->Apply(0, _context.Get());

	_context->IASetInputLayout(_inputLayouts[iPassIndex]);	

	return S_OK;
}

HRESULT Shader::Bind_SRV(const char* pConstantName, ID3D11ShaderResourceView* pSRV)
{
	ID3DX11EffectVariable* pVariable = _effect->GetVariableByName(pConstantName);
	if (nullptr == pVariable)
	{
		MSG_BOX("Failed to throw value to shader");
		return E_FAIL;
	}

	ID3DX11EffectShaderResourceVariable* pSRVariable = pVariable->AsShaderResource();
	if (nullptr == pSRVariable)
	{
		MSG_BOX("Shader types do not match");
		return E_FAIL;
	}

	return pSRVariable->SetResource(pSRV);
}

HRESULT Shader::Bind_SRVs(const char* pConstantName, ID3D11ShaderResourceView** ppSRVs, uint32 iNumSRVs)
{
	ID3DX11EffectVariable* pVariable = _effect->GetVariableByName(pConstantName);
	if (nullptr == pVariable)
	{
		MSG_BOX("Failed to throw value to shader");
		return E_FAIL;
	}

	ID3DX11EffectShaderResourceVariable* pSRVariable = pVariable->AsShaderResource();
	if (nullptr == pSRVariable)
	{
		MSG_BOX("Shader types do not match");
		return E_FAIL;
	}

	return pSRVariable->SetResourceArray(ppSRVs, 0, iNumSRVs);
}

HRESULT Shader::Bind_Matrix(const char* pConstantName, const XMFLOAT4X4* pMatrix)
{
	ID3DX11EffectVariable*		pVariable = _effect->GetVariableByName(pConstantName);
	if (nullptr == pVariable)
	{
		MSG_BOX("Failed to throw value to shader");
		return E_FAIL;
	}

	ID3DX11EffectMatrixVariable*	pMatrixVariable = pVariable->AsMatrix();
	if (nullptr == pMatrixVariable)
	{
		MSG_BOX("Shader types do not match");
		return E_FAIL;
	}

	return pMatrixVariable->SetMatrix(reinterpret_cast<const float*>(pMatrix));
}

HRESULT Shader::Bind_Matrices(const char* pConstantName, const XMFLOAT4X4* pMatrices, uint32 iNumMatrices)
{
	ID3DX11EffectVariable* pVariable = _effect->GetVariableByName(pConstantName);
	if (nullptr == pVariable)
	{
		MSG_BOX("Failed to throw value to shader");
		return E_FAIL;
	}

	ID3DX11EffectMatrixVariable* pMatrixVariable = pVariable->AsMatrix();
	if (nullptr == pMatrixVariable)
	{
		MSG_BOX("Shader types do not match");
		return E_FAIL;
	}

	return pMatrixVariable->SetMatrixArray(reinterpret_cast<const float*>(pMatrices), 0, iNumMatrices);
}


HRESULT Shader::Bind_RawValue(const char* pConstantName, const void* pData, uint32 iLength)
{
	ID3DX11EffectVariable* pVariable = _effect->GetVariableByName(pConstantName);
	if (nullptr == pVariable)
	{
		MSG_BOX("Failed to throw value to shader");
		return E_FAIL;
	}

	return pVariable->SetRawValue(pData, 0, iLength);	
}

Shared<Shader> Shader::Create(ComPtr<Device> device, ComPtr<Context> context, const wchar_t* pShaderFilePath, const D3D11_INPUT_ELEMENT_DESC* pElements, uint32 iNumElements)
{
	auto pInstance = make_shared<Shader>(device, context);

	if (FAILED(pInstance->Initialize_Prototype(pShaderFilePath, pElements, iNumElements)))
	{
		MSG_BOX("Failed to Created : Shader");
		return nullptr;
	}
	return pInstance;
}


Shared<Component> Shader::Clone(void* pArg)
{
	auto pInstance = make_shared<Shader>(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : Shader");
		return nullptr;
	}
	return pInstance;
}


void Shader::Free()
{
    __super::Free();

	for (auto& pInputLayout : _inputLayouts)
		Safe_Release(pInputLayout);

	_inputLayouts.clear();

	Safe_Release(_effect);
}



