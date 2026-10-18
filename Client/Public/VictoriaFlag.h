#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)
class ColliderCom;
class ModelCom;
class ShaderCom;
class Texture;
NS_END

NS_BEGIN(Client)

class EffectCom;

class VictoriaFlag : public GameObject
{
    GENERATED_GAMEOBJECT(VictoriaFlag, ObjectType::StaticMesh)

public:
    struct VICTORIAFLAG_DESC final : public GameObject::GAMEOBJECT_DESC
    {
        wstring clothModelTag = TEXT("Model_Flag_Cloth");
        wstring postModelTag = TEXT("Model_Flag_Cylinder");

        wstring flagShaderTag = TEXT("Shader_VictoriaFlag");
        wstring postShaderTag = TEXT("Shader_VtxMesh");

        wstring numberTextureTag = TEXT("Texture_VictoriaFlag_Numbers");
        wstring numberAtlasTextureTag = TEXT("Texture_VictoriaFlag_NumbersAtlas");
    };

public:
    VictoriaFlag(const ComPtr<Device>& device, const ComPtr<Context>& context);
    VictoriaFlag(const VictoriaFlag& prototype);
    ~VictoriaFlag() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Priority_Update(float timeDelta) override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;
    void    OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void    OnStayOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void    OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;

private:
    HRESULT Ready_Components(const VICTORIAFLAG_DESC& desc);
    HRESULT Bind_FlagShaderResources();
    HRESULT Bind_PostShaderResources();
    Matrix  Build_LocalMatrix(const Vec3& position, const Vec3& rotationDegree, const Vec3& scale) const;
    bool    Has_ActiveHeroOverlap() const;

private:
    Shared<ModelCom> _clothModel = nullptr;
    Shared<ModelCom> _postModel = nullptr;
    Shared<ShaderCom> _flagShader = nullptr;
    Shared<ShaderCom> _postShader = nullptr;
    Shared<Texture> _numberTexture = nullptr;
    Shared<Texture> _numberAtlasTexture = nullptr;
    Shared<ColliderCom> _interactionColliderCom = nullptr;
    Shared<EffectCom> _effectCom = nullptr;

    Vec3 _clothLocalPosition = Vec3(0.f, 51.586124f, 226.74954f);
    Vec3 _clothLocalRotation = Vec3::Zero;
    Vec3 _clothLocalScale = Vec3(1.f, 0.68011755f, 1.f);

    Vec3 _postLocalPosition = Vec3(0.f, 0.f, -45.f);
    Vec3 _postLocalRotation = Vec3(0.f, -79.89521f, 0.f);
    Vec3 _postLocalScale = Vec3(1.f, 1.f, 0.929015f);

    float _flagElapsed = 0.f;
    float _playbackSpeed = 1.f;
    float _flagMotionScale = 1.f;
    float _flagFixedDirection = -1.f;

    float _brightness = 1.f;
    float _flagNumber = 158.f;
    Vec2 _numberCenter = Vec2(0.47f, 0.49f);
    float _numberScale = 1.3f;
    float _numberSpacing = 0.13f;
    uset<ColliderCom*> _playerOverlapColliders{};

public:
    static Shared<VictoriaFlag> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;

};

NS_END
