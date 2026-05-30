#include "Weapon.h"
#include "GameInstance.h"
#include "Material_Types.h"
#include "Player_WorldBase.h"
#include "AnimComputeRuntime.h" 
#include "ComputeShaderCom.h"   

IMPLEMENT_REFLECTION(Weapon)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Weapon";
    info.category = "Weapon";

    return true;
}

Weapon::Weapon(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : PartObject{ device, context }
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

    // body socketMatrix 사용
    _socketMatrix = (desc != nullptr) ? desc->socketMatrix : nullptr;

    CHECK_FAILED(PartObject::Initialize(arg), E_FAIL);

    if (desc != nullptr && desc->modelTag != nullptr)
    {
        CHECK_FAILED(Ready_Components(*desc), E_FAIL);
    }
    else
    {
        CHECK_FAILED(Ready_DefaultComponents(), E_FAIL);
    }

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
    if (_shader == nullptr || _model == nullptr)
        return;

    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());

    if (Casts_DynamicShadow())
        GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
}

Matrix Weapon::Get_GizmoWorldMatrix() const
{
    if (_transformCom == nullptr)
        return _combinedWorldMatrix;

    const Matrix parentMatrix = (nullptr != _parentMatrix) ? *_parentMatrix : Matrix::Identity;
    const Matrix socketMatrix = (nullptr != _socketMatrix) ? *_socketMatrix : Matrix::Identity;
    return _transformCom->Get_WorldMatrix() * socketMatrix * parentMatrix;
}

bool Weapon::Apply_GizmoWorldMatrix(const Matrix& gizmoWorldMatrix)
{
    if (_transformCom == nullptr)
        return false;

    Matrix parentMatrix = (nullptr != _parentMatrix) ? *_parentMatrix : Matrix::Identity;
    Matrix socketMatrix = (nullptr != _socketMatrix) ? *_socketMatrix : Matrix::Identity;
    Matrix localWorldMatrix = gizmoWorldMatrix * (socketMatrix * parentMatrix).Invert();

    Vec3 scale = Vec3::One;
    Quat rotation = Quat::Identity;
    Vec3 translation = Vec3::Zero;

    if (!localWorldMatrix.Decompose(scale, rotation, translation))
        return false;

    _transformCom->Set_WorldPosition(translation);
    _transformCom->Set_RotationQuaternion(rotation);
    _transformCom->Set_Scale(scale);
    return true;
}

HRESULT Weapon::Render()
{
    if (_shader == nullptr || _model == nullptr)
        return S_OK;

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

        CHECK_FAILED(GAME->Bind_PlayerStencilWriteState(true), E_FAIL);

        _model->Render(i);
    }

    return S_OK;
}

HRESULT Weapon::Render_Shadow()
{
    if (_shader == nullptr || _model == nullptr)
        return S_OK;

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

void Weapon::From_Json(const json& data)
{
    if (FAILED(Ensure_RuntimeComponentsFromJson(data)))
        return;

    PartObject::From_Json(data);
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

HRESULT Weapon::Ready_DefaultComponents()
{
    WEAPON_DESC defaultDesc{};
    defaultDesc.modelType = ModelType::NonAnim;
    defaultDesc.modelTag = L"Model_Lanceram";
    defaultDesc.socketMatrix = _socketMatrix;

    return Ready_Components(defaultDesc);
}

HRESULT Weapon::Bind_ShaderResources()
{
    if (_shader == nullptr || _model == nullptr)
        return E_FAIL;

    if (FAILED(_shader->Bind_ObjectCB(_combinedWorldMatrix)))
        return E_FAIL;

    if (FAILED(GAME->Bind_CameraCB(_shader)))
        return E_FAIL;

    return S_OK;
}

HRESULT Weapon::Ensure_RuntimeComponentsFromJson(const json& data)
{
    if (!data.contains("components") || !data["components"].is_array())
        return S_OK;

    auto findComponentTag =
        [this](const Shared<Component>& targetComponent) -> wstring
        {
            if (targetComponent == nullptr)
                return {};

            wstring foundTag{};
            Visit_Components(
                [&foundTag, &targetComponent](const wstring& tag, const Shared<Component>& component)
                {
                    if (foundTag.empty() && component == targetComponent)
                        foundTag = tag;
                });

            return foundTag;
        };

    for (const json& componentJson : data["components"])
    {
        if (!componentJson.is_object())
            continue;

        const string prototypeTagString = componentJson.value("prototype_tag", string{});
        if (prototypeTagString.empty())
            continue;

        const wstring prototypeTag = String::ToWString(prototypeTagString);

        if (prototypeTag == L"Shader_VtxMesh")
        {
            if (_components.find(prototypeTag) != _components.end())
                continue;

            if (_shader != nullptr)
            {
                const wstring currentShaderTag = findComponentTag(static_pointer_cast<Component>(_shader));
                if (!currentShaderTag.empty())
                    CHECK_FAILED(Rename_ComponentTag(currentShaderTag, prototypeTag), E_FAIL);
            }
            else
            {
                CHECK_FAILED(Add_Component(ETOI(LevelType::Static), prototypeTag.c_str(), _shader), E_FAIL);
            }

            continue;
        }

        if (componentJson.contains("model_type"))
        {
            if (_components.find(prototypeTag) != _components.end())
                continue;

            if (_model != nullptr)
            {
                const wstring currentModelTag = findComponentTag(static_pointer_cast<Component>(_model));
                if (!currentModelTag.empty())
                    CHECK_FAILED(Rename_ComponentTag(currentModelTag, prototypeTag), E_FAIL);
            }
            else
            {
                CHECK_FAILED(Add_Component(ETOI(LevelType::Static), prototypeTag.c_str(), _model), E_FAIL);
            }

            continue;
        }
    }

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


