#include "PartObject.h"

PartObject::PartObject(ComPtr<Device> device, ComPtr<Context> context)
	: GameObject { device, context }
{
}

PartObject::PartObject(const PartObject& Prototype)
	: GameObject { Prototype }
{
}

HRESULT PartObject::Initialize_Prototype()
{
	return S_OK;
}

HRESULT PartObject::Initialize(void* pArg)
{
	auto	pDesc = static_cast<PARTOBJECT_DESC*>(pArg);

	_parentMatrix = pDesc->pParentMatrix;

	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	XMStoreFloat4x4(&_combinedWorldMatrix, XMMatrixIdentity());

	return S_OK;
}

void PartObject::Priority_Update(float fTimeDelta)
{
}

void PartObject::Update(float fTimeDelta)
{
	
}

void PartObject::Late_Update(float fTimeDelta)
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

