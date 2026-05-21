#include "pch.h"
#include "TrailPreviewWeaponObject.h"

#include "GameInstance.h"
#include "Material.h"
#include "ModelCom.h"
#include "PB_Gustave.h"
#include "PlayableParts.h"
#include "ShaderPassFinder.h"
#include "ShaderCom.h"

NS_BEGIN(EffectEditor)

TrailPreviewWeaponObject::TrailPreviewWeaponObject(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

TrailPreviewWeaponObject::TrailPreviewWeaponObject(const TrailPreviewWeaponObject& prototype)
    : GameObject{ prototype }
{
}

HRESULT TrailPreviewWeaponObject::Initialize(void* arg)
{
    const PreviewDesc* previewDesc = static_cast<PreviewDesc*>(arg);
    CHECK_NULL(previewDesc, E_FAIL);
    CHECK_NULL(previewDesc->gustave, E_FAIL);

    _gustave = previewDesc->gustave;

    CHECK_FAILED(__super::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    Set_Name(L"Lanceram_TrailPreview_Weapon");
    Set_Visible(false);

    return S_OK;
}

void TrailPreviewWeaponObject::Update(float)
{
    if (!Is_Visible())
    {
        _isSocketAttached = false;
        _weaponWorldMatrix = Matrix::Identity;
        _trailBaseWorldPosition = {};
        _trailTipWorldPosition = {};
        return;
    }

    Update_WeaponWorldMatrix();
}

void TrailPreviewWeaponObject::Late_Update(float)
{
    if (Is_Visible() && nullptr != _weaponModel && nullptr != _weaponShader)
        GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
}

HRESULT TrailPreviewWeaponObject::Render()
{
    CHECK_FAILED(_weaponShader->Bind_ObjectCB(_weaponWorldMatrix), E_FAIL);
    CHECK_FAILED(GAME->Bind_CameraCB(_weaponShader), E_FAIL);

    const uint32 numMeshes = static_cast<uint32>(_weaponModel->Get_NumMeshes());
    for (uint32 meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
    {
        if (FAILED(Bind_ShaderResources(meshIndex)))
            continue;

        CHECK_FAILED(_weaponModel->Render(meshIndex), E_FAIL);
    }

    return S_OK;
}

wstring TrailPreviewWeaponObject::Resolve_ClientResourcePath(const wchar_t* relativePath) const
{
    wchar_t modulePath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0)
        return {};

    const fs::path runtimeRoot = fs::path(modulePath).parent_path();
    return fs::absolute(runtimeRoot / relativePath)
           .lexically_normal()
           .wstring();
}

HRESULT TrailPreviewWeaponObject::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), L"Shader_VtxMesh", _weaponShader), E_FAIL);
    CHECK_FAILED(Ready_WeaponModel(), E_FAIL);

    return S_OK;
}

HRESULT TrailPreviewWeaponObject::Ready_WeaponModel()
{
    const wstring weaponPath = Resolve_ClientResourcePath(kWeaponModelRelativePath);
    if (weaponPath.empty())
        return E_FAIL;

    const string weaponPathUtf8 = String::ToString(weaponPath);
    _weaponModel = ModelCom::Create(_device, _context, ModelType::NonAnim, weaponPathUtf8.c_str());
    CHECK_NULL(_weaponModel, E_FAIL);
    Configure_WeaponMaterials();

    LOG_INFO(
        "[TrailPreview] Lanceram weapon loaded. path={}, meshes={}, materials={}",
        weaponPathUtf8,
        static_cast<uint32>(_weaponModel->Get_NumMeshes()),
        static_cast<uint32>(_weaponModel->Get_NumMaterials())
    );

    return S_OK;
}

void TrailPreviewWeaponObject::Configure_WeaponMaterials()
{
    CHECK_NULL(_weaponModel);

    const uint32 numMaterials = static_cast<uint32>(_weaponModel->Get_NumMaterials());
    for (uint32 materialIndex = 0; materialIndex < numMaterials; ++materialIndex)
    {
        const Shared<Material> material = _weaponModel->Get_Material(materialIndex);
        if (nullptr == material)
            continue;

        MaterialCB materialCB = material->Get_CB();
        materialCB.emissiveColor = Vec4(0.48f, 0.75f, 0.66f, 1.f);
        materialCB.emissiveIntensity = 1.f;
        materialCB.emissiveMaskMul = 0.7f;
        materialCB.normalStrength = 1.f;
        materialCB.roughnessScale = 1.f;
        materialCB.metalnessScale = 1.f;

        material->Set_CB(materialCB);
        material->Set_ForcedPassIndex(static_cast<int32>(VtxMeshPassIndex::PackedAR_NMEPass));
    }
}

PlayableParts* TrailPreviewWeaponObject::Get_BodyPart() const
{
    const Shared<PB_Gustave> gustave = _gustave.lock();
    if (nullptr == gustave)
        return nullptr;

    return dynamic_cast<PlayableParts*>(gustave->Get_PartObject(L"Part_Body"));
}

Matrix TrailPreviewWeaponObject::Get_PlayerWorldMatrix() const
{
    const Shared<PB_Gustave> gustave = _gustave.lock();
    if (nullptr == gustave || nullptr == gustave->Get_Transform())
        return Matrix::Identity;

    return gustave->Get_Transform()->Get_WorldMatrix();
}

Matrix TrailPreviewWeaponObject::Build_WeaponOffsetMatrix() const
{
    return
        Matrix::CreateRotationX(XMConvertToRadians(kWeaponAttachRotationDegrees.x)) *
        Matrix::CreateRotationY(XMConvertToRadians(kWeaponAttachRotationDegrees.y)) *
        Matrix::CreateRotationZ(XMConvertToRadians(kWeaponAttachRotationDegrees.z));
}

void TrailPreviewWeaponObject::Update_WeaponWorldMatrix()
{
    const Matrix playerWorldMatrix = Get_PlayerWorldMatrix();
    const PlayableParts* bodyPart = Get_BodyPart();

    if (nullptr != bodyPart)
        Log_AttachmentSocket(*bodyPart, playerWorldMatrix);
    else if (!_socketDebugLogged)
    {
        LOG_INFO("[TrailPreview] body part missing. attachment socket unavailable.");
        _socketDebugLogged = true;
    }

    if (nullptr == bodyPart)
    {
        Update_FallbackWorldMatrix(playerWorldMatrix);
        return;
    }

    const Matrix* socketMatrix = bodyPart->Get_SocketBoneMatrixPtr(kWeaponSocketName);
    if (nullptr == socketMatrix)
    {
        if (!_socketWarningLogged)
        {
            LOG_WARN("[TrailPreview] socket '{}' not found. Lanceram uses fallback preview pose.", kWeaponSocketName);
            _socketWarningLogged = true;
        }

        Update_FallbackWorldMatrix(playerWorldMatrix);
        return;
    }

    _weaponWorldMatrix = Build_WeaponOffsetMatrix() * *socketMatrix * playerWorldMatrix;
    _isSocketAttached = true;
    Update_TrailPoints();
}

void TrailPreviewWeaponObject::Update_FallbackWorldMatrix(const Matrix& playerWorldMatrix)
{
    _weaponWorldMatrix =
        Build_WeaponOffsetMatrix() *
        Matrix::CreateTranslation(kFallbackLocalPosition) *
        playerWorldMatrix;
    _isSocketAttached = false;
    Update_TrailPoints();
}

void TrailPreviewWeaponObject::Update_TrailPoints()
{
    const Vec3 baseLocal{ kTrailBaseLocalOffset.x, kTrailBaseLocalOffset.y, kTrailBaseLocalOffset.z };
    const Vec3 tipLocal{ kTrailTipLocalOffset.x, kTrailTipLocalOffset.y, kTrailTipLocalOffset.z };
    _trailBaseWorldPosition = Vec3::Transform(baseLocal, _weaponWorldMatrix);
    _trailTipWorldPosition = Vec3::Transform(tipLocal, _weaponWorldMatrix);
}

void TrailPreviewWeaponObject::Log_AttachmentSocket(
    const PlayableParts& bodyPart,
    const Matrix& playerWorldMatrix)
{
    if (_socketDebugLogged)
        return;

    const Matrix* socketMatrix = bodyPart.Get_SocketBoneMatrixPtr(kWeaponSocketName);
    if (nullptr == socketMatrix)
    {
        LOG_INFO("[TrailPreview] attachment socket '{}' missing.", kWeaponSocketName);
        _socketDebugLogged = true;
        return;
    }

    const Matrix socketWorldMatrix = *socketMatrix * playerWorldMatrix;
    const Vec3 socketWorldPosition = socketWorldMatrix.Translation();
    LOG_INFO(
        "[TrailPreview] attachment socket '{}' world position=({}, {}, {})",
        kWeaponSocketName,
        socketWorldPosition.x,
        socketWorldPosition.y,
        socketWorldPosition.z
    );

    _socketDebugLogged = true;
}

HRESULT TrailPreviewWeaponObject::Bind_ShaderResources(uint32 meshIndex)
{
    const uint32 materialIndex = _weaponModel->Get_MeshMaterialIndex(meshIndex);
    const Shared<Material> material = _weaponModel->Get_Material(materialIndex);
    CHECK_NULL(material, E_FAIL);

    CHECK_FAILED(_weaponShader->Bind_MaterialTextures(*material), E_FAIL);
    CHECK_FAILED(_weaponShader->Bind_CBufferData(material->Get_CB()), E_FAIL);
    CHECK_FAILED(_weaponShader->Begin(static_cast<uint32>(material->Get_PassIndex())), E_FAIL);

    return S_OK;
}

Shared<TrailPreviewWeaponObject> TrailPreviewWeaponObject::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context)
{
    auto instance = make_shared<TrailPreviewWeaponObject>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : TrailPreviewWeaponObject");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> TrailPreviewWeaponObject::Clone(void* arg)
{
    auto instance = make_shared<TrailPreviewWeaponObject>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : TrailPreviewWeaponObject");
        MSG_BOX("Failed to Clone : TrailPreviewWeaponObject");
        return nullptr;
    }

    return instance;
}

void TrailPreviewWeaponObject::Free()
{
    __super::Free();
}

NS_END
