#pragma once

#include "Client_Defines.h"
#include "PartObject.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
class AnimComputeRuntime;
NS_END

NS_BEGIN(Client)

class EffectCom;

class Monster_Part abstract : public PartObject
{
public:
    struct MONSTER_PART_DESC : public PartObject::PARTOBJECT_DESC
    {
        ModelType modelType{ ModelType::NonAnim };
        wstring modelTag{};
        bool allowMissingModel = false;
    };

public:
    Monster_Part(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Monster_Part(const Monster_Part& prototype);
    virtual ~Monster_Part() override = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    virtual void Priority_Update(float timeDelta) override;
    virtual void Update(float timeDelta) override;
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;
    virtual HRESULT Render_Shadow() override;

public:
    Shared<ModelCom> Get_Model() const { return _model; }
    void Set_Animation(uint32 animIndex, float blendDuration, bool isLoop = false);
    void Set_Animation(const string& animName, float blendDuration, bool isLoop = false, float startFrame = 0.f);
    bool Set_AnimationByState(uint32 stateIndex, float blendDuration, bool isLoop, float startFrame=0.f);
    bool Play_Animation(float timeDelta);
    bool Is_AnimationFinished() const;
    const RootMotionFrame& Get_RootTransformVelocity() const;
    const Matrix* Get_SocketBoneMatrixPtr(const string& boneName) const;
    const Matrix* Get_SocketBoneMatrixPtr(uint32 boneIndex) const;

protected:
    virtual HRESULT Ready_Components(const MONSTER_PART_DESC& desc);
    virtual HRESULT Bind_ShaderResources();
    virtual uint32 Decide_PassIndex(Shared<Material> mat);

protected:
    Shared<ShaderCom>           _shader = nullptr;
    Shared<ModelCom>            _model = nullptr;
    Shared<AnimComputeRuntime>  _animComputeRuntime = nullptr;
    Shared<EffectCom>           _partEffectCom = nullptr;

public:
    virtual Shared<GameObject> Clone(void* arg) override PURE;
    virtual void Free() override;
};

NS_END
