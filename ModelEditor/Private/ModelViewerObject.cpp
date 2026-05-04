#include "pch.h"
#include "ModelViewerObject.h"
#include "GameInstance.h"

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

	uint32 numMeshes = _modelCom->Get_NumMeshes();
    if (_modelCom->Is_Skeletal())
    {
        for (uint32 i = 0; i < numMeshes; ++i)
        {
            _modelCom->Bind_Material(_animShaderCom.get(), "g_DiffuseTexture", i, aiTextureType_DIFFUSE);

            if (_modelCom->Is_Skeletal())
            {
                _modelCom->Bind_BoneMatrices(_animShaderCom.get(), "g_BoneMatrices", i);
            }

            _animShaderCom->Begin(0);
            _modelCom->Render(i);
        }
    }
    else
    {
        for (uint32 i = 0; i < numMeshes; ++i)
        {
            _modelCom->Bind_Material(_staticShaderCom.get(), "g_DiffuseTexture", i, aiTextureType_DIFFUSE);

            if (_modelCom->Is_Skeletal())
            {
                _modelCom->Bind_BoneMatrices(_staticShaderCom.get(), "g_BoneMatrices", i);
            }

            _staticShaderCom->Begin(0);
            _modelCom->Render(i);
        }
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


}

void ModelViewerObject::Play_Animation(uint32 animIndex, float blendDuration, bool isLoop, bool isRoot)
{
    _modelCom->Set_RootMotionEnabled(isRoot);

	if (_modelCom && _modelCom->Is_Skeletal())
	{
		_modelCom->Set_Animation(animIndex, blendDuration, isLoop);
	}
}

HRESULT ModelViewerObject::Ready_Components()
{
	return S_OK;
}

HRESULT ModelViewerObject::Bind_ShaderResources()
{
    if (nullptr == _modelCom) return E_FAIL;

    auto target = _modelCom->Is_Skeletal() ? _animShaderCom : _staticShaderCom ;

	if (FAILED(_transformCom->Bind_ShaderResource(target.get(), "g_WorldMatrix")))
		return E_FAIL;

	if (FAILED(target->Bind_Matrix("g_ViewMatrix", GAME->Get_Transform(D3DTS::View))))
		return E_FAIL;
    if (FAILED(target->Bind_Matrix("g_ProjMatrix", GAME->Get_Transform(D3DTS::Proj))))
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
