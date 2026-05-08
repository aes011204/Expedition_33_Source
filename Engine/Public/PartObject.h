#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)

class ENGINE_DLL PartObject abstract : public GameObject
{
public:
	typedef struct tagPartObjectDesc : public GameObject::GAMEOBJECT_DESC
	{
		const Matrix* parentMatrix = { nullptr };
	}PARTOBJECT_DESC;
public:
	PartObject(const ComPtr<Device>& device, const ComPtr<Context>& context);
	PartObject(const PartObject& prototype);
	virtual ~PartObject() = default;
public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* arg);
	virtual void Priority_Update(float timeDelta);
	virtual void Update(float timeDelta);
	virtual void Late_Update(float timeDelta);
	virtual HRESULT Render();


protected:
	const Matrix*			_parentMatrix = { nullptr };
	XMFLOAT4X4				_combinedWorldMatrix = {};

protected:
	void Update_CombinedWorldMatrix(FXMMATRIX childMatrix) {
		const Matrix parentMatrix = (nullptr != _parentMatrix) ? *_parentMatrix : Matrix::Identity;
		const Matrix childWorld = childMatrix;
		const Matrix combinedWorld = childWorld * parentMatrix;
		XMStoreFloat4x4(&_combinedWorldMatrix, combinedWorld);
	}

public:
	virtual Shared<GameObject> Clone(void* arg) = 0;
	virtual void Free() override;
};

NS_END
