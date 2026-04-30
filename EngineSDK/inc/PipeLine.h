#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class PipeLine final : public Base
{
private:
	PipeLine();

public:
	virtual ~PipeLine() = default;

public:
	const XMFLOAT4X4* Get_Transform(D3DTS eTransformState) const {
		return &_transformStateMatrices[ETOI(eTransformState)];
	}
	const XMFLOAT4* Get_CamPosition() const {
		return reinterpret_cast<const XMFLOAT4*>(&_transformStateInverseMatrices[ETOI(D3DTS::View)].m[3]);
	}

public:
	void Set_Transform(D3DTS eTransformState, FXMMATRIX TransformStateMatrix) {
		XMStoreFloat4x4(&_transformStateMatrices[ETOI(eTransformState)], TransformStateMatrix);
	}

public:
	HRESULT Bind_CamPosition(class Shader* pShader, const char* pConstantName);
	HRESULT Bind_TransformMatrix(D3DTS eTransformState, class Shader* pShader, const char* pConstantName);
	HRESULT Bind_TransformMatrix_Inverse(D3DTS eTransformState, class Shader* pShader, const char* pConstantName);
	void Update();
	

private:
	XMFLOAT4X4				_transformStateMatrices[ETOI(D3DTS::END)] = { };
	XMFLOAT4X4				_transformStateInverseMatrices[ETOI(D3DTS::END)] = { };


public:
	static Unique<PipeLine> Create();
	virtual void Free() override;

};

NS_END
