#pragma once

#include "Client_Defines.h"
#include "Character_WorldBase.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
class MovementCom;
class ColliderCom;
NS_END

NS_BEGIN(Client)

class SkillTreeCom;
class PlayerStatCom;
class InputCom;
class PlayerControllerCom;
class PlayerStateMachineCom;

class Player_WorldBase : public Character_WorldBase
{
public:
	Player_WorldBase(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Player_WorldBase(const Player_WorldBase& prototype);
	virtual ~Player_WorldBase() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;

	void    Priority_Update(float timeDelta) override;
	void    Update(float timeDelta) override;
	void    Late_Update(float timeDelta) override;
	HRESULT Render() override;

    // 스테이터스 찍을 때, 게임 전체는 멈추는데
    // UI 창 안에서 프리뷰 플레이어들은 업데이트가 돌아야 애니메이션이 재생됨
    void    Update_StatusPreview(float timeDelta);
    void    Enter_GameMenuAnimation(uint32 introAnim, uint32 idleAnim);
    Vec3    Get_MovementVelocity() const;
    void    Start_SwitchDissolve(float durationSec, const string& noiseName); // TEMP_ERASE: DISSOLVE 예시 함수

protected:
    HRESULT Ready_Components();
	HRESULT Ready_PartObjects();

    virtual HairChainPhysicsDesc Get_HairChainPhysicsDesc() const
    {
        return HairChainPhysicsDesc{};
    }

protected:
    // 캐릭터별 파츠
    virtual const wchar_t* Get_BodyModelTag() const { return L"Model_LuneBody"; }
    virtual const wchar_t* Get_HeadModelTag() const { return L"Model_LuneHead"; }
    virtual const wchar_t* Get_HairModelTag() const { return L"Model_LuneHair"; }

protected:
    Shared<MovementCom>             _movementCom;
    Shared<InputCom>                _inputCom;
    Shared<PlayerControllerCom>     _playerControllerCom;
    Shared<PlayerStateMachineCom>   _playerStateMachineCom;
    Shared<SkillTreeCom>            _skillTreeCom;
    Shared<PlayerStatCom>           _combatStatCom;


public:
	static Shared<Player_WorldBase> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;

};

NS_END

