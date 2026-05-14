#pragma once

#include "PlayableParts.h"
#include "HairChainPhysics.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
NS_END

NS_BEGIN(Client)

class Hair_Player final : public PlayableParts
{
    GENERATED_GAMEOBJECT(Hair_Player, ObjectType::PartObject)

public:
    typedef struct tagHairPlayerDesc final : public PlayableParts::PLAYABLE_PART_DESC
    {
    } HAIR_PLAYER_DESC;

public:
    Hair_Player(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Hair_Player(const Hair_Player& prototype);
    ~Hair_Player() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;
    void    Priority_Update(float timeDelta) override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;

private:
    Matrix _offsetMatrix{};

    HairChainPhysics        _hairPhysics{};
    HairChainPhysicsDesc    _hairPhysicsDesc{};
    bool                    _hairPhysicsReady = false;

public:
    static Shared<Hair_Player> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
