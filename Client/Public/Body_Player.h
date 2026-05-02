#pragma once

#include "PartObject.h"

NS_BEGIN(Engine)
class Shader;
class Model;
NS_END

NS_BEGIN(Client)

class Body_Player final : public PartObject
{
    GENERATED_GAMEOBJECT(Body_Player, ObjectType::PartObject)

public:
	typedef struct tagBodyPlayerDesc final : public PartObject::PARTOBJECT_DESC
	{
		const uint32* pParentState = { nullptr };
	}BODY_PLAYER_DESC;

public:
	Body_Player(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Body_Player(const Body_Player& prototype);
	virtual ~Body_Player() = default;

public:
	const XMFLOAT4X4* Get_SocketBoneMatrixPtr(const char* pBoneName);
public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void    Priority_Update(float fTimeDelta) override;
	virtual void    Update(float fTimeDelta) override;
	virtual void    Late_Update(float fTimeDelta) override;
	virtual HRESULT Render() override;


private:
	Shared<Shader> _shader = { nullptr };
	Shared<Model>  _model = { nullptr };

private:
	const uint32* _parentState = { nullptr };

private:
	HRESULT Ready_Components();
	HRESULT Bind_ShaderResources();

public:
	static Shared<Body_Player> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;
};

NS_END
