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
    Shared<ShaderCom> _shader = { nullptr };
    Shared<Texture> _texture = { nullptr };
    Shared<VIBufferCom_PointParticle> _vibuffer = { nullptr };

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
