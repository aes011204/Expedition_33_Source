#pragma once
#include "PlayableParts.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
NS_END

NS_BEGIN(Client)

class Body_Player final : public PlayableParts
{
    GENERATED_GAMEOBJECT(Body_Player, ObjectType::PartObject)

public:
	typedef struct tagBodyPlayerDesc final : public PlayableParts::PLAYABLE_PART_DESC
	{
	}BODY_PLAYER_DESC;

public:
	Body_Player(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Body_Player(const Body_Player& prototype);
	virtual ~Body_Player() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* arg) override;
	virtual void    Priority_Update(float fTimeDelta) override;
	virtual void    Update(float fTimeDelta) override;
	virtual void    Late_Update(float fTimeDelta) override;
	virtual HRESULT Render() override;
    virtual HRESULT Render_Shadow() override;

public:
	static Shared<Body_Player> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;
};

NS_END
