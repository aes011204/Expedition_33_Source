#pragma once

#include "VIBuffer.h"

NS_BEGIN(Engine)

class Mesh final : public VIBuffer
{
private:
	Mesh(ComPtr<Device> device, ComPtr<Context> context);
	Mesh(const Mesh& Prototype);
	virtual ~Mesh() = default;

public:
	uint32 Get_MaterialIndex() const {
		return _materialIndex;
	}

public:
	virtual HRESULT Initialize_Prototype(ModelType eType, const aiMesh* pAIMesh, class Model* pModel, FXMMATRIX PreTransformMatrix);
	virtual HRESULT Initialize(void* pArg) override;

public:
	HRESULT Bind_BoneMatrices(class Shader* pShader, const char* pConstantName, const vector<Shared<class Bone>>& Bones);

private:
	char					_name[MAX_PATH] = {};
	uint32					_materialIndex = {};
	uint32					_numBones = {};
	vector<uint32>			_boneIndices;
	XMFLOAT4X4				_boneMatrices[512] = {};
	vector<XMFLOAT4X4>		_offsetMatrices;



private:
	HRESULT Ready_VertexBuffer_For_NonAnim(const aiMesh* pAIMesh, FXMMATRIX PreTransformMatrix);
	HRESULT Ready_VertexBuffer_For_Anim(const aiMesh* pAIMesh, class Model* pModel);

public:
	static Shared<Mesh> Create(ComPtr<Device> device, ComPtr<Context> context, ModelType eType, const aiMesh* pAIMesh, class Model* pModel, FXMMATRIX PreTransformMatrix);
	virtual Shared<Component> Clone(void* pArg) override;
	virtual void Free() override;


};

NS_END