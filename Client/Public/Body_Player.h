#pragma once
#include "PlayableParts.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
class ComputeShaderCom;
class AnimComputeRuntime;
NS_END

NS_BEGIN(Client)


class Body_Player final : public PlayableParts
{
    GENERATED_GAMEOBJECT(Body_Player, ObjectType::PartObject)
public:
	typedef struct tagBodyPlayerDesc final : public PlayableParts::PLAYABLE_PART_DESC
	{
	} BODY_PLAYER_DESC;

public:
	Body_Player(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Body_Player(const Body_Player& prototype);
	~Body_Player() override = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* arg) override;
	virtual void    Priority_Update(float fTimeDelta) override;
	virtual void    Update(float fTimeDelta) override;
	virtual void    Late_Update(float fTimeDelta) override;
	virtual HRESULT Render() override;
    virtual HRESULT Render_Shadow() override;

private:
    HRESULT Ready_Components(const PLAYABLE_PART_DESC& desc) override;

private:
    Shared<ShaderCom>               _computeRenderShaderCom;
    Shared<ModelCom>                _headModelCom;

    int32 _rootBoneIndex{ -1 };
    bool _useComputeSkinning = false;


public:
	static Shared<Body_Player> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;

public:
    enum class PlayerWorldAnimation
    {
        Pl_Idle_0, Pl_Idle_1, Pl_Idle_2, Pl_Idle_3, Pl_Idle_4,

        Pl_PreWalk, Pl_Walk, Pl_EndWalk,
        Pl_PreRun, Pl_Run, Pl_EndRun,

        Pl_PreSprint, Pl_Sprint, Pl_EndSprint,

        Pl_JumpWalk,
        Pl_JumpRun,
        Pl_Fall,
        Pl_Land,

        Pl_Explo,
    };

    enum class PlayerBattleAnimation
    {
        
    };
};

NS_END
