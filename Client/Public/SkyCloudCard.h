#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)
class ModelCom;
class ShaderCom;
class Texture;
NS_END

NS_BEGIN(Client)

class SkyCloudCard : public GameObject
{
    GENERATED_GAMEOBJECT(SkyCloudCard, ObjectType::Sky)

public:
    struct SKYCLOUDCARD_DESC final : public GameObject::GAMEOBJECT_DESC
    {
        wstring modelTag = TEXT("Model_Cloud_Mesh");
        wstring shaderTag = TEXT("Shader_SkyCloudCard");
        wstring cloudTextureTag = TEXT("Texture_SkyCloud_CleasTower_2");

        Vec3 position = Vec3::Zero;
        Vec3 rotation = Vec3::Zero;
        Vec3 scale = Vec3::One;

        Vec4 tint = Vec4(0.155208f, 0.093756f, 0.064809f, 1.f);
        Vec4 skyScatterColor = Vec4(0.557292f, 0.886886f, 1.f, 1.f);
        Vec4 groundScatterColor = Vec4(0.f, 0.f, 0.f, 1.f);
        Vec4 lightOffset = Vec4(1.f, 1.f, 1.f, 1.f);

        float scatterBlend = -0.010876f;
        float shadowBlur = 0.05f;
        float glow = 20.f;
        float opacity = 1.f;
    };

public:
    SkyCloudCard(const ComPtr<Device>& device, const ComPtr<Context>& context);
    SkyCloudCard(const SkyCloudCard& prototype);
    virtual ~SkyCloudCard() = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* pArg) override;
    void Priority_Update(float timeDelta) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

private:
    HRESULT Ready_Components(const SKYCLOUDCARD_DESC& desc);
    HRESULT Bind_ShaderResources();

private:
    Shared<ModelCom> _model = nullptr;
    Shared<ShaderCom> _shader = nullptr;
    Shared<Texture> _cloudTexture = nullptr;

    Vec4 _tint = Vec4(0.155208f, 0.093756f, 0.064809f, 1.f);
    Vec4 _skyScatterColor = Vec4(0.557292f, 0.886886f, 1.f, 1.f);
    Vec4 _groundScatterColor = Vec4(0.f, 0.f, 0.f, 1.f);
    Vec4 _lightOffset = Vec4(1.f, 1.f, 1.f, 1.f);

    float _scatterBlend = -0.010876f;
    float _shadowBlur = 0.05f;
    float _glow = 20.f;
    float _opacity = 1.f;

public:
    static Shared<SkyCloudCard> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* pArg) override;
    void Free() override;
};

NS_END
