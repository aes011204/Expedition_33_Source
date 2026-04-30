#pragma once

#include "Engine_Define.h"

NS_BEGIN(Engine)

class ENGINE_DLL Base abstract : public enable_shared_from_this<Base>
{
protected:
	Base();

public:
	virtual ~Base() = default;

public:
	template<typename T = Base>
	Shared<T> GetSharedPtr() { return static_pointer_cast<T>(shared_from_this()); }

public:
	virtual void Free() {}

};

NS_END
