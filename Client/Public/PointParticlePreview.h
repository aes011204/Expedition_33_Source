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

class PointParticlePreview final : public GameObject
{
    GENERATED_GAMEOBJECT(PointParticlePreview, ObjectType::Effect)

public:
    struct PointParticlePreviewDesc : public GameObject::GAMEOBJECT_DESC
    {
        PointParticleDrawMode drawMode{ PointParticleDrawMode::Direct }; // 이 preview clone의 draw 제출 방식.
        Vec3 center{ 0.f, 2.f, 0.f };                                    // 이 preview clone의 스폰 중심.
        uint32 instanceCount{ 64 };                                      // 이 preview clone이 생성할 particle 수.
        Vec2 scale{ 0.15f, 0.35f };                                      // 이 preview clone의 랜덤 scale 범위.
        Vec3 pivot{ 0.f, 2.f, 0.f };                                     // 이 preview clone의 퍼짐 기준점.
        Vec2 speed{ 0.5f, 1.5f };                                        // 이 preview clone의 랜덤 속도 범위.
        Vec2 lifeTime{ 1.f, 2.f };                                       // 이 preview clone의 랜덤 생존 시간 범위.
        bool loop{ true };                                               // true면 생존 시간이 끝난 particle을 재생성한다.
    };

public:
    PointParticlePreview(const ComPtr<Device>& device, const ComPtr<Context>& context);
    PointParticlePreview(const PointParticlePreview& prototype);
    ~PointParticlePreview() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

private:
    Shared<ShaderCom> _shader{};
    Shared<Texture> _texture{};
    Shared<VIBufferCom_PointParticle> _viBuffer{};

private:
    PointParticlePreviewDesc _desc{}; // clone 생성 시 전달받은 Stage2 preview 규칙.

private:
    HRESULT Ready_Components();
    HRESULT Bind_ShaderResources();
    PointParticleDesc Make_PointParticleDesc() const;

public:
    static Shared<PointParticlePreview> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
