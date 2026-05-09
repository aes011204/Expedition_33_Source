#pragma once
#include "Base.h"

NS_BEGIN(Client)

class State abstract : public Base
{
protected:
	State() = default;
	virtual ~State() override;

public:
	virtual HRESULT Initialize() { return S_OK; }
    virtual void Begin_Play() {};
    virtual bool EnterInvoke() { return true; }
	virtual void Update(float timeDelta) {}
	virtual void LateUpdate(float timeDelta) {}
	virtual void ExitInvoke() {}

public:
	virtual void Free() override;
};

NS_END
