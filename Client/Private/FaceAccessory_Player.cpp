#include "FaceAccessory_Player.h"
#include "GameInstance.h"
#include "Material.h"
#include "Material_Types.h"
#include "ModelCom.h"
#include "BattleTurnCamera.h"

namespace
{
    // 구스타브 수염 일단 여기서 만드는 거로... 
    HairCB Make_GustaveBeardCardHairCB()
    {
        HairCB cb{};
        cb.g_HairColorTop = { 0.286458f, 0.161008f, 0.088026f, 1.f };
        cb.g_HairColorDown = { 0.046875f, 0.025220f, 0.012695f, 1.f };
        cb.hairHighlightColor = { 0.177888f, 0.111932f, 0.072272f, 0.f };
        cb.opacityMaskClipValue = 0.3333f;
        cb.roughness = 0.65f;
        cb.specular = 0.3f;
        cb.brightness = 0.8f;
        cb.gradientColor = 0.614571f;
        cb.gradientContrast = 0.203904f;
        cb.gradientHardness = 1.0f;
        cb.gradientHeight = 0.462f;
        return cb;
    }
}

FaceAccessory_Player::FaceAccessory_Player(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : PartObject{ device, context }
{
    _hairCB = Make_GustaveBeardCardHairCB();
}

FaceAccessory_Player::FaceAccessory_Player(const FaceAccessory_Player& prototype)
    : PartObject{ prototype }
{
}

HRESULT FaceAccessory_Player::Initialize_Prototype()
{
    return S_OK;
}

HRESULT FaceAccessory_Player::Initialize(void* arg)
{
    FACE_ACCESSORY_DESC* desc = static_cast<FACE_ACCESSORY_DESC*>(arg);

    _socketMatrix = (desc != nullptr) ? desc->socketMatrixPointer : nullptr;

    CHECK_FAILED(PartObject::Initialize(arg), E_FAIL);

    if (desc != nullptr && desc->modelTag != nullptr)
    {
        CHECK_FAILED(Ready_Components(*desc), E_FAIL);
    }

    Set_ShadowCasterType(ShadowCasterType::Dynamic);
    return S_OK;
}

void FaceAccessory_Player::Priority_Update(float timeDelta)
{
}

void FaceAccessory_Player::Update(float timeDelta)
{
    if (_socketMatrix == nullptr)
    {
        Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());
        return;
    }

    Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix() * (*_socketMatrix));
}

void FaceAccessory_Player::Late_Update(float timeDelta)
{
    if (_shader == nullptr || _model == nullptr)
        return;

    GAME->Add_RenderGroup(RenderGroup::NonLight, GetSharedPtr<GameObject>());

    if (Casts_DynamicShadow())
        GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
}

HRESULT FaceAccessory_Player::Render()
{
    if (_shader == nullptr || _model == nullptr)
        return S_OK;

    CHECK_FAILED(Bind_ShaderResources(), E_FAIL); 
    CHECK_FAILED(_shader->Reset_DrawFeatures(), E_FAIL);

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());

    for (uint32 i = 0; i < numMeshes; ++i)
    {
        const uint32 materialIndex = _model->Get_MeshMaterialIndex(i);
        const Shared<Material> material = _model->Get_Material(materialIndex);
        if (material == nullptr)
            continue;

        if (FAILED(_shader->Bind_MaterialTextures(*material)))
            continue;

        CHECK_FAILED(_shader->Bind_CBufferData(material->Get_CB()), E_FAIL);
        CHECK_FAILED(_shader->Begin(material->Get_PassIndex()), E_FAIL);

        CHECK_FAILED(GAME->Bind_PlayerStencilWriteState(true), E_FAIL);

        _model->Render(i);
    }

    return S_OK;
}

HRESULT FaceAccessory_Player::Render_Shadow()
{
    if (_shader == nullptr || _model == nullptr)
        return S_OK;

    CHECK_FAILED(_shader->Bind_ObjectCB(_combinedWorldMatrix), E_FAIL);
    CHECK_FAILED(GAME->Bind_ShadowCB(_shader), E_FAIL);

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());

    for (uint32 i = 0; i < numMeshes; ++i)
    {
        CHECK_FAILED(_shader->Begin(1), E_FAIL);
        _model->Render(i);
    }

    return S_OK;
}

Matrix FaceAccessory_Player::Get_GizmoWorldMatrix() const
{
    return _combinedWorldMatrix;
}

bool FaceAccessory_Player::Apply_GizmoWorldMatrix(const Matrix& gizmoWorldMatrix)
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

HRESULT FaceAccessory_Player::Ready_Components(const FACE_ACCESSORY_DESC& desc)
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxMesh"), _shader), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), desc.modelTag, _model), E_FAIL);
    return S_OK;
}

HRESULT FaceAccessory_Player::Bind_ShaderResources()
{
    CHECK_NULL(_shader, E_FAIL);
    CHECK_NULL(_model, E_FAIL);

    CHECK_FAILED(_shader->Bind_ObjectCB(_combinedWorldMatrix), E_FAIL);
    CHECK_FAILED(GAME->Bind_CameraCB(_shader), E_FAIL);
    CHECK_FAILED(Bind_PlayerRenderCB(), E_FAIL);
    CHECK_FAILED(_shader->Bind_CBufferData(_hairCB), E_FAIL);
    return S_OK;
}

HRESULT FaceAccessory_Player::Bind_PlayerRenderCB()
{
    CHECK_NULL(_shader, E_FAIL);

    PlayerRenderCB playerRenderCB{};

    if (const auto battleCamera =
        dynamic_pointer_cast<BattleTurnCamera>(GAME->Get_CameraByType(ObjectType::BattleTurnCamera)))
    {
        if (battleCamera->Should_ClipPlayerNearCamera())
        {
            playerRenderCB.nearClipParams.x = 1.f;
            playerRenderCB.nearClipParams.y = battleCamera->Get_PlayerNearClipDistance();
        }
    }

    return _shader->Bind_CBufferData(playerRenderCB);
}

Shared<FaceAccessory_Player> FaceAccessory_Player::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<FaceAccessory_Player>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : FaceAccessory_Player");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> FaceAccessory_Player::Clone(void* arg)
{
    auto clone = make_shared<FaceAccessory_Player>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : FaceAccessory_Player");
        return nullptr;
    }

    return clone;
}

void FaceAccessory_Player::Free()
{
    __super::Free();
}
