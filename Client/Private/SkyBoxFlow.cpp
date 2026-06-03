#include "SkyBoxFlow.h"
#include "Camera.h"
#include "GameInstance.h"
#include "ModelCom.h"
#include "ShaderCom.h"
#include "Texture.h"

IMPLEMENT_REFLECTION(SkyBoxFlow)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "SkyBoxFlow";
    info.category = "World";

    info.properties.push_back(PropertyInfo{
        "_tint", "Tint", "_tint",
        PropertyType::Vec4, offsetof(SelfType, _tint),
        0.f, 0.f, 0.01f });

    info.properties.push_back(PropertyInfo{
        "_brightness", "Brightness", "_brightness",
        PropertyType::Float, offsetof(SelfType, _brightness),
        0.f, 0.f, 1.f });

    info.properties.push_back(PropertyInfo{
        "_contrast", "Contrast", "_contrast",
        PropertyType::Float, offsetof(SelfType, _contrast),
        0.f, 0.f, 0.01f });

    info.properties.push_back(PropertyInfo{
        "_noiseStrength", "Noise Strength", "_noiseStrength",
        PropertyType::Float, offsetof(SelfType, _noiseStrength),
        0.f, 0.f, 0.01f });

    info.properties.push_back(PropertyInfo{
        "_speed", "Flow Speed", "_speed",
        PropertyType::Float, offsetof(SelfType, _speed),
        0.f, 0.f, 0.0001f });

    return true;
}

SkyBoxFlow::SkyBoxFlow(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

SkyBoxFlow::SkyBoxFlow(const SkyBoxFlow& prototype)
    : GameObject{ prototype }
{
}

HRESULT SkyBoxFlow::Initialize_Prototype()
{
    return S_OK;
}

HRESULT SkyBoxFlow::Initialize(void* pArg)
{
    if (FAILED(GameObject::Initialize(pArg)))
        return E_FAIL;

    SKYBOXFLOW_DESC desc{};
    if (pArg != nullptr)
        desc = *static_cast<SKYBOXFLOW_DESC*>(pArg);

    _tint = desc.tint;
    _brightness = desc.brightness;
    _contrast = desc.contrast;
    _noiseStrength = desc.noiseStrength;
    _speed = desc.speed;

    if (FAILED(Ready_Components(desc)))
        return E_FAIL;

    // 원본 BP_SkyBox_Sandfall은 카메라 배경용 대형 static mesh라서 오브젝트 scale에서만 크기를 조절한다.
    _transformCom->Set_Scale(Vec3(desc.scale, desc.scale, desc.scale));

    return S_OK;
}

void SkyBoxFlow::Priority_Update(float fTimeDelta)
{
    UNREFERENCED_PARAMETER(fTimeDelta);
}

void SkyBoxFlow::Update(float fTimeDelta)
{
    // Flowmap 시간은 렌더 직전 shader가 소비하므로 Update에서 누적한다.
    _materialTime += fTimeDelta;
}

void SkyBoxFlow::Late_Update(float fTimeDelta)
{
    UNREFERENCED_PARAMETER(fTimeDelta);

    // 기존 Sky와 같은 배경 렌더 순서를 사용한다.
    GAME->Add_RenderGroup(RenderGroup::Priority, GetSharedPtr<GameObject>());
}

HRESULT SkyBoxFlow::Render()
{
    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    if (_model == nullptr)
        return E_FAIL;

    const uint32 meshCount = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 i = 0; i < meshCount; ++i)
    {
        if (FAILED(_shader->Begin(0)))
            return E_FAIL;

        if (FAILED(_model->Render(i)))
            return E_FAIL;
    }

    return S_OK;
}

HRESULT SkyBoxFlow::Ready_Components(const SKYBOXFLOW_DESC& desc)
{
    if (FAILED(__super::Add_Component(ETOI(LevelType::Static), desc.modelTag.c_str(), _model)))
        return E_FAIL;

    if (FAILED(__super::Add_Component(ETOI(LevelType::Static), desc.shaderTag.c_str(), _shader)))
        return E_FAIL;

    if (FAILED(__super::Add_Component(ETOI(LevelType::Static), desc.skyTextureTag.c_str(), _skyTexture)))
        return E_FAIL;

    if (FAILED(__super::Add_Component(ETOI(LevelType::Static), desc.flowTextureTag.c_str(), _flowTexture)))
        return E_FAIL;

    return S_OK;
}

HRESULT SkyBoxFlow::Bind_ShaderResources()
{
    Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
    if (activeCamera != nullptr)
    {
        Shared<TransformCom> cameraTransform = activeCamera->Get_Transform();
        if (cameraTransform != nullptr)
        {
            // Skybox는 카메라 위치를 따라가야 배경이 월드 이동에 끌려 보이지 않는다.
            _transformCom->Set_WorldPosition(cameraTransform->Get_WorldPosition());
        }
    }

    CHECK_FAILED(_shader->Bind_ObjectCB(_transformCom->Get_WorldMatrix()), E_FAIL);
    CHECK_FAILED(GAME->Bind_CameraCB(_shader), E_FAIL);

    CHECK_FAILED(_skyTexture->Bind_ShaderResourceView(_shader.get(), "g_SkyTexture", 0), E_FAIL);
    CHECK_FAILED(_flowTexture->Bind_ShaderResourceView(_shader.get(), "g_FlowTexture", 0), E_FAIL);
    CHECK_FAILED(_flowTexture->Bind_ShaderResourceView(_shader.get(), "g_NoiseTexture", 0), E_FAIL);

    DrawFeatureCB skyBoxCB{};
    skyBoxCB.param0 = _tint; // M_Flowmap_CleasTower의 Tint.
    skyBoxCB.param1 = Vec4(
        _brightness,
        _contrast,
        _noiseStrength,
        _materialTime * _speed); // shader는 누적된 flow offset만 소비한다.

    CHECK_FAILED(_shader->Bind_CBufferData(skyBoxCB), E_FAIL);

    return S_OK;
}

Shared<SkyBoxFlow> SkyBoxFlow::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<SkyBoxFlow>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : SkyBoxFlow");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> SkyBoxFlow::Clone(void* pArg)
{
    auto instance = make_shared<SkyBoxFlow>(*this);

    if (FAILED(instance->Initialize(pArg)))
    {
        MSG_BOX("Failed to Cloned : SkyBoxFlow");
        return nullptr;
    }

    return instance;
}

void SkyBoxFlow::Free()
{
    __super::Free();
}
