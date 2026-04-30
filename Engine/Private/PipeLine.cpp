#include "PipeLine.h"
#include "Shader.h"

PipeLine::PipeLine()
{
}

HRESULT PipeLine::Bind_CamPosition(Shader* pShader, const char* pConstantName)
{
	return pShader->Bind_RawValue(pConstantName, Get_CamPosition(), sizeof(XMFLOAT4));
}

HRESULT PipeLine::Bind_TransformMatrix(D3DTS eTransformState, Shader* pShader, const char* pConstantName)
{
	return pShader->Bind_Matrix(pConstantName, &_transformStateMatrices[ETOI(eTransformState)]);
}

HRESULT PipeLine::Bind_TransformMatrix_Inverse(D3DTS eTransformState, Shader* pShader, const char* pConstantName)
{
	return pShader->Bind_Matrix(pConstantName, &_transformStateInverseMatrices[ETOI(eTransformState)]);
}

void PipeLine::Update()
{
	for (size_t i = 0; i < ETOI(D3DTS::END); i++)
	{
		XMStoreFloat4x4(&_transformStateInverseMatrices[i],
			XMMatrixInverse(nullptr, XMLoadFloat4x4(&_transformStateMatrices[i])));
	}
}

Unique<PipeLine> PipeLine::Create()
{
	return Unique<PipeLine>{ new PipeLine() };
}

void PipeLine::Free()
{
	__super::Free();
}