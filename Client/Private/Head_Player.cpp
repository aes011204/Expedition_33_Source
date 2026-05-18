#include "Head_Player.h"
#include "GameInstance.h"
#include "Material.h"
#include "Material_Types.h"
#include "Player_WorldBase.h"
#include "ModelCom.h"
#include "Debug_Profile.h"

Head_Player::Head_Player(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: PlayableParts{ device, context }
{
}

Head_Player::Head_Player(const Head_Player& prototype)
	: PlayableParts{ prototype }
{
}

HRESULT Head_Player::Initialize_Prototype()
{
	return PlayableParts::Initialize_Prototype();
}

HRESULT Head_Player::Initialize(void* arg)
{
	if (FAILED(PlayableParts::Initialize(arg)))
	{
        LOG_ERROR("[Head_Player] : Failed to Initialize");
        return S_OK;
	}

    _offsetMatrix = Matrix::Identity;

	return S_OK;
}

void Head_Player::Priority_Update(float fTimeDelta)
{
}

void Head_Player::Update(float timeDelta)
{
    Debug_Profile::Scope updateScope("Player::Head::Update");

    if (_pendingRigidAttachmentCalibration)
    {
        Debug_Profile::Scope profileScope("Player::Head::LeaderPose");

        if (Has_LeaderPoseComponent())
            Update_LeaderPoseMatrices();

        Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());
        const Matrix currentHeadWorld = _combinedWorldMatrix; 

        Matrix parentMatrix = Matrix::Identity;
        if (const Shared<GameObject> owner = Get_Owner())
        {
            if (const Shared<TransformCom> ownerTransform = owner->Get_Transform())
                parentMatrix = ownerTransform->Get_WorldMatrix();
        }
        else if (_parentMatrix != nullptr)
        {
            parentMatrix = *_parentMatrix;
        }

        const Matrix childMatrix = _transformCom->Get_WorldMatrix();
        const Matrix rigidSocketMatrix = (_pendingRigidSocketMatrix != nullptr)
            ? *_pendingRigidSocketMatrix
            : Matrix::Identity;

        const Matrix rigidBaseWorld = childMatrix * rigidSocketMatrix * parentMatrix;

        _offsetMatrix = currentHeadWorld * rigidBaseWorld.Invert();

        _socketMatrix = _pendingRigidSocketMatrix; // 이제부터는 rigid socket으로 붙인다.
        _pendingRigidSocketMatrix = nullptr;
        _pendingRigidAttachmentCalibration = false;

        Set_LeaderPoseComponent(nullptr); // 보정값을 얻었으니 LeaderPose는 끊는다.
        CHECK_FAILED(_modelCom->Prepare_BoneMatrixBuffers()); 

        Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());
        return;
    }

    if (Has_LeaderPoseComponent())
    {
        Debug_Profile::Scope profileScope("Player::Head::LeaderPose");
        Update_LeaderPoseMatrices(); // socket을 못 찾은 경우에는 기존 LeaderPose 경로로 유지한다.
    }

    Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix()); // 이후에는 rigid 부착 world만 따라간다.
}

void Head_Player::Late_Update(float timeDelta)
{
    __super::Late_Update(timeDelta);
}

HRESULT Head_Player::Render()
{
    Debug_Profile::Scope profileScope("Player::Head::Render");

    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    const uint32 numMeshes = static_cast<uint32>(_modelCom->Get_NumMeshes());
    for (uint32 i = 0; i < numMeshes; ++i)
    {
        const uint32 materialIndex = _modelCom->Get_MeshMaterialIndex(i);
        const Shared<Material> material = _modelCom->Get_Material(materialIndex);
        if (material == nullptr)
            continue;

        if (FAILED(_shaderCom->Bind_MaterialTextures(*material)))
            continue;

        if (FAILED(_shaderCom->Bind_CBufferData(material->Get_CB())))
            return E_FAIL;

        CHECK_FAILED(_modelCom->Bind_PreparedBoneMatrices(_shaderCom.get(), "g_BoneMatricesBuffer", i), E_FAIL);

        const uint32 passIndex = material->Get_PassIndex();
        if (FAILED(_shaderCom->Begin(passIndex)))
            return E_FAIL;

        _modelCom->Render(i);
    }

    return S_OK;
}

HRESULT Head_Player::Render_Shadow()
{
    return PlayableParts::Render_Shadow();
}

void Head_Player::Initialize_RigidAttachmentFromLeaderPose(const PlayableParts* leaderPart,
    const Matrix* rigidSocketMatrix)
{
    Set_LeaderPoseComponent(leaderPart);

    _socketMatrix = nullptr; // calibration 전에는 LeaderPose 결과를 그대로 받는다.
    _pendingRigidSocketMatrix = rigidSocketMatrix;
    _pendingRigidAttachmentCalibration = (rigidSocketMatrix != nullptr);
}

Shared<Head_Player> Head_Player::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Head_Player>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : Head_Player");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> Head_Player::Clone(void* arg)
{
    auto clone = make_shared<Head_Player>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : Head_Player");
        return nullptr;
    }

    return clone;
}

void Head_Player::Free()
{
	__super::Free();
}



