#pragma once

#include "Transform.h"

NS_BEGIN(Engine)

class ENGINE_DLL GameObject abstract : public Base
{
public:
	typedef struct tagGameObjectDesc : public Transform::TRANSFORM_DESC
	{
		uint32		iFlag = {};
	}GAMEOBJECT_DESC;
protected:
	GameObject(ComPtr<Device> device, ComPtr<Context> context);
	GameObject(const GameObject& Prototype);
	virtual ~GameObject() = default;

public:
	class Component* Get_Component(const wstring& strComponentTag);

public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* pArg);
	virtual void Priority_Update(float fTimeDelta);
	virtual void Update(float fTimeDelta);
	virtual void Late_Update(float fTimeDelta);
	virtual HRESULT Render();

protected:
	ComPtr<Device> _device = { nullptr };
	ComPtr<Context> _context = { nullptr };
	class Transform*		 _transformCom = { nullptr };

protected:
	map<const wstring, Shared<class Component>>		_components;

protected:
	HRESULT Add_Component(uint32 iPrototypeLevelIndex, const wstring& strPrototypeTag,
		const wstring& strComponentTag, Component** ppOut, void* pArg = nullptr);

public:
	virtual Shared<GameObject> Clone(void* pArg) = 0;
	virtual void Free() override;
};

NS_END



