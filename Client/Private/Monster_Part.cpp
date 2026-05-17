#include "Monster_Part.h"

#include "GameInstance.h"
#include "Material_Types.h"
#include "ModelCom.h"
#include "ShaderCom.h"

NS_BEGIN(Client)

Monster_Part::Monster_Part(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : PartObject(device, context)
{
}

Monster_Part::Monster_Part(const Monster_Part& prototype)
    : PartObject(prototype)
{
}

HRESULT Monster_Part::Initialize_Prototype()
{
    return PartObject::Initialize_Prototype();
}

HRESULT Monster_Part::Initialize(void* arg)
{
    if (FAILED(PartObject::Initialize(arg)))
        return E_FAIL;

    auto* desc = static_cast<MONSTER_PART_DESC*>(arg);
    if (desc == nullptr)
        return E_FAIL;



    return Ready_Components(*desc);
}

void Monster_Part::Priority_Update(float timeDelta)
{
}

void Monster_Part::Update(float timeDelta)
{
    Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());
}

void Monster_Part::Late_Update(float timeDelta)
{
    // culling
    if (!Get_Owner() || !Get_Owner()->Is_Visible())
    {
        return;
    }

    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
    GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
}

HRESULT Monster_Part::Render()
{
    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    const uint32 iNumMeshes = static_cast<uint32>(_model->Get_NumMeshes());

    for (uint32 i = 0; i < iNumMeshes; i++)
    {
        const uint32 materialIndex = _model->Get_MeshMaterialIndex(i);
        const Shared<Material> material = _model->Get_Material(materialIndex);
        if (material == nullptr)
            continue;

        if (FAILED(_shader->Bind_MaterialTextures(*material)))
            continue;

        if (FAILED(_shader->Bind_CBufferData(material->Get_CB())))
            return E_FAIL;

        if (_model->Get_NumAnimations() != 0)
            CHECK_FAILED_ONCE(_model->Bind_BoneMatrices(_shader.get(), "g_BoneMatricesBuffer", i), E_FAIL);

        const uint32 passIndex = Decide_PassIndex(material);
        if (FAILED(_shader->Begin(passIndex)))
            return E_FAIL;

        _model->Render(i);
    }

    return S_OK;
}

HRESULT Monster_Part::Render_Shadow()
{
    if (FAILED(_shader->Bind_ObjectCB(_combinedWorldMatrix)))
        return E_FAIL;

    if (FAILED(GAME->Bind_ShadowCB(_shader)))
        return E_FAIL;

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());

    for (uint32 i = 0; i < numMeshes; ++i)
    {
        if (_model->Is_Skeletal())
            CHECK_FAILED(_model->Bind_BoneMatrices(_shader.get(), "g_BoneMatricesBuffer", i), E_FAIL);

        const uint32 shadowPass =
            _model->Is_Skeletal() ? static_cast<uint32>(VtxAnimMeshPass::Shadow) : 1u;
        
        if (FAILED(_shader->Begin(shadowPass)))
            return E_FAIL;

        _model->Render(i);
    }

    return S_OK;
}

void Monster_Part::Set_Animation(uint32 animIndex, float blendDuration, bool isLoop)
{
    if (_model == nullptr)
        return;

    _model->Set_Animation(animIndex, blendDuration, isLoop);
}

bool Monster_Part::Play_Animation(float timeDelta)
{
    if (_model == nullptr)
        return false;

    return _model->Play_Animation(timeDelta);
}

const RootMotionFrame& Monster_Part::Get_RootTransformVelocity() const
{
    return _model->Get_RootTransformVelocity();
}

const Matrix* Monster_Part::Get_SocketBoneMatrixPtr(const string& boneName) const
{
    return _model ? _model->Get_BoneMatrixPtr(boneName.c_str()) : nullptr;
}

const Matrix* Monster_Part::Get_SocketBoneMatrixPtr(uint32 boneIndex) const
{
    return _model ? _model->Get_BoneMatrixPtr(boneIndex) : nullptr;
}

HRESULT Monster_Part::Ready_Components(const MONSTER_PART_DESC& desc)
{
    if (desc.modelType == ModelType::Anim)
    {
        if (FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxAnimMesh"), _shader)))
            return E_FAIL;
    }
    else
    {
        if (FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxMesh"), _shader)))
            return E_FAIL;
    }

    if (FAILED(Add_Component(ETOI(LevelType::GamePlay), desc.modelTag, _model)))
        return E_FAIL;

    return S_OK;
}

HRESULT Monster_Part::Bind_ShaderResources()
{
    if (FAILED(_shader->Bind_ObjectCB(_combinedWorldMatrix)))
        return E_FAIL;

    if (FAILED(GAME->Bind_CameraCB(_shader)))
        return E_FAIL;

    return S_OK;
}

uint32 Monster_Part::Decide_PassIndex(Shared<Material> mat)
{
    return mat->Get_PassIndex();
}

void Monster_Part::Free()
{
    PartObject::Free();
}

NS_END
