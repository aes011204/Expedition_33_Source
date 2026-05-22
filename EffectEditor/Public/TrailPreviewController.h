#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)
class ModelCom;
NS_END

NS_BEGIN(Client)
class PB_Gustave;
NS_END

NS_BEGIN(EffectEditor)

class TrailPreviewWeaponObject;

class TrailPreviewController final : public GameObject
{
public:
    struct PreviewDesc : public GameObject::GAMEOBJECT_DESC
    {
        Shared<PB_Gustave> gustave{};
        Shared<TrailPreviewWeaponObject> weapon{};
    };

public:
    TrailPreviewController(const ComPtr<Device>& device, const ComPtr<Context>& context);
    TrailPreviewController(const TrailPreviewController& prototype);
    ~TrailPreviewController() override = default;

public:
    HRESULT Initialize_Prototype() override { return S_OK; }
    HRESULT Initialize(void* arg) override;
    void Priority_Update(float timeDelta) override;
    void Update(float timeDelta) override;

private:
    enum class PreviewAnimationMode
    {
        Rest,
        Attack,
    };

private:
    static constexpr const char* kAttackAnimationName{ "Gustave_Noah_Skill_Blitz" };
    static constexpr const char* kRestAnimationName{ "Gustave_Anim_Noah_Idle_World_FeetFixed" };

private:
    Weak<PB_Gustave> _gustave{};
    Weak<TrailPreviewWeaponObject> _weapon{};
    PreviewAnimationMode _currentMode{ PreviewAnimationMode::Rest };
    bool _animationInitialized{ false };

private:
    void Set_PlayerVisible(bool visible);
    void Set_WeaponVisible(bool visible);
    void Reset_PreviewPose();
    Shared<ModelCom> Get_BodyModel() const;
    void Ensure_AnimationMode(PreviewAnimationMode mode);
    bool Has_Animation(const Shared<ModelCom>& model, const char* animationName) const;

public:
    static Shared<TrailPreviewController> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
