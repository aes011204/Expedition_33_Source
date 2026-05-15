#pragma once

#include "Client_Defines.h"
#include "GameObject.h"
#include "Particle_Types.h"

NS_BEGIN(Engine)
class ShaderCom;
class Texture;
class VIBufferCom_PointParticle;
NS_END

NS_BEGIN(Client)

class [[deprecated("절.대.컴.퓨.트.셰.이.더.를.사.용.해 | Use ComputePointParticleEffect for compute sprite particles.")]] PointParticleEffect final : public GameObject
{
    GENERATED_GAMEOBJECT(PointParticleEffect, ObjectType::Effect)

public:
    struct [[deprecated("Use ComputePointParticleEffectDesc for compute sprite particles.")]] PointParticleEffectDesc : public GameObject::GAMEOBJECT_DESC
    {
        PointParticleDrawMode drawMode{ PointParticleDrawMode::Direct }; // 이 effect clone의 draw 제출 방식.

        string textureId{ "Texture_Effect_DefaultTexture" };      // Required.textureId에서 내려온 main particle texture id.
        string blendMode{ "AlphaBlend" };                         // effect shader/render path가 해석할 blend mode 단서.
        string opacitySource{ "Alpha" };                          // texture에서 opacity를 뽑을 채널/방식.
        Vec4 tint{ 1.f, 1.f, 1.f, 1.f };                          // texture sample에 곱할 material tint.
        float intensity{ 1.f };                                   // sprite 색상 밝기/발광감을 조절하는 effect 계수.
        float opacityPower{ 1.f };                                // opacity를 선명하게 만들기 위한 effect sharpening 계수.
        string noiseTextureId{};                                  // 선택 noise texture runtime id. 비어 있으면 사용하지 않는다.
        float noiseStrength{ 0.f };                               // noise가 opacity/color modulation에 끼치는 effect 강도.
        uint32 subUVRows{ 1 };                                    // flipbook grid 기록용 row 수.
        uint32 subUVCols{ 1 };                                    // flipbook grid 기록용 column 수.
        PointParticleSubUVFrameOverLifeDesc subUVFrameOverLife{}; // 수명 진행률 기준 SubUV frame 선택 규칙이다.
        PointParticleSizeByLifeDesc sizeByLife{};                 // 수명 진행률 기준 크기 배율 규칙이다.

        Vec3 center{ 0.f, 2.f, 0.f };             // 이 effect clone의 스폰 중심.
        uint32 instanceCount{ 64 };               // 이 effect clone이 생성할 particle 수.
        Vec2 scale{ 0.15f, 0.35f };               // 이 effect clone의 랜덤 scale 범위.
        Vec3 pivot{ 0.f, 2.f, 0.f };              // 이 effect clone의 퍼짐 기준점.
        Vec2 speed{ 0.5f, 1.5f };                 // 이 effect clone의 랜덤 속도 범위.
        Vec2 lifeTime{ 1.f, 2.f };                // 이 effect clone의 랜덤 생존 시간 범위.
        Vec4 startColorMin{ 1.f, 1.f, 1.f, 1.f }; // Initial Color에서 내려온 시작 색상/알파 하한.
        Vec4 startColorMax{ 1.f, 1.f, 1.f, 1.f }; // Initial Color에서 내려온 시작 색상/알파 상한.
        Vec4 endColorMin{ 1.f, 1.f, 1.f, 1.f };   // Color Over Life에서 내려온 종료 색상/알파 하한.
        Vec4 endColorMax{ 1.f, 1.f, 1.f, 1.f };   // Color Over Life에서 내려온 종료 색상/알파 상한.
        bool loop{ true };                        // true면 생존 시간이 끝난 particle을 재생성한다.
    };

public:
    PointParticleEffect(const ComPtr<Device>& device, const ComPtr<Context>& context);
    PointParticleEffect(const PointParticleEffect& prototype);
    ~PointParticleEffect() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

private:
    static constexpr const char* kFallbackTextureId{ "Texture_Effect_DefaultTexture" };
    static constexpr const char* kEffectSpriteShaderId{ "Shader_EffectSprite" };
    static constexpr int kOpacitySourceAlpha{ 0 };
    static constexpr int kOpacitySourceRed{ 1 };
    static constexpr int kOpacitySourceLuminance{ 2 };

private:
    Shared<ShaderCom> _shader{};
    Shared<Texture> _texture{};
    Shared<Texture> _noiseTexture{};
    Shared<VIBufferCom_PointParticle> _viBuffer{};

private:
    PointParticleEffectDesc _desc{};                // clone 생성 시 전달받은 Stage2 effect 규칙.
    int _opacitySourceIndex{ kOpacitySourceAlpha }; // shader opacity source selector.

private:
    HRESULT Ready_Components();
    HRESULT Ready_TextureComponent(string& resolvedTextureId);
    HRESULT Ready_ShaderComponent();
    HRESULT Ready_NoiseTextureComponent(const string& resolvedTextureId);
    HRESULT Bind_ShaderResources();
    HRESULT Bind_MaterialResources();
    int Resolve_OpacitySourceIndex() const;
    PointParticleDesc Make_PointParticleDesc() const;

public:
    static Shared<PointParticleEffect> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
