#include "Weapon.h"
#include "GameInstance.h"
#include "Material_Types.h"
#include "Player_WorldBase.h"
#include "Client_Constants.h"
#include "AnimComputeRuntime.h" 
#include "ComputeShaderCom.h"   
#include "EffectCom.h"
#include "SwitchDissolveFeature.h"

IMPLEMENT_REFLECTION(Weapon)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Weapon";
    info.category = "Weapon";

    PROPERTY_STRING("Portrait", _portraitGuid);
    PROPERTY_WSTRING("기본 트레일 이펙트", _trailEffectName);

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

    _socketMatrix = (desc != nullptr) ? desc->socketMatrix : nullptr;
    _useBodyFollow = (desc != nullptr) ? desc->useBodyFollow : false;
    _bodyFollowOffset = (desc != nullptr) ? desc->bodyFollowOffset : Vec3::Zero;
    _applyAttachScaleCompensation = (desc != nullptr) ? desc->applyAttachScaleCompensation : false;
    _hoverTimeSec = 0.f;

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
    if (Is_LuneHoverWeapon())
        _hoverTimeSec += max(timeDelta, 0.f);

    const Matrix hoverMatrix = Is_LuneHoverWeapon()
        ? Matrix::CreateTranslation(0.f, Get_LuneHoverOffsetY(), 0.f)
        : Matrix::Identity;

    if (_useBodyFollow)
    {
        Matrix parentMatrix = (nullptr != _parentMatrix) ? *_parentMatrix : Matrix::Identity;
        Matrix bodyFollowMatrix = Matrix::CreateTranslation(_bodyFollowOffset) * parentMatrix;

        Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix() * bodyFollowMatrix * hoverMatrix);
        return;
    }

    if (_socketMatrix == nullptr)
    {
        Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix() * hoverMatrix);
        return;
    }

    Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix() * (*_socketMatrix) * hoverMatrix);
}

void Weapon::Late_Update(float timeDelta)
{
    if (_shader == nullptr || _model == nullptr)
        return;

    if (!Is_Visible())
        return;

    if (const Shared<GameObject> owner = Get_Owner())
    {
        if (!owner->Is_Visible())
            return;
    }

    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());

    if (Casts_DynamicShadow())
        GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
}

Matrix Weapon::Get_GizmoWorldMatrix() const
{
    if (_transformCom == nullptr)
        return _combinedWorldMatrix;

    const Matrix parentMatrix = (nullptr != _parentMatrix) ? *_parentMatrix : Matrix::Identity;
    const Matrix hoverMatrix = Is_LuneHoverWeapon()
        ? Matrix::CreateTranslation(0.f, Get_LuneHoverOffsetY(), 0.f)
        : Matrix::Identity;

    if (_useBodyFollow)
    {
        const Matrix bodyFollowMatrix = Matrix::CreateTranslation(_bodyFollowOffset) * parentMatrix;
        return _transformCom->Get_WorldMatrix() * bodyFollowMatrix * hoverMatrix;
    }

    const Matrix socketMatrix = (nullptr != _socketMatrix) ? *_socketMatrix : Matrix::Identity;
    return _transformCom->Get_WorldMatrix() * socketMatrix * parentMatrix * hoverMatrix;
}

bool Weapon::Apply_GizmoWorldMatrix(const Matrix& gizmoWorldMatrix)
{
    if (_transformCom == nullptr)
        return false;

    Matrix anchorMatrix = Matrix::Identity;
    const Matrix hoverMatrix = Is_LuneHoverWeapon()
        ? Matrix::CreateTranslation(0.f, Get_LuneHoverOffsetY(), 0.f)
        : Matrix::Identity;

    if (_useBodyFollow)
    {
        Matrix parentMatrix = (nullptr != _parentMatrix) ? *_parentMatrix : Matrix::Identity;
        anchorMatrix = Matrix::CreateTranslation(_bodyFollowOffset) * parentMatrix * hoverMatrix;
    }
    else
    {
        Matrix parentMatrix = (nullptr != _parentMatrix) ? *_parentMatrix : Matrix::Identity;
        Matrix socketMatrix = (nullptr != _socketMatrix) ? *_socketMatrix : Matrix::Identity;
        anchorMatrix = socketMatrix * parentMatrix * hoverMatrix;
    }

    Matrix localWorldMatrix = gizmoWorldMatrix * anchorMatrix.Invert();

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

    if (FAILED(EffectCom::Bind_DrawFeatures(Get_Owner(), _shader.get(), DrawFeatureFlagDissolve)))
        return E_FAIL; // owner EffectCom의 dissolve 상태를 무기에도 적용해 몸과 같이 사라지게 한다.

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

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());

    for (uint32 i = 0; i < numMeshes; ++i)
    {
        if (FAILED(_shader->Begin(1)))
            return E_FAIL;

        _model->Render(i);
    }

    return S_OK;
}

json Weapon::To_Json() const
{
    json data = PartObject::To_Json();
    json& customProperties = data["custom_properties"];

    customProperties["_sourceHistoryAnchors"] = json::array();
    for (const WeaponSourceHistoryAnchorDesc& anchor : _sourceHistoryAnchors)
    {
        customProperties["_sourceHistoryAnchors"].push_back(
            {
                { "name", anchor.name },
                { "boneName", anchor.boneName },
                { "localPosition", { anchor.localPosition.x, anchor.localPosition.y, anchor.localPosition.z } },
            }
        );
    }

    return data;
}

void Weapon::From_Json(const json& data)
{
    if (FAILED(Ensure_RuntimeComponentsFromJson(data)))
        return;

    PartObject::From_Json(data);

    _sourceHistoryAnchors.clear();
    if (data.contains("custom_properties") && data["custom_properties"].is_object())
    {
        const json& customProperties = data["custom_properties"];
        if (customProperties.contains("_sourceHistoryAnchors") && customProperties["_sourceHistoryAnchors"].is_array())
        {
            for (const json& anchorJson : customProperties["_sourceHistoryAnchors"])
            {
                if (!anchorJson.is_object())
                    continue;

                WeaponSourceHistoryAnchorDesc anchor{};
                anchor.name = anchorJson.value("name", string{});
                anchor.boneName = anchorJson.value("boneName", string{});

                if (anchorJson.contains("localPosition") &&
                    anchorJson["localPosition"].is_array() &&
                    anchorJson["localPosition"].size() >= 3)
                {
                    anchor.localPosition.x = anchorJson["localPosition"][0].get<float>();
                    anchor.localPosition.y = anchorJson["localPosition"][1].get<float>();
                    anchor.localPosition.z = anchorJson["localPosition"][2].get<float>();
                }

                if (!anchor.name.empty() && !anchor.boneName.empty())
                    _sourceHistoryAnchors.push_back(std::move(anchor));
            }
        }
    }

    if (_applyAttachScaleCompensation)
        Apply_AttachScale(); // Battle/ChangeWeapon 장착 runtime에서만 WP prefab authoring scale 0.1을 x10 보정한다.
}

const Matrix* Weapon::Get_SocketBoneMatrixPtr(const string& boneName) const
{
    if (_model == nullptr)
        return nullptr;

    return _model->Get_BoneMatrixPtr(boneName.c_str());
}

const WeaponSourceHistoryAnchorDesc* Weapon::Find_SourceHistoryAnchor(const string& anchorName) const
{
    for (const WeaponSourceHistoryAnchorDesc& anchor : _sourceHistoryAnchors)
    {
        if (anchor.name == anchorName)
            return &anchor;
    }

    return nullptr;
}

bool Weapon::Begin_NotifyRotation()
{
    if (_transformCom == nullptr)
        return false;

    if (_notifyRotationActive)
        return false;

    _notifyRotationBaseLocal = _transformCom->Get_RotationQuaternion();
    _notifyRotationActive = true;

    return true;
}

void Weapon::Apply_NotifyRotation(const Vec3 axisLocal, float degree)
{
    if (_transformCom == nullptr || !_notifyRotationActive)
        return;

    Vec3 normalizeAxis = axisLocal;
    if (normalizeAxis.LengthSquared() <= 0.00001f)
        return;

    normalizeAxis.Normalize();

    const Quat deltaRotation = Quat::CreateFromAxisAngle(normalizeAxis, XMConvertToRadians(degree));
    _transformCom->Set_RotationQuaternion(_notifyRotationBaseLocal * deltaRotation); // notify 시작 시점 무기 로컬축 기준으로 회전한다.
}

void Weapon::End_NotifyRotation()
{
    if (_transformCom == nullptr || !_notifyRotationActive)
        return;

    // 노티파이 시작 전 회전값으로 맞추기
    _transformCom->Set_RotationQuaternion(_notifyRotationBaseLocal);
    _notifyRotationBaseLocal = Quat::Identity;
    _notifyRotationActive = false;
}

bool Weapon::Is_LuneHoverWeapon() const
{
    const string& sourcePrefabName = Get_SourcePrefabName();
    return sourcePrefabName == Constants::Prefab::Lune_Colim ||
        sourcePrefabName == Constants::Prefab::Lune_Baguette ||
        sourcePrefabName == Constants::Prefab::Lune_Snowim;
}

float Weapon::Get_LuneHoverOffsetY() const
{
    return sinf(_hoverTimeSec * 1.8f) * 0.06f;
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
    defaultDesc.useBodyFollow = _useBodyFollow;
    defaultDesc.bodyFollowOffset = _bodyFollowOffset;
    defaultDesc.applyAttachScaleCompensation = _applyAttachScaleCompensation;

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

void Weapon::Apply_AttachScale()
{
    if (_transformCom == nullptr)
        return;

    const Vec3 scale = _transformCom->Get_Scale();
    _transformCom->Set_Scale(scale * 10.f);
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
