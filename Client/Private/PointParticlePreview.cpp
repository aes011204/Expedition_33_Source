#include "PointParticlePreview.h"
#include "GameInstance.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "VIBufferCom_PointParticle.h"

IMPLEMENT_REFLECTION(PointParticlePreview)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "PointParticlePreview";
    info.category = "Effect";

    return true;
}

PointParticlePreview::PointParticlePreview(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

PointParticlePreview::PointParticlePreview(const PointParticlePreview& prototype)
    : GameObject{ prototype }
{
}

HRESULT PointParticlePreview::Initialize_Prototype()
{
    return S_OK;
}

HRESULT PointParticlePreview::Initialize(void* arg)
{
    if (FAILED(__super::Initialize(arg)))
        return E_FAIL;

    if (FAILED(Ready_Components()))
        return E_FAIL;

    return S_OK;
}

void PointParticlePreview::Update(float timeDelta)
{
    if (nullptr != _viBuffer)
        _viBuffer->Spread(timeDelta);
}

void PointParticlePreview::Late_Update(float)
{
    GAME->Add_RenderGroup(RenderGroup::Blend, GetSharedPtr<GameObject>());
}

HRESULT PointParticlePreview::Render()
{
    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    if (FAILED(_shader->Begin(0)))
        return E_FAIL;

    if (FAILED(_viBuffer->Bind_Resources()))
        return E_FAIL;

    if (FAILED(_viBuffer->Render()))
        return E_FAIL;

    return S_OK;
}

HRESULT PointParticlePreview::Ready_Components()
{
    if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Texture_Snow"), _texture)))
        return E_FAIL;

    if (FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("Shader_BillboardParticle"), _shader)))
        return E_FAIL;

    PointParticleDesc desc = Make_PointParticleDesc();
    if (FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("Com_VIBuffer_PointParticle"), _viBuffer, &desc)))
        return E_FAIL;

    return S_OK;
}

HRESULT PointParticlePreview::Bind_ShaderResources()
{
    if (FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix")))
        return E_FAIL;

    if (FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix")))
        return E_FAIL;

    if (FAILED(GAME->Bind_CamPosition(_shader.get(), "g_vBillboardTarget")))
        return E_FAIL;

    if (FAILED(_texture->Bind_ShaderResourceView(_shader.get(), "g_Texture", 0)))
        return E_FAIL;

    return S_OK;
}

PointParticleDesc PointParticlePreview::Make_PointParticleDesc() const
{
    PointParticleDesc desc{};
    desc.instanceCount = 64;
    desc.center = Vec3{ 0.f, 2.f, 0.f };
    desc.range = Vec3{ 2.f, 2.f, 2.f };
    desc.scale = Vec2{ 0.15f, 0.35f };
    desc.pivot = Vec3{ 0.f, 2.f, 0.f };
    desc.speed = Vec2{ 0.5f, 1.5f };
    desc.lifeTime = Vec2{ 1.f, 2.f };
    desc.loop = true;

    return desc;
}

Shared<PointParticlePreview> PointParticlePreview::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<PointParticlePreview>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : PointParticlePreview");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> PointParticlePreview::Clone(void* arg)
{
    auto instance = make_shared<PointParticlePreview>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : PointParticlePreview");
        MSG_BOX("Failed to Clone : PointParticlePreview");
        return nullptr;
    }

    return instance;
}

void PointParticlePreview::Free()
{
    __super::Free();
}
