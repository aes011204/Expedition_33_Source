#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)

class ENGINE_DLL ContainerObject abstract : public GameObject
{
public:
	typedef struct tagContainerObjectDesc : public GameObject::GAMEOBJECT_DESC
	{
		
	}CONTAINEROBJECT_DESC;
protected:
	ContainerObject(ComPtr<Device> device, ComPtr<Context> context);
	ContainerObject(const ContainerObject& Prototype);
public:
	virtual ~ContainerObject() = default;
public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* pArg);
	virtual void Priority_Update(float fTimeDelta);
	virtual void Update(float fTimeDelta);
	virtual void Late_Update(float fTimeDelta);
	virtual HRESULT Render();

protected:
	map<const wstring, Shared<class PartObject>>				_partObjects;

protected:
	HRESULT Add_PartObject(uint32 iPrototypeLevelIndex, const wstring& strPrototypeTag, 
		const wstring& strPartTag, void* pArg);

	PartObject* Find_PartObject(const wstring& strPartTag);

public:
	virtual Shared<GameObject> Clone(void* pArg) = 0;
	virtual void Free() override;
};

NS_END



