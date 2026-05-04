#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class GameObject;

class ENGINE_DLL Component abstract : public Base
{
public:
	Component(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Component(const Component& prototype);

public:
	virtual ~Component() = default;

public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* arg);
    virtual void    BeginPlay();

    virtual ClassReflectionInfo& Get_ReflectionInfo()
    {
        static ClassReflectionInfo emptyInfo{};
        return emptyInfo;
    }

public:
    Shared<GameObject>  Get_Owner() const { return _owner.lock(); }
    void                Set_Owner(Shared<GameObject> owner) { _owner = owner; }

protected:
	ComPtr<Device>  _device = { nullptr };
	ComPtr<Context> _context = { nullptr };

protected:
    Weak<GameObject> _owner;

public:
	virtual Shared<Component> Clone(void* arg) = 0;
	virtual void Free() override;
};

NS_END





