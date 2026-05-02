#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)

class ENGINE_DLL ContainerObject abstract : public GameObject
{
public:
	typedef struct tagContainerObjectDesc : public GameObject::GAMEOBJECT_DESC
	{
		
	}CONTAINEROBJECT_DESC;
public:
	ContainerObject(const ComPtr<Device>& device, const ComPtr<Context>& context);
	ContainerObject(const ContainerObject& prototype);
public:
	virtual ~ContainerObject() = default;
public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* arg);
    virtual void BeginPlay() override;

	virtual void Priority_Update(float timeDelta);
	virtual void Update(float timeDelta);
	virtual void Late_Update(float timeDelta);
	virtual HRESULT Render();

protected:
	map<const wstring, Shared<class PartObject>>				_partObjects;

protected:
	HRESULT Add_PartObject(uint32 prototypeLevelIndex, const wstring& prototypeTag, 
		const wstring& partTag, void* arg);

	PartObject* Find_PartObject(const wstring& partTag);

public:
	virtual Shared<GameObject> Clone(void* arg) = 0;
	virtual void Free() override;
};

NS_END



