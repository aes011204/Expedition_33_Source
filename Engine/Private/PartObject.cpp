#include "PartObject.h"

PartObject::PartObject(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: GameObject { device, context }
{
}

PartObject::PartObject(const PartObject& prototype)
	: GameObject { prototype }
{
}

HRESULT PartObject::Initialize_Prototype()
{
	return S_OK;
}

HRESULT PartObject::Initialize(void* arg)
{
	auto	desc = static_cast<PARTOBJECT_DESC*>(arg);

	_parentMatrix = (nullptr != desc) ? desc->parentMatrix : nullptr;

	if (FAILED(__super::Initialize(arg)))
		return E_FAIL;

	XMStoreFloat4x4(&_combinedWorldMatrix, XMMatrixIdentity());

	return S_OK;
}

void PartObject::Priority_Update(float timeDelta)
{
}

void PartObject::Update(float timeDelta)
{
	
}

void PartObject::Late_Update(float timeDelta)
{
}

HRESULT PartObject::Render()
{
	return S_OK;
}


void PartObject::Free()
{
	__super::Free();



}

