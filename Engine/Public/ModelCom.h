#pragma once

#include "Component.h"

NS_BEGIN(Engine)

class Mesh;
class Material;
class Bone;
class Animation;

class ENGINE_DLL ModelCom final : public Component
{
public:
	ModelCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
	ModelCom(const ModelCom& prototype);
	~ModelCom() override = default;

public:
    uint32 Get_MaxTextureType() const { return _maxTextureType; }
    size_t Get_NumMeshes() const { return _meshes.size(); }
    size_t Get_NumMaterials() const { return _materials.size(); }
    size_t Get_NumBones() const { return _bones.size(); }
    size_t Get_NumAnimations() const { return _animations.size(); }

public:
    bool Is_Skeletal() const { return _isSkeletal; } 
	int32 Get_BoneIndex(const char* boneName);
	const Matrix* Get_BoneMatrixPtr(const char* boneName);
    const Matrix* Get_BoneMatrixPtr(uint32 boneIndex) const;

public:
    void Set_Animation(uint32 animIndex, bool isLoop = false);
    void Set_Animation(uint32 animIndex, float blendDuration, bool isLoop = false);
    const char* Get_AnimationName(uint32 animationIndex) const;

public:
    void Set_RootMotionEnabled(bool enable) { _isRootMotion = enable; }
    void Set_LocalRootNode(int32 nodeIndex) { _rootNodeIndex = nodeIndex; _isRootMotion = true; }
    const RootMotionFrame& Get_RootTransformVelocity() const { return _rootFrameVelocity; }

public:
	HRESULT Initialize_Prototype(ModelType type, const char* modelFilePath, const Matrix& preLocalTransformMatrix);
	HRESULT Initialize(void* arg) override;
	HRESULT Render(uint32 meshIndex);

public:
	bool Play_Animation(float timeDelta);

public:
	HRESULT Bind_Material(class ShaderCom* shader, const char* constantName, uint32 meshIndex, uint32 materialType, uint32 textureIndex = 0);
	HRESULT Bind_BoneMatrices(class ShaderCom* shader, const char* constantName, uint32 meshIndex);

private:
	ModelType					        _type = { ModelType::END };
    bool                                _isSkeletal{ false };
	Matrix					            _preLocalTransformMatrix = {};

private:
    uint32                              _maxTextureType{};
	uint32						        _numMeshes{};
    uint32						        _numMaterials{};
    uint32                              _numBones{};
    uint32                              _numAnimations{};

    vector<Shared<Mesh>>	            _meshes;
	vector<Shared<Material>>	        _materials;
	vector<Shared<Bone>>		        _bones;
    vector<Shared<Animation>>	        _animations;

private: /* For Animation Control */
	bool						        _isAnimLoop{ false };       
    bool                                _isBlending{ false };
	uint32						        _currentAnimIndex{ 0 };
    uint32                              _nextAnimIndex{ 0 };
    float                               _blendDuration{ 0.f };
    float                               _blendElapsedTime{ 0.f };

private: /* For Root Motion */
    bool                                _isRootMotion{ false };
    int32                               _rootNodeIndex{ -1 }; // -1 일때는 최상위 노드 기준으로 
    RootMotionFrame                     _rootFrameVelocity{};

private:
	HRESULT Ready_Model(const char* modelPath);
    HRESULT Ready_Animation(const char* modelPath);

public:
	static Shared<ModelCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, ModelType type, const char* modelFilePath, const Matrix& preLocalTransformMatrix = XMMatrixIdentity());
	virtual Shared<Component> Clone(void* arg) override;
	virtual void Free() override;
};

NS_END



