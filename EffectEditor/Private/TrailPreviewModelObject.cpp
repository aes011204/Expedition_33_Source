#include "pch.h"
#include "TrailPreviewModelObject.h"

#include "GameInstance.h"
#include "Material.h"
#include "ModelCom.h"
#include "ShaderCom.h"
#include "ShaderPassFinder.h"
#include "EffectEditorInstance.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    const Vec3 kRestPosition{ 0.f, 0.f, 0.f };
    const Vec3 kRestRotationDegrees{ 0.f, 0.f, 0.f };
    const Vec3 kRestScale{ 1.f, 1.f, 1.f };

    constexpr float kSwingDurationSeconds{ 1.2f };
    constexpr float kSwingYawAmplitudeDegrees{ 52.f };
    constexpr float kSwingRollAmplitudeDegrees{ 34.f };
    constexpr float kSwingPitchAmplitudeDegrees{ 12.f };

    const Vec3 kTrailBaseLocalOffset{ 0.f, 0.04f, 0.f };
    const Vec3 kTrailTipLocalOffset{ 0.f, 1.35f, 0.f };
}

TrailPreviewModelObject::TrailPreviewModelObject(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

TrailPreviewModelObject::TrailPreviewModelObject(const TrailPreviewModelObject& prototype)
    : GameObject{ prototype }
{
}

HRESULT TrailPreviewModelObject::Initialize(void* arg)
{
    CHECK_FAILED(__super::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    Reset_Pose();
    Set_Name(L"Lanceram_TrailPreview");

    return S_OK;
}

void TrailPreviewModelObject::Update(float timeDelta)
{
    const bool visible = EDITOR != nullptr && EDITOR->Is_TrailPreviewWeaponVisible();
    Set_Visible(visible);

    if (!visible)
    {
        _swingTime = 0.f;
        Reset_Pose();
        return;
    }

    if (EDITOR->IsPlaying())
    {
        Update_Swing(timeDelta);
        return;
    }

    _swingTime = 0.f;
    Reset_Pose();
}

void TrailPreviewModelObject::Late_Update(float)
{
    if (Is_Visible() && _model != nullptr && _shader != nullptr)
        GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
}

HRESULT TrailPreviewModelObject::Render()
{
    CHECK_FAILED(Bind_ShaderResources(), E_FAIL);

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
    {
        const uint32 materialIndex = _model->Get_MeshMaterialIndex(meshIndex);
        const Shared<Material> material = _model->Get_Material(materialIndex);
        if (material == nullptr)
            continue;

        if (FAILED(_shader->Bind_MaterialTextures(*material)))
            continue;

        CHECK_FAILED(_shader->Bind_CBufferData(material->Get_CB()), E_FAIL);
        CHECK_FAILED(_shader->Begin(static_cast<uint32>(material->Get_PassIndex())), E_FAIL);
        CHECK_FAILED(_model->Render(meshIndex), E_FAIL);
    }

    return S_OK;
}

wstring TrailPreviewModelObject::Resolve_ModelPath()
{
    wchar_t modulePath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0)
        return {};

    const fs::path runtimeRoot = fs::path(modulePath).parent_path();
    return fs::absolute(runtimeRoot / kModelRelativePath)
           .lexically_normal()
           .wstring();
}

HRESULT TrailPreviewModelObject::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), L"Shader_VtxMesh", _shader), E_FAIL);

    const wstring modelPath = Resolve_ModelPath();
    if (modelPath.empty())
        return E_FAIL;

    const string modelPathUtf8 = String::ToString(modelPath);
    _model = ModelCom::Create(_device, _context, ModelType::NonAnim, modelPathUtf8.c_str(), Matrix::CreateScale(0.05f));
    CHECK_NULL(_model, E_FAIL);
    Configure_Materials();

    LOG_INFO(
        "[TrailPreviewModel] loaded. path={}, meshes={}, materials={}",
        modelPathUtf8,
        static_cast<uint32>(_model->Get_NumMeshes()),
        static_cast<uint32>(_model->Get_NumMaterials())
    );

    return S_OK;
}

HRESULT TrailPreviewModelObject::Bind_ShaderResources()
{
    CHECK_FAILED(_shader->Bind_ObjectCB(_transformCom->Get_WorldMatrix()), E_FAIL);
    CHECK_FAILED(GAME->Bind_CameraCB(_shader), E_FAIL);

    return S_OK;
}

void TrailPreviewModelObject::Configure_Materials()
{
    CHECK_NULL(_model);

    const uint32 numMaterials = static_cast<uint32>(_model->Get_NumMaterials());
    for (uint32 materialIndex = 0; materialIndex < numMaterials; ++materialIndex)
    {
        const Shared<Material> material = _model->Get_Material(materialIndex);
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

void TrailPreviewModelObject::Reset_Pose()
{
    _transformCom->Set_Position(Internal::kRestPosition);
    _transformCom->Set_RotationEuler(Internal::kRestRotationDegrees);
    _transformCom->Set_Scale(Internal::kRestScale);
}

void TrailPreviewModelObject::Update_Swing(float timeDelta)
{
    _swingTime += max(0.f, timeDelta);

    const float normalizedTime = fmodf(_swingTime, Internal::kSwingDurationSeconds) / Internal::kSwingDurationSeconds;
    const float swing = sinf(normalizedTime * XM_2PI);
    const float followThrough = sinf((normalizedTime * XM_2PI) + XM_PIDIV2);

    _transformCom->Set_Position(Internal::kRestPosition);
    _transformCom->Set_RotationEuler(
        Vec3(
            Internal::kRestRotationDegrees.x + followThrough * Internal::kSwingPitchAmplitudeDegrees,
            Internal::kRestRotationDegrees.y + swing * Internal::kSwingYawAmplitudeDegrees,
            Internal::kRestRotationDegrees.z + swing * Internal::kSwingRollAmplitudeDegrees
        )
    );
    _transformCom->Set_Scale(Internal::kRestScale);
}

Shared<TrailPreviewModelObject> TrailPreviewModelObject::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<TrailPreviewModelObject>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : TrailPreviewModelObject");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> TrailPreviewModelObject::Clone(void* arg)
{
    auto instance = make_shared<TrailPreviewModelObject>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : TrailPreviewModelObject");
        MSG_BOX("Failed to Clone : TrailPreviewModelObject");
        return nullptr;
    }

    return instance;
}

void TrailPreviewModelObject::Free()
{
    __super::Free();
}

NS_END
