#include "Channel.h"

#include "Model.h"
#include "Bone.h"

Channel::Channel()
{
}

HRESULT Channel::Initialize(const aiNodeAnim* pAIChannel, class Model* pModel)
{
	_boneIndex = 	pModel->Get_BoneIndex(pAIChannel->mNodeName.data);

	_numKeyFrames = max(pAIChannel->mNumScalingKeys, pAIChannel->mNumRotationKeys);
	_numKeyFrames = max(_numKeyFrames, pAIChannel->mNumPositionKeys);

	Vec3			vScale = Vec3::Zero;
	Quat			vRotation = Quat::Identity;
	Vec3			vTranslation = Vec3::Zero;

	
	for (size_t i = 0; i < _numKeyFrames; i++)
	{
		KEYFRAME			KeyFrame{};	

		if (pAIChannel->mNumScalingKeys > i)
		{
			memcpy(&vScale, &pAIChannel->mScalingKeys[i].mValue, sizeof(Vec3));
			KeyFrame.fTrackPosition = pAIChannel->mScalingKeys[i].mTime;
		}
		if (pAIChannel->mNumRotationKeys > i)
		{
			vRotation.x = pAIChannel->mRotationKeys[i].mValue.x;
			vRotation.y = pAIChannel->mRotationKeys[i].mValue.y;
			vRotation.z = pAIChannel->mRotationKeys[i].mValue.z;
			vRotation.w = pAIChannel->mRotationKeys[i].mValue.w;

			KeyFrame.fTrackPosition = pAIChannel->mRotationKeys[i].mTime;
		}
		if (pAIChannel->mNumPositionKeys > i)
		{
			memcpy(&vTranslation, &pAIChannel->mPositionKeys[i].mValue, sizeof(Vec3));
			KeyFrame.fTrackPosition = pAIChannel->mPositionKeys[i].mTime;
		}

		KeyFrame.vScale = vScale;
		KeyFrame.vRotation = vRotation;
		KeyFrame.vTranslation = vTranslation;

		_keyFrames.push_back(KeyFrame);
	}

	

    return S_OK;
}

void Channel::Update_TransformationMatrix(uint32* pCurrentKeyFrameIndex, float fCurrentTrackPosition, const vector<Shared<class Bone>>& Bones)
{
	if (0.f == fCurrentTrackPosition)
		(*pCurrentKeyFrameIndex) = 0;

	KEYFRAME		LastKeyFrame = _keyFrames.back();

	XMVECTOR		vScale, vRotation, vTranslation;


	if (fCurrentTrackPosition >= LastKeyFrame.fTrackPosition) /*  마지막 키프레임의 상태를 띈다. */
	{
		vScale = XMLoadFloat3(&LastKeyFrame.vScale);
		vRotation = XMLoadFloat4(&LastKeyFrame.vRotation);
		vTranslation = XMVectorSetW(XMLoadFloat3(&LastKeyFrame.vTranslation), 1.f);

	}
	else /* 사이 키프레임으로 보간한다. */
	{
		while (fCurrentTrackPosition >= _keyFrames[(*pCurrentKeyFrameIndex) + 1].fTrackPosition)
			++(*pCurrentKeyFrameIndex);

		float		fRatio = (fCurrentTrackPosition - _keyFrames[(*pCurrentKeyFrameIndex)].fTrackPosition) / 
			(_keyFrames[(*pCurrentKeyFrameIndex) + 1].fTrackPosition - _keyFrames[(*pCurrentKeyFrameIndex)].fTrackPosition);

		XMVECTOR		vLeftScale, vRightScale;
		XMVECTOR		vLeftRotation, vRightRotation;
		XMVECTOR		vLeftTranslation, vRightTranslation;

		vLeftScale = XMLoadFloat3(&_keyFrames[(*pCurrentKeyFrameIndex)].vScale);
		vRightScale = XMLoadFloat3(&_keyFrames[(*pCurrentKeyFrameIndex) + 1].vScale);
		vScale = XMVectorLerp(vLeftScale, vRightScale, fRatio);

		vLeftRotation = XMLoadFloat4(&_keyFrames[(*pCurrentKeyFrameIndex)].vRotation);
		vRightRotation = XMLoadFloat4(&_keyFrames[(*pCurrentKeyFrameIndex) + 1].vRotation);
		vRotation = XMQuaternionSlerp(vLeftRotation, vRightRotation, fRatio);

		vLeftTranslation = XMVectorSetW(XMLoadFloat3(&_keyFrames[(*pCurrentKeyFrameIndex)].vTranslation), 1.f);
		vRightTranslation = XMVectorSetW(XMLoadFloat3(&_keyFrames[(*pCurrentKeyFrameIndex) + 1].vTranslation), 1.f);
		vTranslation = XMVectorLerp(vLeftTranslation, vRightTranslation, fRatio);
	}

	// XMMATRIX		BoneTransformationMatrix = XMMatrixScaling(vScale) * XMMatrixRotationQuaternion(vRotation) * XMMatrixTranslation(vTranslation);
	XMMATRIX		BoneTransformationMatrix = XMMatrixAffineTransformation(vScale, XMVectorSet(0.f, 0.f, 0.f, 1.f), vRotation, vTranslation);

	Bones[_boneIndex]->Update_TransformationMatrix(BoneTransformationMatrix);
}

Shared<Channel> Channel::Create(const aiNodeAnim* pAIChannel, class Model* pModel)
{
	Shared<Channel> pInstance = MakeSharedFromRaw(new Channel());

	if (FAILED(pInstance->Initialize(pAIChannel, pModel)))
	{
		MSG_BOX("Failed to Created : Channel");
		Safe_Release(pInstance);
	}
	return pInstance;
}
void Channel::Free()
{
    __super::Free();

	_keyFrames.clear();

}
