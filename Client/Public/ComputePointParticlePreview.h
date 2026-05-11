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

class ComputePointParticlePreview final : public GameObject
{
    GENERATED_GAMEOBJECT(ComputePointParticlePreview, ObjectType::Effect);

public:
    struct ComputePointParticlePreviewDesc : public GameObject::GAMEOBJECT_DESC
    {
        PointParticleDrawMode drawMode{ PointParticleDrawMode::Direct }; // 이 preview clone의 compute draw 제출 방식.
        Vec3 center{ 0.f, 2.f, 0.f };                                    // 이 preview clone의 스폰 중심.
    };

public:
    ComputePointParticlePreview(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ComputePointParticlePreview(const ComputePointParticlePreview& prototype);
    ~ComputePointParticlePreview() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

private: //## Data::Components
    Shared<ShaderCom> _shader{};                          // billboard particle shader.
    Shared<Texture> _texture{};                           // 화면 확인용 particle texture.
    Shared<VIBufferCom_ComputePointParticle> _viBuffer{}; // compute output을 instance draw로 제출하는 검증 버퍼.

private: //## Data::Desc
    ComputePointParticlePreviewDesc _desc{}; // clone 생성 시 전달받은 Stage3 preview 규칙.

private: //## Helper::Setup
    HRESULT Ready_Components(); // preview에 필요한 shader/texture/compute buffer component를 준비한다.

private: //## Helper::Render
    HRESULT Bind_ShaderResources(); // billboard shader에 camera/texture resource를 바인딩한다.

private: //## Helper::Desc
    ComputePointParticleDesc Make_ComputePointParticleDesc() const; // Smoke test용 compute particle 규칙을 만든다.

public:
    static Shared<ComputePointParticlePreview> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
