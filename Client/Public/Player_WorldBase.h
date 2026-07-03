#pragma once

#include "Client_Defines.h"
#include "Character_WorldBase.h"
#include "WorldInteractionSpot.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
class MovementCom;
class ColliderCom;
class FacialAnimationModifier;
class FacialClip;
NS_END

NS_BEGIN(Client)

class SkillTreeCom;
class PlayerStatCom;
class InputCom;
class PlayerControllerCom;
class PlayerStateMachineCom;
class EffectCom;
class Head_Player;
class FaceAccessory_Player;

class Body_Player;
class Head_Player;
class Hair_Player;

struct CostumePreset;
struct CostumePartLocalTransform;

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
    void    Post_PhysX_Update(float timeDelta) override;
	HRESULT Render() override;

    // 스테이터스 찍을 때, 게임 전체는 멈추는데
    // UI 창 안에서 프리뷰 플레이어들은 업데이트가 돌아야 애니메이션이 재생됨
    void    Update_StatusPreview(float timeDelta);
    void    Update_GameMenuFacialPreview(float timeDelta, bool enabled);
    void    Enter_GameMenuAnimation(uint32 introAnim, uint32 idleAnim);
    Vec3    Get_MovementVelocity() const;
    void    Start_SwitchDissolve(float durationSec, const string& noiseName);
    void    Set_PartyVisibilityAllowed(bool allowed);

    void    Set_WorldInteractionCandidate(const Shared<GameObject>& spot);
    void    Clear_WorldInteractionCandidate(const GameObject* spot);
    Shared<GameObject> Get_WorldInteractionCandidate() const;
    bool    Begin_WorldInteraction(const Shared<GameObject>& spot, const WorldInteractionSpot::WorldInteractionRequest& request);
    void    Clear_WorldInteractionRequest(const GameObject* spot);
    void    End_WorldInteraction();
    bool    Is_WorldInteracting() const;
    const WorldInteractionSpot::WorldInteractionRequest& Get_WorldInteractionRequest() const;

    HRESULT Apply_CostumePreset(const CostumePreset& preset);
    HRESULT Prewarm_CostumeVariants(); // 옷장 첫 교체 프레임 전에 모든 costume variant를 미리 만든다.

    float Get_ParkourLowProbeDistance() const { return _parkourLowProbeDistance; }
    float Get_ParkourHighProbeDistance() const { return _parkourHighProbeDistance; }
    const Vec3& Get_ParkourLowProbeCenter() const { return _parkourLowProbeCenter; }
    const Vec3& Get_ParkourHighProbeCenter() const { return _parkourHighProbeCenter; }
    bool Is_ParkourBlockedByCurrentLevel() const;

protected:
    HRESULT Ready_Components();
	HRESULT Ready_PartObjects();
    virtual HRESULT Ready_BodyModifiers(Body_Player* body, const string& costumePresetId)
    {
        UNREFERENCED_PARAMETER(body);
        UNREFERENCED_PARAMETER(costumePresetId);
        return S_OK;
    }

    virtual HeroId Get_HeroId() const { return HeroId::END; }

    virtual HairChainPhysicsDesc Get_HairChainPhysicsDesc() const
    {
        return HairChainPhysicsDesc{};
    }

protected:
    // 캐릭터별 파츠
    virtual const wchar_t* Get_BodyModelTag() const { return L"Model_LuneBody"; }
    virtual const wchar_t* Get_HeadModelTag() const { return L"Model_LuneHead"; }
    virtual const wchar_t* Get_HairModelTag() const { return L"Model_LuneHair"; }

    // 구스타브 수염
    virtual const wchar_t* Get_BeardModelTag() const { return nullptr; }
    virtual const wchar_t* Get_MustacheModelTag() const { return nullptr; }

    virtual const wchar_t* Get_GameMenuFacialClipPath() const;

private:
    void Handle_DebugWarpShortcuts();

    struct CostumePartTransform
    {
        Vec3 position{ Vec3::Zero };
        Vec3 rotation{ Vec3::Zero };
        Vec3 scale{ Vec3::One };
    };

    using CostumePartTransformMap = umap<wstring, CostumePartTransform>;

    struct CostumeVariantParts
    {
        Shared<Body_Player> body{};
        Shared<Head_Player> head{};
        Shared<Hair_Player> hair{};
        Shared<FaceAccessory_Player> beard{};
        Shared<FaceAccessory_Player> mustache{};
        Shared<FaceAccessory_Player> glasses{};
        CostumePartTransformMap baseTransforms{};
        CostumePartTransformMap presetTransforms{};
    };

    HRESULT Warmup_CostumeVariants();
    HRESULT Warmup_CostumeVariantParts(CostumeVariantParts& parts);
    HRESULT Ensure_CostumeVariantBuilt(const CostumePreset& preset);
    HRESULT Activate_CostumeVariant(const CostumePreset& preset);
    void Rebind_ActiveCostumeVariant(const string& presetId);
    void Capture_DefaultCostumePartTransformsIfNeeded();
    void Clear_AllCostumeVariants();
    CostumePartTransformMap Capture_CostumeVariantPartTransforms(const CostumeVariantParts& parts) const;
    CostumePartTransformMap Build_CostumePresetTransforms(
        const CostumePreset& preset,
        const CostumePartTransformMap& baseTransforms) const;
    void Apply_CostumePartTransforms(const CostumePartTransformMap& partTransforms);

    HRESULT Rebuild_CostumeParts(const CostumePreset& preset);
    void Clear_CostumeParts();
    CostumePartTransformMap Capture_CostumePartTransforms() const;
    const CostumePartTransformMap& Get_DefaultCostumePartTransforms();
    void Apply_CostumePartTransform(const wstring& partTag, const CostumePartTransformMap& partTransforms);
    void Apply_CostumePartTransform(const wstring& partTag, const CostumePartTransformMap& partTransforms,
        const CostumePartLocalTransform& fallbackTransform);
    void Reapply_CostumePartBaseMaterial(const wstring& partTag);
    void Reapply_CostumeBaseMaterials();

    Shared<Body_Player> Create_CostumeBodyVariant(const wstring& modelTag);
    Shared<Head_Player> Create_CostumeHeadVariant(const wstring& modelTag, const Body_Player* body);
    Shared<Hair_Player> Create_CostumeHairVariant(const wstring& modelTag, const Head_Player* head);
    Shared<FaceAccessory_Player> Create_CostumeFaceAccessoryVariant(const wstring& modelTag, const Head_Player* head);

    Body_Player* Add_CostumeBody(const wstring& modelTag);
    Head_Player* Add_CostumeHead(const wstring& modelTag, const Body_Player* body);
    HRESULT      Add_CostumeHair(const wstring& modelTag, const Head_Player* head);
    HRESULT      Add_CostumeFaceAccessory(const wstring& partTag, const wstring& modelTag, const Head_Player* head);

public:
    void        Set_CinematicDriven(bool cinematicDriven);

protected:
    Shared<MovementCom>             _movementCom;
    Shared<InputCom>                _inputCom;
    Shared<PlayerControllerCom>     _playerControllerCom;
    Shared<PlayerStateMachineCom>   _playerStateMachineCom;
    Shared<SkillTreeCom>            _skillTreeCom;
    Shared<PlayerStatCom>           _combatStatCom;
    Shared<EffectCom>               _effectCom;
    CostumePartTransformMap         _defaultCostumePartTransforms{};
    umap<string, CostumeVariantParts> _costumeVariantsByPreset{};
    string                          _activeCostumePresetId{};
    float                           _fallStateEnterAcc = 0.f; // 점프 입력 없는 짧은 공중 흔들림은 바로 Jump 상태로 소비하지 않기 위한 누적 시간
    float                           _airborneFallTimeAcc = 0.f; // Jump 상태 진입 전부터 실제 공중에 있던 시간을 착지 판정에 반영한다.
    bool                            _partyVisibilityAllowed = true; // 파티 해금 전에는 culling이 다시 켜도 월드 렌더를 막는다.
    bool                            _isCinematicDriven = false;
    float                           _parkourLowProbeDistance = 0.15f;
    float                           _parkourHighProbeDistance = 0.15f;
    Vec3                            _parkourLowProbeCenter = Vec3(0.f, 1.00f, 0.850f);
    Vec3                            _parkourHighProbeCenter = Vec3(0.f, 1.75f, 0.850f);

private:
    struct PlayerWorldInteractionContext
    {
        Weak<GameObject> candidateSpot{};
        Weak<GameObject> activeSpot{};
        WorldInteractionSpot::WorldInteractionRequest activeRequest{};
        bool isInteracting = false;
    };

    Shared<Engine::FacialClip>       _gameMenuFacialClip{};
    Shared<Engine::FacialAnimationModifier> _gameMenuFacialModifier{};
    bool                            _gameMenuFacialEnabled = false;
    PlayerWorldInteractionContext   _worldInteractionContext{};

public:
	static Shared<Player_WorldBase> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;

};

NS_END
