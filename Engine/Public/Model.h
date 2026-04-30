#pragma once

#include "Component.h"

NS_BEGIN(Engine)

class ENGINE_DLL Model final : public Component
{
public:
	Model(ComPtr<Device> device, ComPtr<Context> context);
	Model(const Model& Prototype);
	virtual ~Model() { Free(); }

public:
	size_t Get_NumMeshes() {
		return _meshes.size();
	}

	int32 Get_BoneIndex(const char* pBoneName);
	const XMFLOAT4X4* Get_BoneMatrixPtr(const char* pBoneName);

	void Set_Animation(uint32 iIndex, bool isLoop) {
		_currentAnimIndex = iIndex;
		_isAnimLoop = isLoop;
	}

public:
	virtual HRESULT Initialize_Prototype(ModelType eType, const char* pModelFilePath, FXMMATRIX PreLocalTransformMatrix);
	virtual HRESULT Initialize(void* pArg);
	virtual HRESULT Render(uint32 iMeshIndex);

public:
	bool Play_Animation(float fTimeDelta);

public:
	HRESULT Bind_Material(class Shader* pShader, const char* pConstantName, uint32 iMeshIndex, aiTextureType eMaterialType, uint32 iTextureIndex = 0);
	HRESULT Bind_BoneMatrices(class Shader* pShader, const char* pConstantName, uint32 iMeshIndex);

private:
	const aiScene*				_aiScene = { nullptr };
	Assimp::Importer			_importer = {};

	ModelType						_type = { ModelType::END };
	XMFLOAT4X4					_preLocalTransformMatrix = {};

private:
	uint32						_numMeshes = {};
	vector<Shared<class Mesh>>	_meshes;

	uint32						_numMaterials = {};
	vector<Shared<class Material>>	_materials;

	vector<Shared<class Bone>>		_bones;

	bool						_isAnimLoop = { false };
	uint32						_currentAnimIndex = {};
	uint32						_numAnimations = {};
	vector<Shared<class Animation>>	_animations;


private:
	HRESULT Ready_Meshes();
	HRESULT Ready_Materials(const char* pModelFilePath);
	HRESULT Ready_Bones(const aiNode* pAINode, int32 iParentIndex);
	HRESULT Ready_Animations();

public:
	static Shared<Model> Create(ComPtr<Device> device, ComPtr<Context> context, ModelType eType, const char* pModelFilePath, FXMMATRIX PreLocalTransformMatrix = XMMatrixIdentity());
	virtual Shared<Component> Clone(void* pArg) override;
	virtual void Free() override;
};

NS_END



