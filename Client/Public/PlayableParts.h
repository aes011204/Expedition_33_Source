#pragma once
#include "PartObject.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
NS_END

NS_BEGIN(Client)

class PlayableParts abstract : public PartObject
{
public:
    typedef struct tagBodyPlayerDesc : public PartObject::PARTOBJECT_DESC
    {
        ModelType       modelType{ ModelType::Anim };
        wstring         modelTag{};
        const Matrix*   socketMatrixPointer{ nullptr };
    } PLAYABLE_PART_DESC;

public:
    PlayableParts(const ComPtr<Device>& device, const ComPtr<Context>& context);
    PlayableParts(const PlayableParts& prototype);
    virtual ~PlayableParts() override = default;

public:
    const Matrix* Get_SocketBoneMatrixPtr(const string& boneName) const;
    const Matrix* Get_SocketBoneMatrixPtr(uint32 boneIndex) const;
    void Set_Animation(uint32 animIndex, float blendDuration, bool isLoop = false) const;
    void Set_Animation(uint32 animIndex, bool isLoop) const;
    bool Set_AnimationByState(uint32 stateIndex, float blendDuration, bool isLoop = false) const;
    bool Set_AnimationByState(uint32 stateIndex, bool isLoop) const;
    uint32 Get_AnimationIndex() const;
    uint32 Get_AnimationStateIndex() const;
    float Get_AnimationProgress() const;
    bool Is_AnimationBlending() const;
    bool Is_AnimationFinished() const;
    Vec3 Get_RootMotionPosition() const;

    void Set_AnimationSpeed(float speed) const;
    float Get_AnimationSpeed() const;

    Shared<ModelCom> Get_Model() const { return _modelCom; }

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    virtual void    Priority_Update(float timeDelta) override;
    virtual void    Update(float timeDelta) override;
    virtual void    Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;
    virtual HRESULT Render_Shadow() override;

    // 머리 붙이기
    void Set_LeaderPoseComponent(const PlayableParts* leaderPart) const;
    bool Has_LeaderPoseComponent() const;
    void Update_LeaderPoseMatrices() const;

protected:
    void Update_CombinedWorldMatrix(const Matrix& childMatrix) override;

protected:
    Matrix              _offsetMatrix{};
    const Matrix*       _socketMatrix{ nullptr };
    Shared<ShaderCom>   _shaderCom{ nullptr };
    Shared<ModelCom>    _modelCom{ nullptr };

protected:
    virtual HRESULT Ready_Components(const PLAYABLE_PART_DESC& desc);
    virtual HRESULT Bind_ShaderResources();

public:
    virtual Shared<GameObject> Clone(void* arg) override PURE;
    virtual void Free() override;
};

NS_END
