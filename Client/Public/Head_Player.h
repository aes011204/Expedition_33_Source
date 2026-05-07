#pragma once

#include "PartObject.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
NS_END

NS_BEGIN(Client)

class Head_Player final : public PartObject
{
    GENERATED_GAMEOBJECT(Head_Player, ObjectType::PartObject)

public:
	typedef struct tagBodyPlayerDesc final : public PartObject::PARTOBJECT_DESC
	{
		const uint32* pParentState = { nullptr };
	}BODY_PLAYER_DESC;

public:
	Head_Player(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Head_Player(const Head_Player& prototype);
	virtual ~Head_Player() = default;

public:
	const XMFLOAT4X4* Get_SocketBoneMatrixPtr(const char* pBoneName);
public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void    Priority_Update(float fTimeDelta) override;
	virtual void    Update(float fTimeDelta) override;
	virtual void    Late_Update(float fTimeDelta) override;
	virtual HRESULT Render() override;
    virtual HRESULT Render_Shadow() override;


private:
	Shared<ShaderCom> _shader = { nullptr };
	Shared<ModelCom>  _model = { nullptr };

private:
	const uint32* _parentState = { nullptr };

private:
	HRESULT Ready_Components();
	HRESULT Bind_ShaderResources();

public:
	static Shared<Head_Player> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;
};

NS_END
