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
	virtual ~PartObject() override = default;

public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* arg);
	virtual void    Priority_Update(float timeDelta);
	virtual void    Update(float timeDelta);
	virtual void    Late_Update(float timeDelta);
	virtual HRESULT Render();

public:
    const Matrix& Get_CombinedWorldMatrix() const { return _combinedWorldMatrix; }

protected:
	const Matrix*			_parentMatrix = { nullptr };
    Matrix				    _combinedWorldMatrix = {};

protected:
	virtual void Update_CombinedWorldMatrix(const Matrix& childMatrix) {
		const Matrix parentMatrix = (nullptr != _parentMatrix) ? *_parentMatrix : Matrix::Identity;
		const Matrix childWorld = childMatrix;
		const Matrix combinedWorld = childWorld * parentMatrix;
        _combinedWorldMatrix = combinedWorld;
	}

public:
	virtual Shared<GameObject> Clone(void* arg) override = 0;
	virtual void Free() override;
};

NS_END
