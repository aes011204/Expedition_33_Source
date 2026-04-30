#pragma once

#include "Client_Defines.h"
#include "UIObject.h"

NS_BEGIN(Engine)
class Shader;
class Texture;
class VIBuffer_Rect;
NS_END

NS_BEGIN(Client)

class BackGround final : public UIObject
{
public:
	typedef struct tagBackGroundDesc final : public UIObject::UI_DESC
	{
		
	}BACKGROUND_DESC;
private:
	BackGround(ComPtr<Device> device, ComPtr<Context> context);
	BackGround(const BackGround& Prototype);
	virtual ~BackGround() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Priority_Update(float fTimeDelta) override;
	virtual void Update(float fTimeDelta) override;
	virtual void Late_Update(float fTimeDelta) override;
	virtual HRESULT Render() override;

protected:
	Shader* _shader = { nullptr };
	VIBuffer_Rect* _vibuffer = { nullptr };
	Texture* _texture = { nullptr };

protected:
	HRESULT Ready_Components();


public:
	static Shared<BackGround> Create(ComPtr<Device> device, ComPtr<Context> context);
	virtual Shared<GameObject> Clone(void* pArg) override;
	virtual void Free() override;
};

NS_END

