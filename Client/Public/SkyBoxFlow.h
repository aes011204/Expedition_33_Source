#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)
class ModelCom;
class ShaderCom;
class Texture;
NS_END

NS_BEGIN(Client)

class SkyBoxFlow : public GameObject
{
    GENERATED_GAMEOBJECT(SkyBoxFlow, ObjectType::Sky)

public:
    struct SKYBOXFLOW_DESC final : public GameObject::GAMEOBJECT_DESC
    {
        wstring modelTag = TEXT("Model_SM_Skybox_Mesh");          // 원본 BP_SkyBox_Sandfall의 SM_Skybox_Mesh를 가리킨다.
        wstring shaderTag = TEXT("Shader_SkyBoxFlow");            // 원본 Flowmap sky material 전용 shader다.
        wstring skyTextureTag = TEXT("Texture_SkyBox_CleasTower");// M_Flowmap_CleasTower의 SkyTexture다.
        wstring flowTextureTag = TEXT("Texture_SkyBox_Flow03");   // FlowTexture와 NoiseTexture 모두 같은 원본 T_flowmap_03을 쓴다.

        Vec4 tint = Vec4(1.604023f, 0.887117f, 0.631814f, 1.f);
        float brightness = 500.f;
        float contrast = 1.524392f;
        float noiseStrength = 0.25f;
        float speed = 0.001f;
        float scale = 1.f;
    };

public:
    SkyBoxFlow(const ComPtr<Device>& device, const ComPtr<Context>& context);
    SkyBoxFlow(const SkyBoxFlow& prototype);
    virtual ~SkyBoxFlow() = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* pArg) override;
    virtual void Priority_Update(float fTimeDelta) override;
    virtual void Update(float fTimeDelta) override;
    virtual void Late_Update(float fTimeDelta) override;
    virtual HRESULT Render() override;

private:
    HRESULT Ready_Components(const SKYBOXFLOW_DESC& desc);
    HRESULT Bind_ShaderResources();

private:
    Shared<ModelCom> _model = nullptr;
    Shared<ShaderCom> _shader = nullptr;
    Shared<Texture> _skyTexture = nullptr;
    Shared<Texture> _flowTexture = nullptr;

    Vec4 _tint = Vec4(1.604023f, 0.887117f, 0.631814f, 1.f);
    float _brightness = 500.f;
    float _contrast = 1.524392f;
    float _noiseStrength = 0.25f;
    float _speed = 0.001f;
    float _materialTime = 0.f;

public:
    static Shared<SkyBoxFlow> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* pArg) override;
    virtual void Free() override;
};

NS_END
