#include "Channel.h"

#include "ModelCom.h"
#include "Bone.h"

Channel::Channel()
{
}

HRESULT Channel::Initialize(const aiNodeAnim* aiChannel, class ModelCom* model)
{
	_boneIndex = 	model->Get_BoneIndex(aiChannel->mNodeName.data);

	_numKeyFrames = max(aiChannel->mNumScalingKeys, aiChannel->mNumRotationKeys);
	_numKeyFrames = max(_numKeyFrames, aiChannel->mNumPositionKeys);

	Vec3			scale = Vec3::Zero;
	Quat			rotation = Quat::Identity;
	Vec3			translation = Vec3::Zero;

	
	for (size_t i = 0; i < _numKeyFrames; i++)
	{
		KEYFRAME			KeyFrame{};	

		if (aiChannel->mNumScalingKeys > i)
		{
			memcpy(&scale, &aiChannel->mScalingKeys[i].mValue, sizeof(Vec3));
			KeyFrame.trackPosition = static_cast<float>(aiChannel->mScalingKeys[i].mTime);
		}
		if (aiChannel->mNumRotationKeys > i)
		{
			rotation.x = aiChannel->mRotationKeys[i].mValue.x;
			rotation.y = aiChannel->mRotationKeys[i].mValue.y;
			rotation.z = aiChannel->mRotationKeys[i].mValue.z;
			rotation.w = aiChannel->mRotationKeys[i].mValue.w;

			KeyFrame.trackPosition = static_cast<float>(aiChannel->mRotationKeys[i].mTime);
		}
		if (aiChannel->mNumPositionKeys > i)
		{
			memcpy(&translation, &aiChannel->mPositionKeys[i].mValue, sizeof(Vec3));
			KeyFrame.trackPosition = static_cast<float>(aiChannel->mPositionKeys[i].mTime);
		}

		KeyFrame.scale = scale;
		KeyFrame.rotation = rotation;
		KeyFrame.translation = translation;

		_keyFrames.push_back(KeyFrame);
	}

	

    return S_OK;
}

void Channel::Update_TransformationMatrix(uint32* currentKeyFrameIndex, float currentTrackPosition, const vector<Shared<class Bone>>& bones)
{
	if (0.f == currentTrackPosition)
		(*currentKeyFrameIndex) = 0;

	KEYFRAME		LastKeyFrame = _keyFrames.back();

	XMVECTOR		scale, rotation, translation;


	if (currentTrackPosition >= LastKeyFrame.trackPosition) /*  마지막 키프레임의 상태를 띈다. */
	{
		scale = XMLoadFloat3(&LastKeyFrame.scale);
		rotation = XMLoadFloat4(&LastKeyFrame.rotation);
		translation = XMVectorSetW(XMLoadFloat3(&LastKeyFrame.translation), 1.f);

	}
	else /* 사이 키프레임으로 보간한다. */
	{
		while (currentTrackPosition >= _keyFrames[(*currentKeyFrameIndex) + 1].trackPosition)
			++(*currentKeyFrameIndex);

		float		ratio = (currentTrackPosition - _keyFrames[(*currentKeyFrameIndex)].trackPosition) / 
			(_keyFrames[(*currentKeyFrameIndex) + 1].trackPosition - _keyFrames[(*currentKeyFrameIndex)].trackPosition);

		XMVECTOR		leftScale, rightScale;
		XMVECTOR		leftRotation, rightRotation;
		XMVECTOR		leftTranslation, rightTranslation;

		leftScale = XMLoadFloat3(&_keyFrames[(*currentKeyFrameIndex)].scale);
		rightScale = XMLoadFloat3(&_keyFrames[(*currentKeyFrameIndex) + 1].scale);
		scale = XMVectorLerp(leftScale, rightScale, ratio);

		leftRotation = XMLoadFloat4(&_keyFrames[(*currentKeyFrameIndex)].rotation);
		rightRotation = XMLoadFloat4(&_keyFrames[(*currentKeyFrameIndex) + 1].rotation);
		rotation = XMQuaternionSlerp(leftRotation, rightRotation, ratio);

		leftTranslation = XMVectorSetW(XMLoadFloat3(&_keyFrames[(*currentKeyFrameIndex)].translation), 1.f);
		rightTranslation = XMVectorSetW(XMLoadFloat3(&_keyFrames[(*currentKeyFrameIndex) + 1].translation), 1.f);
		translation = XMVectorLerp(leftTranslation, rightTranslation, ratio);
	}

	// XMMATRIX		BoneTransformationMatrix = XMMatrixScaling(scale) * XMMatrixRotationQuaternion(rotation) * XMMatrixTranslation(translation);
	XMMATRIX		BoneTransformationMatrix = XMMatrixAffineTransformation(scale, XMVectorSet(0.f, 0.f, 0.f, 1.f), rotation, translation);

	bones[_boneIndex]->Update_TransformationMatrix(BoneTransformationMatrix);
}

Shared<Channel> Channel::Create(const aiNodeAnim* aiChannel, class ModelCom* model)
{
	auto instance = make_shared<Channel>();

	if (FAILED(instance->Initialize(aiChannel, model)))
	{
		MSG_BOX("Failed to Created : Channel");
		return nullptr;
	}
	return instance;
}
void Channel::Free()
{
    __super::Free();

	_keyFrames.clear();

}
