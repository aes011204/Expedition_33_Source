#pragma once
#include "Client_Defines.h"
#include "EffectEmitter.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)
class ShaderCom;
class Texture;
class VIBufferCom_ComputePointParticle;
NS_END

NS_BEGIN(Client)

class ComputeSpriteEmitter final : public EffectEmitter
{
    GENERATED_GAMEOBJECT(ComputeSpriteEmitter, ObjectType::EffectEmitter);

public:
    ComputeSpriteEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ComputeSpriteEmitter(const ComputeSpriteEmitter& prototype);
    ~ComputeSpriteEmitter() override;

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
    ComputeSpriteEmitterDesc _desc{};               // clone 생성 시 전달받은 compute sprite emitter 규칙이다.
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
    void Sync_WorldCenterToComputeBuffer();                         // emitter transform world position을 compute center에 반영한다.

protected:
    void On_EffectTransformSynced() override;                       // root/local-space sync 직후 compute center를 즉시 갱신한다.

public:
    static Shared<ComputeSpriteEmitter> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
