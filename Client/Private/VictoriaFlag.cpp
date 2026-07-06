#include "VictoriaFlag.h"
#include "GameInstance.h"
#include "Bounding_Sphere.h"
#include "ClientInstance.h"
#include "ColliderCom.h"
#include "EffectCom.h"
#include "GameplayUI.h"
#include "Material.h"
#include "ModelCom.h"
#include "Player_WorldBase.h"
#include "ShaderCom.h"
#include "Texture.h"

namespace
{
    constexpr uint32 VICTORIA_FLAG_POST_ALPHA_FILL_PASS = 20u;
    const Vec3 kInteractionColliderCenter = Vec3(0.f, 80.f, -45.f);
    constexpr float kInteractionColliderRadius = 180.f;
}

IMPLEMENT_REFLECTION(VictoriaFlag)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "VictoriaFlag";
    info.category = "World";

    PROPERTY_VEC3("Flag Local Position", _clothLocalPosition, 0.1f);
    PROPERTY_VEC3("Flag Local Rotation", _clothLocalRotation, 0.1f);
    PROPERTY_VEC3("Flag Local Scale", _clothLocalScale, 0.01f);
    PROPERTY_VEC3("Pole Local Position", _postLocalPosition, 0.1f);
    PROPERTY_VEC3("Pole Local Rotation", _postLocalRotation, 0.1f);
    PROPERTY_VEC3("Pole Local Scale", _postLocalScale, 0.01f);

    PROPERTY_FLOAT("Playback Speed", _playbackSpeed, 0.f, 5.f, 0.01f);
    PROPERTY_FLOAT("Flag Motion Scale", _flagMotionScale, 0.f, 5.f, 0.01f);
    PROPERTY_FLOAT("Flag Fixed Direction", _flagFixedDirection, -1.f, 1.f, 1.f);
    PROPERTY_FLOAT("Brightness", _brightness, 0.f, 5.f, 0.01f);
    PROPERTY_FLOAT("Flag Number", _flagNumber, 0.f, 999.f, 1.f);
    PROPERTY_VEC2("Number Center", _numberCenter, 0.001f);
    PROPERTY_FLOAT("Number Scale", _numberScale, 0.1f, 3.f, 0.01f);
    PROPERTY_FLOAT("Number Spacing", _numberSpacing, 0.01f, 0.4f, 0.001f);

    return true;
}

VictoriaFlag::VictoriaFlag(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{


}

VictoriaFlag::VictoriaFlag(const VictoriaFlag& prototype)
    : GameObject{ prototype }
{
}

HRESULT VictoriaFlag::Initialize_Prototype()
{
    return S_OK;
}

HRESULT VictoriaFlag::Initialize(void* arg)
{
    CHECK_FAILED(GameObject::Initialize(arg), E_FAIL);

    VICTORIAFLAG_DESC desc{};
    if (arg != nullptr)
    {
        desc = *static_cast<VICTORIAFLAG_DESC*>(arg);
    }

    CHECK_FAILED(Ready_Components(desc), E_FAIL);

    Set_ShadowCasterType(ShadowCasterType::Dynamic);

    return S_OK;
}

void VictoriaFlag::Priority_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void VictoriaFlag::Update(float timeDelta)
{
    _flagElapsed += max(timeDelta, 0.f);

    if (_effectCom)
        _effectCom->Update(timeDelta);

    if (!Has_ActiveHeroOverlap())
    {
        CLIENT->Clear_InteractionPrompt(this);
        return;
    }

    if (CLIENT->Is_WorldControlSuspended() ||
        CLIENT->IsDialogueActive() ||
        CLIENT->IsDialogueInteractLocked() ||
        GAME->Is_WorldGameplayInputBlocked())
    {
        CLIENT->Clear_InteractionPrompt(this);
        return;
    }

    CLIENT->Request_InteractionPrompt(this, InteractionPromptType::SavePoint);

    if (!GAME->KeyDown(KEY_TYPE::E))
        return;

    if (const Shared<GameplayUI> gameplayUI = CLIENT->Get_GameplayUI())
    {
        if (gameplayUI->Open_SavePoint())
            CLIENT->Clear_InteractionPrompt(this);
    }
}

void VictoriaFlag::Late_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());

    if (Casts_DynamicShadow())
        GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());

    if (_interactionColliderCom != nullptr && _interactionColliderCom->Get_IsActive())
    {
        _interactionColliderCom->Update_Collider(_transformCom->Get_WorldMatrix());
        GAME->Add_Collider(_interactionColliderCom);
    }
}

HRESULT VictoriaFlag::Render()
{
    CHECK_FAILED(Bind_FlagShaderResources(), E_FAIL);

    const uint32 clothMeshCount = static_cast<uint32>(_clothModel->Get_NumMeshes());
    for (uint32 i = 0; i < clothMeshCount; ++i)
    {
        const uint32 materialIndex = _clothModel->Get_MeshMaterialIndex(i);
        const Shared<Material> material = _clothModel->Get_Material(materialIndex);
        if (material == nullptr)
            continue;

        CHECK_FAILED(_flagShader->Bind_MaterialTextures(*material), E_FAIL);
        CHECK_FAILED(_flagShader->Bind_CBufferData(material->Get_CB()), E_FAIL);
        CHECK_FAILED(_flagShader->Begin(4), E_FAIL);
        CHECK_FAILED(_clothModel->Render(i), E_FAIL);
    }

    CHECK_FAILED(Bind_PostShaderResources(), E_FAIL);

    const uint32 postMeshCount = static_cast<uint32>(_postModel->Get_NumMeshes());
    for (uint32 i = 0; i < postMeshCount; ++i)
    {
        const uint32 materialIndex = _postModel->Get_MeshMaterialIndex(i);
        const Shared<Material> material = _postModel->Get_Material(materialIndex);
        if (material == nullptr)
            continue;

        CHECK_FAILED(_postShader->Bind_MaterialTextures(*material), E_FAIL);
        CHECK_FAILED(_postShader->Bind_CBufferData(material->Get_CB()), E_FAIL);
        CHECK_FAILED(_postShader->Begin(VICTORIA_FLAG_POST_ALPHA_FILL_PASS), E_FAIL);
        CHECK_FAILED(_postModel->Render(i), E_FAIL);
    }

    return S_OK;
}

HRESULT VictoriaFlag::Render_Shadow()
{
    CHECK_FAILED(Bind_FlagShaderResources(), E_FAIL);

    const uint32 clothMeshCount = static_cast<uint32>(_clothModel->Get_NumMeshes());
    for (uint32 i = 0; i < clothMeshCount; ++i)
    {
        const uint32 materialIndex = _clothModel->Get_MeshMaterialIndex(i);
        const Shared<Material> material = _clothModel->Get_Material(materialIndex);
        if (material == nullptr)
            continue;

        CHECK_FAILED(_flagShader->Bind_MaterialTextures(*material), E_FAIL);
        CHECK_FAILED(_flagShader->Bind_CBufferData(material->Get_CB()), E_FAIL);
        CHECK_FAILED(_flagShader->Begin(1), E_FAIL); // wave가 적용된 masked shadow pass다.
        CHECK_FAILED(_clothModel->Render(i), E_FAIL);
    }

    CHECK_FAILED(_postShader->Bind_ObjectCB(Build_LocalMatrix(_postLocalPosition, _postLocalRotation, _postLocalScale) * _transformCom->Get_WorldMatrix()), E_FAIL);
    CHECK_FAILED(GAME->Bind_ShadowCB(_postShader), E_FAIL);

    const uint32 postMeshCount = static_cast<uint32>(_postModel->Get_NumMeshes());
    for (uint32 i = 0; i < postMeshCount; ++i)
    {
        CHECK_FAILED(_postShader->Begin(1), E_FAIL);
        CHECK_FAILED(_postModel->Render(i), E_FAIL);
    }

    return S_OK;
}

HRESULT VictoriaFlag::Ready_Components(const VICTORIAFLAG_DESC& desc)
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), desc.clothModelTag, _clothModel), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), desc.postModelTag, _postModel), E_FAIL);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), desc.flagShaderTag, _flagShader), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), desc.postShaderTag, _postShader), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), desc.numberTextureTag, _numberTexture), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), desc.numberAtlasTextureTag, _numberAtlasTexture), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Effect"), _effectCom), E_FAIL);

    Bounding_Sphere::FBoundingSphereDesc sphereDesc{};
    sphereDesc.center = kInteractionColliderCenter;
    sphereDesc.radius = kInteractionColliderRadius;

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_Sphere"), _interactionColliderCom, &sphereDesc), E_FAIL);
    _interactionColliderCom->Set_CollisionPreset(Collision_Preset::Trigger);
    _interactionColliderCom->Set_IsActive(true);

    return S_OK;
}

HRESULT VictoriaFlag::Bind_FlagShaderResources()
{
    CHECK_FAILED(_flagShader->Bind_ObjectCB(Build_LocalMatrix(_clothLocalPosition, _clothLocalRotation, _clothLocalScale) * _transformCom->Get_WorldMatrix()), E_FAIL);
    CHECK_FAILED(GAME->Bind_CameraCB(_flagShader), E_FAIL);
    CHECK_FAILED(GAME->Bind_ShadowCB(_flagShader), E_FAIL);
    CHECK_FAILED(_numberTexture->Bind_ShaderResourceViews(_flagShader.get(), "g_NumberTextures"), E_FAIL);
    CHECK_FAILED(_numberAtlasTexture->Bind_ShaderResourceView(_flagShader.get(), "g_NumberAtlasTexture", 0), E_FAIL);

    DrawFeatureCB flagCB{};
    flagCB.param0 = Vec4(_flagElapsed, _playbackSpeed, 0.f, 0.f);
    flagCB.param1 = Vec4(0.f, 0.f, _flagMotionScale, _brightness);
    flagCB.param2 = Vec4(_numberCenter.x, _numberCenter.y, _numberScale, _flagFixedDirection);
    flagCB.param3 = Vec4(_numberSpacing, 0.f, 0.f, _flagNumber);

    CHECK_FAILED(_flagShader->Bind_CBufferData(flagCB), E_FAIL);
    return S_OK;
}

HRESULT VictoriaFlag::Bind_PostShaderResources()
{
    CHECK_FAILED(_postShader->Bind_ObjectCB(Build_LocalMatrix(_postLocalPosition, _postLocalRotation, _postLocalScale) * _transformCom->Get_WorldMatrix()), E_FAIL);
    CHECK_FAILED(GAME->Bind_CameraCB(_postShader), E_FAIL);

    return S_OK;
}

Matrix VictoriaFlag::Build_LocalMatrix(const Vec3& position, const Vec3& rotationDegree, const Vec3& scale) const
{
    const Vec3 rotationRadian(
        XMConvertToRadians(rotationDegree.x),
        XMConvertToRadians(rotationDegree.y),
        XMConvertToRadians(rotationDegree.z));

    return Matrix::CreateScale(scale) *
        Matrix::CreateFromYawPitchRoll(rotationRadian.y, rotationRadian.x, rotationRadian.z) *
        Matrix::CreateTranslation(position);
}

void VictoriaFlag::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnBeginOverlap(self, other);

    if (_interactionColliderCom == nullptr || self.get() != _interactionColliderCom.get())
        return;

    const Shared<GameObject> otherOwner = other ? other->Get_Owner() : nullptr;
    if (dynamic_pointer_cast<Player_WorldBase>(otherOwner) == nullptr)
        return;

    if (other != nullptr)
        _playerOverlapColliders.insert(other.get());
}

void VictoriaFlag::OnStayOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnStayOverlap(self, other);

    if (_interactionColliderCom == nullptr || self.get() != _interactionColliderCom.get())
        return;

    const Shared<GameObject> otherOwner = other ? other->Get_Owner() : nullptr;
    if (dynamic_pointer_cast<Player_WorldBase>(otherOwner) == nullptr)
        return;

    if (other != nullptr)
        _playerOverlapColliders.insert(other.get());
}

void VictoriaFlag::OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnEndOverlap(self, other);

    if (_interactionColliderCom == nullptr || self.get() != _interactionColliderCom.get())
        return;

    const Shared<GameObject> otherOwner = other ? other->Get_Owner() : nullptr;
    if (dynamic_pointer_cast<Player_WorldBase>(otherOwner) == nullptr)
        return;

    if (other != nullptr)
        _playerOverlapColliders.erase(other.get());

    if (!Has_ActiveHeroOverlap())
        CLIENT->Clear_InteractionPrompt(this);
}

bool VictoriaFlag::Has_ActiveHeroOverlap() const
{
    for (ColliderCom* collider : _playerOverlapColliders)
    {
        if (collider == nullptr)
            continue;

        if (CLIENT->Is_ActiveHero(collider->Get_Owner()))
            return true;
    }

    return false;
}

Shared<VictoriaFlag> VictoriaFlag::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<VictoriaFlag>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> VictoriaFlag::Clone(void* arg)
{
    auto instance = make_shared<VictoriaFlag>(*this);

    if (FAILED(instance->Initialize(arg)))
        return nullptr;

    return instance;
}

void VictoriaFlag::Free()
{
    _playerOverlapColliders.clear();
    CLIENT->Clear_InteractionPrompt(this);

    GameObject::Free();
}
