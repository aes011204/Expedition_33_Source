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
    uint32 Get_AnimationIndex() const;
    float Get_AnimationProgress() const;
    bool Is_AnimationFinished() const;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    virtual void    Priority_Update(float timeDelta) override;
    virtual void    Update(float timeDelta) override;
    virtual void    Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;
    virtual HRESULT Render_Shadow() override;

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
