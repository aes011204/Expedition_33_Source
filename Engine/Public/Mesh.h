#pragma once

#include "VIBuffer.h"

NS_BEGIN(Engine)

class Mesh final : public VIBuffer
{
public:
	Mesh(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Mesh(const Mesh& prototype);
	~Mesh() override = default;

public:
	uint32 Get_MaterialIndex() const { return _materialIndex; }

public:
	virtual HRESULT Initialize_Prototype(ModelType type, const aiMesh* aiMesh, class Model* model, FXMMATRIX preTransformMatrix);
	virtual HRESULT Initialize(void* arg) override;

public:
	HRESULT Bind_BoneMatrices(class Shader* shader, const char* constantName, const vector<Shared<class Bone>>& bones);

private:
	char					_name[MAX_PATH] = {};
	uint32					_materialIndex = {};
	uint32					_numBones = {};
	vector<uint32>			_boneIndices;
	XMFLOAT4X4				_boneMatrices[maxBoneCount] = {};
	vector<XMFLOAT4X4>		_offsetMatrices;

private:
	HRESULT Ready_VertexBuffer_For_NonAnim(const aiMesh* aiMesh, FXMMATRIX preTransformMatrix);
	HRESULT Ready_VertexBuffer_For_Anim(const aiMesh* aiMesh, class Model* model);

public:
	static Shared<Mesh> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, ModelType type, const aiMesh* aiMesh, class Model* model, FXMMATRIX preTransformMatrix);
	virtual Shared<Component> Clone(void* arg) override;
	virtual void Free() override;


};

NS_END
