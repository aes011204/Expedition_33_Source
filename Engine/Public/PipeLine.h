#pragma once

#include "Base.h"
#include "Material_Define.h"

NS_BEGIN(Engine)

class PipeLine final : public Base
{
private:
	PipeLine();

public:
	virtual ~PipeLine() = default;

public:
	const Matrix* Get_Transform(D3DTS transformState) const {
		return reinterpret_cast<const Matrix*>(&_transformStateMatrices[ETOI(transformState)]);
	}

    const Matrix* Get_Transform_Inverse(D3DTS transformState) const
	{
	    return reinterpret_cast<const Matrix*>(&_transformStateInverseMatrices[ETOI(transformState)]);
	}

	const Vec4* Get_CamPosition() const {
		return reinterpret_cast<const Vec4*>(&_transformStateInverseMatrices[ETOI(D3DTS::View)].m[3]);
	}

public:
	void Set_Transform(D3DTS transformState, FXMMATRIX transformStateMatrix) {
		XMStoreFloat4x4(&_transformStateMatrices[ETOI(transformState)], transformStateMatrix);
	}

public:
	HRESULT Bind_CamPosition(class ShaderCom* shader, const char* constantName);
	HRESULT Bind_TransformMatrix(D3DTS transformState, class ShaderCom* shader, const char* constantName);
	HRESULT Bind_TransformMatrix_Inverse(D3DTS transformState, class ShaderCom* shader, const char* constantName);
	void    Update();
	
public:
    HRESULT Bind_CameraCB(ShaderCom* shader, float farPlane) const;


private:
	XMFLOAT4X4				_transformStateMatrices[ETOI(D3DTS::END)] = { };
	XMFLOAT4X4				_transformStateInverseMatrices[ETOI(D3DTS::END)] = { };


public:
	static Unique<PipeLine> Create();
	virtual void Free() override;

};

NS_END
