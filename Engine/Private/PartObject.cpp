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

    _combinedWorldMatrix = Matrix::Identity;

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

void PartObject::Post_PhysX_Update(float timeDelta)
{
}

HRESULT PartObject::Render()
{
	return S_OK;
}

Matrix PartObject::Get_GizmoWorldMatrix() const
{
    const Matrix parentMatrix = (nullptr != _parentMatrix) ? *_parentMatrix : Matrix::Identity;

    if (_transformCom == nullptr)
        return parentMatrix;

    return _transformCom->Get_WorldMatrix() * parentMatrix;
}

bool PartObject::Apply_GizmoWorldMatrix(const Matrix& gizmoWorldMatrix)
{
    if (_transformCom == nullptr)
        return false;

    Matrix parentMatrix = (nullptr != _parentMatrix) ? *_parentMatrix : Matrix::Identity;
    Matrix localWorldMatrix = gizmoWorldMatrix * parentMatrix.Invert();

    Vec3 scale = Vec3::One;
    Quat rotation = Quat::Identity;
    Vec3 translation = Vec3::Zero;

    if (!localWorldMatrix.Decompose(scale, rotation, translation))
        return false;

    _transformCom->Set_WorldPosition(translation);
    _transformCom->Set_RotationQuaternion(rotation);
    _transformCom->Set_Scale(scale);
    return true;
}

void PartObject::Free()
{
	__super::Free();



}

