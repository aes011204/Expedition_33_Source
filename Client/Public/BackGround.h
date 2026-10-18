#pragma once

#include "Client_Defines.h"
#include "UIObject.h"

NS_BEGIN(Engine)
class ShaderCom;
class Texture;
class VIBuffer_Rect;
NS_END

NS_BEGIN(Client)

class BackGround final : public UIObject
{
    GENERATED_GAMEOBJECT(BackGround, ObjectType::UI)

public:
	typedef struct tagBackGroundDesc final : public UIObject::UI_DESC
	{
		
	}BACKGROUND_DESC;
public:
	BackGround(const ComPtr<Device>& device, const ComPtr<Context>& context);
	BackGround(const BackGround& prototype);
	virtual ~BackGround() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* arg) override;
	virtual void    Priority_Update(float timeDelta) override;
	virtual void    Update(float timeDelta) override;
	virtual void    Late_Update(float timeDelta) override;
	virtual HRESULT Render() override;

protected:
	Shared<ShaderCom>       _shader     = { nullptr };
	Shared<VIBuffer_Rect>   _vibuffer   = { nullptr };
	Shared<Texture>         _texture    = { nullptr };

protected:
	HRESULT Ready_Components();


public:
	static Shared<BackGround> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;
};

NS_END

