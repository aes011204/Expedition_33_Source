#include "PipeLine.h"
#include "ShaderCom.h"

PipeLine::PipeLine()
{
}

HRESULT PipeLine::Bind_CamPosition(ShaderCom* shader, const char* constantName)
{
	return shader->Bind_RawValue(constantName, Get_CamPosition(), sizeof(XMFLOAT4));
}

HRESULT PipeLine::Bind_TransformMatrix(D3DTS transformState, ShaderCom* shader, const char* constantName)
{
	return shader->Bind_Matrix(constantName, &_transformStateMatrices[ETOI(transformState)]);
}

HRESULT PipeLine::Bind_TransformMatrix_Inverse(D3DTS transformState, ShaderCom* shader, const char* constantName)
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

HRESULT PipeLine::Bind_CameraCB(ShaderCom* shader, float farPlane) const
{
    CHECK_NULL(shader, E_FAIL);

    CameraCB cameraCB{};
    cameraCB.viewMatrix = *Get_Transform(D3DTS::View);
    cameraCB.projMatrix = *Get_Transform(D3DTS::Proj);

    cameraCB.viewInverseMatrix = *Get_Transform_Inverse(D3DTS::View);
    cameraCB.projInverseMatrix = *Get_Transform_Inverse(D3DTS::Proj);

    cameraCB.cameraPosition = *Get_CamPosition();
    cameraCB.farPlane = farPlane;

    return shader->Bind_CBufferData(cameraCB);
}

Unique<PipeLine> PipeLine::Create()
{
	return Unique<PipeLine>{ new PipeLine() };
}

void PipeLine::Free()
{
	__super::Free();
}
