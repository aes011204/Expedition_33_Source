#include "pch.h"
#include "ModelViewerObject.h"
#include "GameInstance.h"
#include "Material.h"
#include "ShaderCom.h"
#include "ComputeShaderCom.h"
#include "AnimComputeRuntime.h"
#include "ShaderPassFinder.h"

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

	if (_modelCom && _modelCom->Is_Skeletal() && _animComputeRuntime != nullptr)
	{
		CHECK_FAILED(_animComputeRuntime->Update_Frame(timeDelta), );
	}
	else if (_modelCom && _modelCom->Is_Skeletal())
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
    _lastRenderComputeSkinned = false;

	if (nullptr == _modelCom || nullptr == _staticShaderCom || nullptr == _animShaderCom)
		return S_OK;

    if (_modelCom->Is_Skeletal() && _animComputeRuntime != nullptr)
        return Render_ComputeSkinned();

    return Render_Default();
}

HRESULT ModelViewerObject::Render_Default()
{
    const Shared<ShaderCom> shader = _modelCom->Is_Skeletal() ? _animShaderCom : _staticShaderCom;
    CHECK_NULL(shader, E_FAIL);
    
    CHECK_FAILED(Bind_ShaderResources(shader), E_FAIL);
    CHECK_FAILED(shader->Reset_DrawFeatures(), E_FAIL);

	const uint32 numMeshes = static_cast<uint32>(_modelCom->Get_NumMeshes());
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

HRESULT ModelViewerObject::Render_ComputeSkinned()
{
    CHECK_NULL(_animComputeRuntime, E_FAIL);
    CHECK_NULL(_staticShaderCom, E_FAIL);

    _lastRenderComputeSkinned = true;

    CHECK_FAILED(_animComputeRuntime->Prepare_ComputeSkinning(), E_FAIL);
    CHECK_FAILED(Bind_ShaderResources(_staticShaderCom), E_FAIL);
    CHECK_FAILED(_staticShaderCom->Reset_DrawFeatures(), E_FAIL);

    const uint32 numMeshes = static_cast<uint32>(_modelCom->Get_NumMeshes());
    for (uint32 i = 0; i < numMeshes; ++i)
    {
        const uint32 materialIndex = _modelCom->Get_MeshMaterialIndex(i);
        const Shared<Material> material = _modelCom->Get_Material(materialIndex);
        if (material == nullptr)
            continue;

        if (FAILED(_staticShaderCom->Bind_MaterialTextures(*material)))
            continue;

        if (FAILED(_staticShaderCom->Bind_CBufferData(material->Get_CB())))
            return E_FAIL;

        const uint32 passIndex =
            ShaderPassFinder::Find_PassIndex(ShaderType::VtxMesh, material->Get_RenderState());
        if (FAILED(_staticShaderCom->Begin(passIndex)))
            return E_FAIL;

        CHECK_FAILED(_animComputeRuntime->Render_ComputeSkinned(i), E_FAIL);
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
    _animComputeRuntime.reset();

    Shared<ModelCom> newModelCom = ModelCom::Create(_device, _context, ModelType::Anim, modelFilePath);

    if (newModelCom) {
        _modelCom = newModelCom;
        _components.emplace(TEXT("Com_Model"), _modelCom);
        _modelCom->Apply_BaseMaterialFromBundle_AllSlots();

        LOG_INFO("Successfully created and attached new ModelCom: {}", modelFilePath);

        if (FAILED(Ready_AnimComputeRuntime()))
            LOG_WARN("ModelViewerObject: compute skinning preview disabled for {}", modelFilePath);
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

    if (FAILED(GameObject::Add_Component(0, TEXT("ComputeShader_BoneRemap"), _boneRemapShaderCom)))
        return E_FAIL;

    if (FAILED(GameObject::Add_Component(0, TEXT("ComputeShader_AnimMeshSkinning"), _skinningShaderCom)))
        return E_FAIL;

    if (FAILED(GameObject::Add_Component(0, TEXT("ComputeShader_MorphDeform"), _morphShaderCom)))
        return E_FAIL;

    return S_OK;
}

HRESULT ModelViewerObject::Ready_AnimComputeRuntime()
{
    if (_modelCom == nullptr || !_modelCom->Is_Skeletal())
        return S_OK;

    AnimComputeRuntime::CreateDesc runtimeDesc{};
    runtimeDesc.model = _modelCom.get();
    runtimeDesc.boneRemapShader = _boneRemapShaderCom;
    runtimeDesc.skinningShader = _skinningShaderCom;
    runtimeDesc.morphShader = _morphShaderCom;

    _animComputeRuntime = AnimComputeRuntime::Create(_device, _context, runtimeDesc);
    CHECK_NULL(_animComputeRuntime, E_FAIL);

    return S_OK;
}

HRESULT ModelViewerObject::Bind_ShaderResources(const Shared<ShaderCom>& shader)
{
    if (nullptr == _modelCom || nullptr == shader) return E_FAIL;

	if (FAILED(shader->Bind_ObjectCB(_transformCom->Get_WorldMatrix())))
		return E_FAIL;

    if (FAILED(GAME->Bind_CameraCB(shader)))
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
