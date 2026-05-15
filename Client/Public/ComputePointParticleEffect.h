#pragma once
#include "Client_Defines.h"
#include "GameObject.h"
#include "Particle_Types.h"

NS_BEGIN(Engine)
class ShaderCom;
class Texture;
class VIBufferCom_ComputePointParticle;
NS_END

NS_BEGIN(Client)

class ComputePointParticleEffect final : public GameObject
{
    GENERATED_GAMEOBJECT(ComputePointParticleEffect, ObjectType::Effect);

public:
    struct ComputePointParticleEffectDesc : public GameObject::GAMEOBJECT_DESC
    {
        PointParticleDrawMode drawMode{ PointParticleDrawMode::DrawIndexedInstancedIndirect }; // compute sprite effect의 정식 draw 제출 방식.

        string textureId{ "Texture_Effect_DefaultTexture" }; // Required.material에서 내려온 main particle texture id.
        string blendMode{ "AlphaBlend" };                    // effect material bridge 기록값. 현재 pass 선택은 바꾸지 않는다.
        string opacitySource{ "Alpha" };                     // texture에서 opacity를 뽑을 채널/방식.
        Vec4 tint{ 1.f, 1.f, 1.f, 1.f };                     // texture sample에 곱할 material tint.
        float intensity{ 1.f };                              // sprite 색상 밝기/발광감을 조절하는 effect 계수.
        float opacityPower{ 1.f };                           // opacity를 선명하게 만들기 위한 effect sharpening 계수.
        string noiseTextureId{};                             // 선택 noise texture runtime id. 비어 있으면 사용하지 않는다.
        float noiseStrength{ 0.f };                          // noise가 opacity/color modulation에 끼치는 effect 강도.
        uint32 subUVRows{ 1 };                               // flipbook grid row 수.
        uint32 subUVCols{ 1 };                               // flipbook grid column 수.
        PointParticleSubUVFrameOverLifeDesc subUVFrameOverLife{}; // 수명 진행률 기준 SubUV frame 선택 규칙이다.
        PointParticleSizeByLifeDesc sizeByLife{};            // 수명 진행률 기준 크기 배율 규칙이다.

        Vec3 center{ 0.f, 2.f, 0.f };             // 이 effect clone의 스폰 중심.
        uint32 instanceCount{ 96 };               // 이 effect clone이 생성할 particle 수.
        Vec2 scale{ 0.12f, 0.28f };               // compute payload가 사용할 랜덤 scale 범위.
        Vec2 lifeTime{ 1.f, 2.f };                // compute payload가 사용할 생존 시간 범위.
        Vec4 startColorMin{ 1.f, 1.f, 1.f, 1.f }; // Initial Color에서 내려온 시작 색상/알파 하한.
        Vec4 startColorMax{ 1.f, 1.f, 1.f, 1.f }; // Initial Color에서 내려온 시작 색상/알파 상한.
        Vec4 endColorMin{ 1.f, 1.f, 1.f, 1.f };   // Color Over Life에서 내려온 종료 색상/알파 하한.
        Vec4 endColorMax{ 1.f, 1.f, 1.f, 1.f };   // Color Over Life에서 내려온 종료 색상/알파 상한.
    };

public:
    ComputePointParticleEffect(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ComputePointParticleEffect(const ComputePointParticleEffect& prototype);
    ~ComputePointParticleEffect() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

private: //## Static Values
    static constexpr const char* kFallbackTextureId{ "Texture_Effect_DefaultTexture" };
    static constexpr const char* kEffectSpriteShaderId{ "Shader_EffectSprite" };
    static constexpr int kOpacitySourceAlpha{ 0 };
    static constexpr int kOpacitySourceRed{ 1 };
    static constexpr int kOpacitySourceLuminance{ 2 };

private: //## Data::Components
    Shared<ShaderCom> _shader{};                          // EffectSprite material effect shader.
    Shared<Texture> _texture{};                           // compute sprite effect main texture.
    Shared<Texture> _noiseTexture{};                      // 선택 noise texture. 없으면 material noise를 비활성화한다.
    Shared<VIBufferCom_ComputePointParticle> _viBuffer{}; // compute output을 instance draw로 제출하는 검증 버퍼.

private: //## Data::Desc
    ComputePointParticleEffectDesc _desc{};         // clone 생성 시 전달받은 Stage3 effect 규칙.
    int _opacitySourceIndex{ kOpacitySourceAlpha }; // shader opacity source selector.

private: //## Helper::Setup
    HRESULT Ready_Components();                                              // effect에 필요한 shader/texture/compute buffer component를 준비한다.
    HRESULT Ready_TextureComponent(string& outResolvedTextureId);            // compute sprite main texture를 준비하고 fallback 결과를 기록한다.
    HRESULT Ready_ShaderComponent();                                         // compute sprite effect가 사용할 EffectSprite shader를 준비한다.
    HRESULT Ready_NoiseTextureComponent(const string& outResolvedTextureId); // 선택 noise texture를 준비하고 없으면 noise path를 비활성화한다.

private: //## Helper::Render
    HRESULT Bind_ShaderResources();         // EffectSprite shader에 camera/texture resource를 바인딩한다.
    HRESULT Bind_MaterialResources();       // EffectSprite material 파라미터를 shader에 바인딩한다.
    int Resolve_OpacitySourceIndex() const; // authoring opacitySource 문자열을 shader selector 값으로 변환한다.

private: //## Helper::Desc
    ComputePointParticleDesc Make_ComputePointParticleDesc() const; // compute sprite effect용 particle 규칙을 만든다.

public:
    static Shared<ComputePointParticleEffect> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
