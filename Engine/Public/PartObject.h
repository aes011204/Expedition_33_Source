#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)

class ENGINE_DLL PartObject abstract : public GameObject
{
public:
	typedef struct tagPartObjectDesc : public GameObject::GAMEOBJECT_DESC
	{
		const XMFLOAT4X4* pParentMatrix = { nullptr };
	}PARTOBJECT_DESC;
public:
	PartObject(ComPtr<Device> device, ComPtr<Context> context);
	PartObject(const PartObject& Prototype);
	virtual ~PartObject() = default;
public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* pArg);
	virtual void Priority_Update(float fTimeDelta);
	virtual void Update(float fTimeDelta);
	virtual void Late_Update(float fTimeDelta);
	virtual HRESULT Render();


protected:
	const XMFLOAT4X4*		_parentMatrix = { nullptr };
	XMFLOAT4X4				_combinedWorldMatrix = {};

protected:
	void Update_CombinedWorldMatrix(FXMMATRIX ChildMatrix) {
		XMStoreFloat4x4(&_combinedWorldMatrix, ChildMatrix * XMLoadFloat4x4(_parentMatrix));
	}

public:
	virtual Shared<GameObject> Clone(void* pArg) = 0;
	virtual void Free() override;
};

NS_END
