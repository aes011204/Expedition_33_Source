#include "pch.h"
#include "Hair_Player.h"
#include "GameInstance.h"
#include "Material.h"
#include "Material_Types.h"
#include "Player.h"
#include "ModelCom.h"

Hair_Player::Hair_Player(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : PlayableParts{ device, context } {}

Hair_Player::Hair_Player(const Hair_Player& prototype)
    : PlayableParts{ prototype } {}

HRESULT Hair_Player::Initialize_Prototype()
{
    return PlayableParts::Initialize_Prototype();
}

HRESULT Hair_Player::Initialize(void* arg)
{
    if (FAILED(PlayableParts::Initialize(arg)))
    {
        LOG_ERROR("[Hair_Player] : Failed to Initialize");
        return S_OK;
    }

    _offsetMatrix = Matrix::Identity;

    //Vec3 hairOffset = Vec3(17.61f, 0.0f, -2.765f);
    Vec3 hairOffset = Vec3(17.61f, -2.765f, 0.f);

    _transformCom->Set_Position(hairOffset);


    return S_OK;
}

void Hair_Player::BeginPlay()
{
    PlayableParts::BeginPlay();
    
}

void Hair_Player::Priority_Update(float timeDelta)
{
}

void Hair_Player::Update(float timeDelta)
{
    Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());
}

void Hair_Player::Late_Update(float timeDelta)
{
    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
    GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
}

HRESULT Hair_Player::Render()
{
    //return PlayableParts::Render();

    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    const uint32 numMeshes = static_cast<uint32>(_modelCom->Get_NumMeshes());
    for (uint32 i = 0; i < numMeshes; i++)
    {
        _modelCom->Bind_Material(_shaderCom.get(), "g_DiffuseTexture", i, MaterialTextureSlot::BaseColor);

        const Material::TextureSlot* baseColorSlot = _modelCom->Find_MaterialSlot(i, { MaterialTextureSlot::BaseColor });
        const Vec2 uvScale = Vec2(1.f, 1.f); //baseColorSlot ? baseColorSlot->uvScale : Vec2(1.f, 1.f);
        const Vec2 uvOffset = Vec2(0.f, 0.f); //baseColorSlot ? baseColorSlot->uvOffset : Vec2(0.f, 0.f);
        const float uvRotation = 0.f; // baseColorSlot ? baseColorSlot->uvRotation : 0.f;
        const int32 useOpacityMask = 0;

        _shaderCom->Bind_RawValue("g_UVScale", &uvScale, sizeof(Vec2));
        _shaderCom->Bind_RawValue("g_UVOffset", &uvOffset, sizeof(Vec2));
        _shaderCom->Bind_RawValue("g_UVRotation", &uvRotation, sizeof(float));
        _shaderCom->Bind_RawValue("g_UseOpacityMask", &useOpacityMask, sizeof(int32));

        _modelCom->Bind_BoneMatrices(_shaderCom.get(), "g_BoneMatrices", i);

        if (FAILED(_shaderCom->Begin(0)))
            return E_FAIL;

        _modelCom->Render(i);
    }

    return S_OK;
}

HRESULT Hair_Player::Render_Shadow()
{
    return PlayableParts::Render_Shadow();
}

Shared<Hair_Player> Hair_Player::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Hair_Player>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("[Hair_Player] : Failed to Initialize Prototype");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> Hair_Player::Clone(void* arg)
{
    auto clone = make_shared<Hair_Player>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("[Hair_Player] : Failed to Initialize");
        return nullptr;
    }

    return clone;
}

void Hair_Player::Free()
{
    __super::Free();
}



