#pragma once
#include "GameObject.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
NS_END

NS_BEGIN(ModelEditor)

class ModelViewerObject final : public GameObject
{
public:
	ModelViewerObject(const ComPtr<Device>& device, const ComPtr<Context>& context);
	ModelViewerObject(const ModelViewerObject& prototype);
	~ModelViewerObject() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void Priority_Update(float timeDelta) override;
	void Update(float timeDelta) override;
	void Late_Update(float timeDelta) override;
	HRESULT Render() override;

public:
    bool Is_Skeletal();
    bool Is_RootMotion();
	void Change_Model(const char* modelFilePath);
	void Play_Animation(uint32 animIndex, float blendDuration, bool isLoop, bool isRoot);
    uint32 Get_NumAnimations() const;
    const char* Get_AnimationName(uint32 animIndex);
    const char* Get_BoneName(uint32 boneIndex) const;
    uint32 Get_NumBones() const;
    void Set_RootNode(int32 boneIndex);

public:
    Shared<ModelCom> Get_ModelComponent() const { return _modelCom; }

private:
	HRESULT Ready_Components();
	HRESULT Bind_ShaderResources();

private:
    Shared<ShaderCom>                   _staticShaderCom{ nullptr };
    Shared<ShaderCom>                   _animShaderCom{ nullptr };
	Shared<ModelCom>                    _modelCom = nullptr;

public:
    static Shared<ModelViewerObject> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Shared<GameObject> Clone(void* arg) override;
	void Free() override;
	
};

NS_END
