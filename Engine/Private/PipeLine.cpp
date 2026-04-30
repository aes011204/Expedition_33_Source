#include "PipeLine.h"
#include "Shader.h"

PipeLine::PipeLine()
{
}

HRESULT PipeLine::Bind_CamPosition(Shader* shader, const char* constantName)
{
	return shader->Bind_RawValue(constantName, Get_CamPosition(), sizeof(XMFLOAT4));
}

HRESULT PipeLine::Bind_TransformMatrix(D3DTS transformState, Shader* shader, const char* constantName)
{
	return shader->Bind_Matrix(constantName, &_transformStateMatrices[ETOI(transformState)]);
}

HRESULT PipeLine::Bind_TransformMatrix_Inverse(D3DTS transformState, Shader* shader, const char* constantName)
{
	return shader->Bind_Matrix(constantName, &_transformStateInverseMatrices[ETOI(transformState)]);
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