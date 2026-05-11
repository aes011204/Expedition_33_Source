#include "ComputePointParticlePreview.h"

#include "GameInstance.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "VIBufferCom_ComputePointParticle.h"

IMPLEMENT_REFLECTION(ComputePointParticlePreview)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "ComputePointParticlePreview";
    info.category = "Effect";

    return true;
}

ComputePointParticlePreview::ComputePointParticlePreview(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

ComputePointParticlePreview::ComputePointParticlePreview(const ComputePointParticlePreview& prototype)
    : GameObject{ prototype }
{
}

ComputePointParticlePreview::~ComputePointParticlePreview()
{
    Free();
}

HRESULT ComputePointParticlePreview::Initialize_Prototype()
{
    return S_OK;
}

HRESULT ComputePointParticlePreview::Initialize(void* arg)
{
    if (nullptr != arg)
        _desc = *static_cast<ComputePointParticlePreviewDesc*>(arg);

    if (FAILED(__super::Initialize(&_desc)))
        return E_FAIL;

    CHECK_FAILED(Ready_Components(), E_FAIL);

    return S_OK;
}

void ComputePointParticlePreview::Update(float timeDelta)
{
    if (nullptr != _viBuffer)
        _viBuffer->Dispatch_Compute(timeDelta);
}

void ComputePointParticlePreview::Late_Update(float timeDelta)
{
    GAME->Add_RenderGroup(RenderGroup::Blend, GetSharedPtr<GameObject>());
}

HRESULT ComputePointParticlePreview::Render()
{
    CHECK_FAILED(Bind_ShaderResources(), E_FAIL);
    CHECK_FAILED(_shader->Begin(0), E_FAIL);
    CHECK_FAILED(_viBuffer->Bind_Resources(), E_FAIL);
    CHECK_FAILED(_viBuffer->Render(), E_FAIL);

    return S_OK;
}

HRESULT ComputePointParticlePreview::Ready_Components()
{
    CHECK_FAILED(
        __super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Texture_Snow"), _texture),
        E_FAIL
    );
    CHECK_FAILED(
        __super::Add_Component(ETOI(LevelType::Static), TEXT("Shader_BillboardParticle"), _shader),
        E_FAIL
    );

    ComputePointParticleDesc desc = Make_ComputePointParticleDesc();
    CHECK_FAILED(
        __super::Add_Component(ETOI(LevelType::Static), TEXT("Com_VIBuffer_ComputePointParticle"), _viBuffer, &desc),
        E_FAIL
    );

    return S_OK;
}

HRESULT ComputePointParticlePreview::Bind_ShaderResources()
{
    CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix"), E_FAIL);
    CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix"), E_FAIL);
    CHECK_FAILED(GAME->Bind_CamPosition(_shader.get(), "g_vBillboardTarget"), E_FAIL);
    CHECK_FAILED(_texture->Bind_ShaderResourceView(_shader.get(), "g_Texture", 0), E_FAIL);

    return S_OK;
}

ComputePointParticleDesc ComputePointParticlePreview::Make_ComputePointParticleDesc() const
{
    ComputePointParticleDesc desc{};
    desc.instanceCount = 96;
    desc.center = _desc.center;
    desc.range = Vec3{ 3.f, 1.f, 3.f };
    desc.scale = Vec2{ 0.12f, 0.28f };
    desc.lifeTime = Vec2{ 1.f, 2.f };
    desc.radius = Vec2{ 1.2f, 2.2f };
    desc.drawMode = _desc.drawMode;
    desc.computeShaderLevelIndex = ETOI(LevelType::Static);
    desc.computeShaderPrototypeTag = L"ComputeShader_PointParticle";

    return desc;
}

Shared<ComputePointParticlePreview> ComputePointParticlePreview::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<ComputePointParticlePreview>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : ComputePointParticlePreview");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> ComputePointParticlePreview::Clone(void* arg)
{
    auto instance = make_shared<ComputePointParticlePreview>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : ComputePointParticlePreview");
        MSG_BOX("Failed to Clone : ComputePointParticlePreview");
        return nullptr;
    }

    return instance;
}

void ComputePointParticlePreview::Free()
{
    __super::Free();
}
