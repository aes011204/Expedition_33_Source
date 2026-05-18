#pragma once
#include "PlayableParts.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
NS_END

NS_BEGIN(Client)

class Head_Player final : public PlayableParts
{
    GENERATED_GAMEOBJECT(Head_Player, ObjectType::PartObject)

public:
	typedef struct tagBodyPlayerDesc final : public PLAYABLE_PART_DESC
	{
	} HEAD_PLAYER_DESC;

public:
	Head_Player(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Head_Player(const Head_Player& prototype);
	~Head_Player() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void    Priority_Update(float fTimeDelta) override;
	void    Update(float timeDelta) override;
	void    Late_Update(float timeDelta) override;
	HRESULT Render() override;
    HRESULT Render_Shadow() override;

public:
    void Initialize_RigidAttachmentFromLeaderPose(
        const PlayableParts* leaderPart,
        const Matrix* rigidSocketMatrix);

private:
    Matrix _headOffsetMatrix{};

    const Matrix* _pendingRigidSocketMatrix{ nullptr };
    bool _pendingRigidAttachmentCalibration{ false };

public:
	static Shared<Head_Player> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Shared<GameObject> Clone(void* arg) override;
	void Free() override;
};

NS_END
