#pragma once

#include "Component.h"

NS_BEGIN(Engine)

class ENGINE_DLL Model final : public Component
{
public:
	Model(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Model(const Model& prototype);
	virtual ~Model() = default;

public:
	size_t Get_NumMeshes() {
		return _meshes.size();
	}

	int32 Get_BoneIndex(const char* boneName);
	const XMFLOAT4X4* Get_BoneMatrixPtr(const char* boneName);

	void Set_Animation(uint32 index, bool isLoop) {
		_currentAnimIndex = index;
		_isAnimLoop = isLoop;
	}

public:
	virtual HRESULT Initialize_Prototype(ModelType type, const char* modelFilePath, FXMMATRIX preLocalTransformMatrix);
	virtual HRESULT Initialize(void* arg);
	virtual HRESULT Render(uint32 meshIndex);

public:
	bool Play_Animation(float timeDelta);

public:
	HRESULT Bind_Material(class Shader* shader, const char* constantName, uint32 meshIndex, aiTextureType materialType, uint32 textureIndex = 0);
	HRESULT Bind_BoneMatrices(class Shader* shader, const char* constantName, uint32 meshIndex);

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
	HRESULT Ready_Materials(const char* modelFilePath);
	HRESULT Ready_Bones(const aiNode* aiNode, int32 parentIndex);
	HRESULT Ready_Animations();

public:
	static Shared<Model> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, ModelType type, const char* modelFilePath, FXMMATRIX preLocalTransformMatrix = XMMatrixIdentity());
	virtual Shared<Component> Clone(void* arg) override;
	virtual void Free() override;
};

NS_END



