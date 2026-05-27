#include "Weapon.h"
#include "GameInstance.h"
#include "Material_Types.h"
#include "Player_WorldBase.h"
#include "AnimComputeRuntime.h" 
#include "ComputeShaderCom.h"   

Weapon::Weapon(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: PartObject { device, context }
{
}

Weapon::Weapon(const Weapon& prototype)
	: PartObject{ prototype }
{
}

HRESULT Weapon::Initialize_Prototype()
{
	return S_OK;
}

HRESULT Weapon::Initialize(void* arg)
{
	auto* desc = static_cast<WEAPON_DESC*>(arg);
    if (desc == nullptr)
        return E_FAIL;

    // body socketMatrix 사용
    _socketMatrix = desc->socketMatrix;

    CHECK_FAILED(PartObject::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_Components(*desc), E_FAIL);

    Set_ShadowCasterType(ShadowCasterType::Dynamic);

	return S_OK;
}

void Weapon::Priority_Update(float timeDelta)
{
}

void Weapon::Update(float timeDelta)
{
    if (_socketMatrix == nullptr)
    {
        Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());
        return;
    }

        
	Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix() * (*_socketMatrix));
}

void Weapon::Late_Update(float timeDelta)
{
	GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());

    if (Casts_DynamicShadow())
	    GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
}

HRESULT Weapon::Render()
{
    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    if (FAILED(_shader->Reset_DrawFeatures()))
        return E_FAIL;

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());

    for (uint32 i = 0; i < numMeshes; ++i)
    {
        const uint32 materialIndex = _model->Get_MeshMaterialIndex(i);
        const Shared<Material> material = _model->Get_Material(materialIndex);
        if (material == nullptr)
            continue;

        if (FAILED(_shader->Bind_MaterialTextures(*material)))
            continue;

        if (FAILED(_shader->Bind_CBufferData(material->Get_CB())))
            return E_FAIL;

        if (FAILED(_shader->Begin(material->Get_PassIndex())))
            return E_FAIL;

        // 플레이어 무기 실루엣도 Background UI를 가리는 영역으로 기록한다.
        CHECK_FAILED(GAME->Bind_PlayerStencilWriteState(true), E_FAIL);

        _model->Render(i);
    }

    return S_OK;
}

HRESULT Weapon::Render_Shadow()
{
    if (FAILED(_shader->Bind_ObjectCB(_combinedWorldMatrix)))
        return E_FAIL;

    if (FAILED(GAME->Bind_ShadowCB(_shader)))
        return E_FAIL;

    if (FAILED(_shader->Reset_DrawFeatures()))
        return E_FAIL;
        
    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());

    for (uint32 i = 0; i < numMeshes; ++i)
    {
        if (FAILED(_shader->Begin(1)))
            return E_FAIL;

        _model->Render(i);
    }

    return S_OK;
}

const Matrix* Weapon::Get_SocketBoneMatrixPtr(const string& boneName) const
{
    if (_model == nullptr)
        return nullptr;

    return _model->Get_BoneMatrixPtr(boneName.c_str());
}

HRESULT Weapon::Ready_Components(const WEAPON_DESC& desc)
{
	CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxMesh"), _shader), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), desc.modelTag, _model), E_FAIL);

    return S_OK;
}

HRESULT Weapon::Bind_ShaderResources()
{
	if (FAILED(_shader->Bind_ObjectCB(_combinedWorldMatrix)))
		return E_FAIL;

	if (FAILED(GAME->Bind_CameraCB(_shader)))
        return E_FAIL;

	return S_OK;
}

Shared<Weapon> Weapon::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto pInstance = make_shared<Weapon>(device, context);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		LOG_ERROR("Failed to Created : Weapon");
		return nullptr;
	}
	return pInstance;
}


Shared<GameObject> Weapon::Clone(void* pArg)
{
	auto pInstance = make_shared<Weapon>(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : Weapon");
		return nullptr;
	}
	return pInstance;
}


void Weapon::Free()
{
	__super::Free();

}


