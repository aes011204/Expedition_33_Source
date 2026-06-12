#include "pch.h"
#include "ModelViewerObject.h"
#include "GameInstance.h"
#include "Material.h"
#include "ShaderCom.h"

NS_BEGIN(ModelEditor)

ModelViewerObject::ModelViewerObject(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: GameObject(device, context) {}

ModelViewerObject::ModelViewerObject(const ModelViewerObject& prototype)
	: GameObject(prototype) {}

HRESULT ModelViewerObject::Initialize_Prototype()
{
	return GameObject::Initialize_Prototype();
}

HRESULT ModelViewerObject::Initialize(void* arg)
{
	if (FAILED(GameObject::Initialize(arg)))
		return E_FAIL;

	if (FAILED(Ready_Components()))
		return E_FAIL;

    return S_OK;
}

void ModelViewerObject::Priority_Update(float timeDelta)
{
	GameObject::Priority_Update(timeDelta);
}

void ModelViewerObject::Update(float timeDelta)
{
	GameObject::Update(timeDelta);

	if (_modelCom && _modelCom->Is_Skeletal())
	{
		_modelCom->Play_Animation(timeDelta);
	}
}

void ModelViewerObject::Late_Update(float timeDelta)
{
	GameObject::Late_Update(timeDelta);

	GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
}

HRESULT ModelViewerObject::Render()
{
	if (nullptr == _modelCom || nullptr == _staticShaderCom || nullptr == _animShaderCom)
		return S_OK;

	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

    const Shared<ShaderCom> shader = _modelCom->Is_Skeletal() ? _animShaderCom : _staticShaderCom;
    if (FAILED(shader->Reset_DrawFeatures()))
        return E_FAIL;

	uint32 numMeshes = static_cast<uint32>(_modelCom->Get_NumMeshes());
    for (uint32 i = 0; i < numMeshes; ++i)
    {
        const uint32 materialIndex = _modelCom->Get_MeshMaterialIndex(i);
        const Shared<Material> material = _modelCom->Get_Material(materialIndex);
        if (material == nullptr)
        {
            continue;
        }

        if (FAILED(shader->Bind_MaterialTextures(*material)))
        {
            continue;
        }

        if (FAILED(shader->Bind_CBufferData(material->Get_CB())))
        {
            return E_FAIL;
        }

        if (_modelCom->Is_Skeletal())
        {
            if (FAILED(_modelCom->Bind_BoneMatrices(shader.get(), "g_BoneMatricesBuffer", i)))
            {
                return E_FAIL;
            }
        }

        const uint32 passIndex = material->Get_PassIndex();
        if (FAILED(shader->Begin(passIndex)))
        {
            return E_FAIL;
        }

        _modelCom->Render(i);
    }
	

	return S_OK;
}

bool ModelViewerObject::Is_Skeletal()
{
    return _modelCom->Is_Skeletal();
}

bool ModelViewerObject::Is_RootMotion()
{
    return false;
}

void ModelViewerObject::Change_Model(const char* modelFilePath)
{
	if (_modelCom)
	{
		_components.erase(TEXT("Com_Model"));
		_modelCom.reset();
	}

    Shared<ModelCom> newModelCom = ModelCom::Create(_device, _context, ModelType::Anim, modelFilePath);

    if (newModelCom) {
        _modelCom = newModelCom;
        _components.emplace(TEXT("Com_Model"), _modelCom);

        LOG_INFO("Successfully created and attached new ModelCom: {}", modelFilePath);
    }
    else {
        LOG_ERROR("Failed to create ModelCom from: {}", modelFilePath);
    }
}

void ModelViewerObject::Play_Animation(uint32 animIndex, float blendDuration, bool isLoop, bool isRoot)
{
    _modelCom->Set_RootMotionEnabled(isRoot);

	if (_modelCom && _modelCom->Is_Skeletal())
	{
		_modelCom->Set_Animation(animIndex, blendDuration, isLoop);
	}
}

uint32 ModelViewerObject::Get_NumAnimations() const
{
    return static_cast<uint32>(_modelCom->Get_NumAnimations());
}

const char* ModelViewerObject::Get_AnimationName(uint32 animIndex)
{
    return _modelCom->Get_AnimationName(animIndex);
}

const char* ModelViewerObject::Get_BoneName(uint32 boneIndex) const
{
    return _modelCom->Get_BoneName(boneIndex).c_str();
}

uint32 ModelViewerObject::Get_NumBones() const
{
    return (uint32)_modelCom->Get_NumBones();
}

void ModelViewerObject::Set_RootNode(int32 boneIndex)
{
    _modelCom->Set_LocalRootNode(boneIndex);
}

HRESULT ModelViewerObject::Ready_Components()
{
    if (FAILED(GameObject::Add_Component(0, TEXT("Shader_VtxMesh"), _staticShaderCom)))
        return E_FAIL;

    if (FAILED(GameObject::Add_Component(0, TEXT("Shader_VtxAnimMesh"), _animShaderCom)))
        return E_FAIL;

    return S_OK;
}

HRESULT ModelViewerObject::Bind_ShaderResources()
{
    if (nullptr == _modelCom) return E_FAIL;

    auto target = _modelCom->Is_Skeletal() ? _animShaderCom : _staticShaderCom ;

	if (FAILED(target->Bind_ObjectCB(_transformCom->Get_WorldMatrix())))
		return E_FAIL;

    if (FAILED(GAME->Bind_CameraCB(target)))
        return E_FAIL;

	return S_OK;
}

Shared<ModelViewerObject> ModelViewerObject::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto instance = make_shared<ModelViewerObject>(device, context);

	if (FAILED(instance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : ModelViewerObject");
		return nullptr;
	}

	return instance;
}

Shared<GameObject> ModelViewerObject::Clone(void* arg)
{
	auto instance = make_shared<ModelViewerObject>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : ModelViewerObject");
		return nullptr;
	}

	return instance;
}

void ModelViewerObject::Free()
{
    GameObject::Free();
}

NS_END
