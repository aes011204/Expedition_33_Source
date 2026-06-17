#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)
class ModelCom;
NS_END

NS_BEGIN(EffectEditor)

class TrailPreviewPlayerObject;

class TrailPreviewController final : public GameObject
{
public:
    struct PreviewDesc : public GAMEOBJECT_DESC
    {
        Shared<TrailPreviewPlayerObject> player{};
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

public:
    bool Try_GetAttackAnimationRatio(float& outRatio) const;

private:
    enum class PreviewAnimationMode
    {
        Rest,
        Attack,
    };

private:
    static constexpr auto kAttackAnimationName{ "Gustave_Noah_Skill_Blitz" };
    static constexpr auto kRestAnimationName{ "Gustave_Anim_Noah_Idle_World_FeetFixed" };

private:
    Weak<TrailPreviewPlayerObject> _player{};
    PreviewAnimationMode _currentMode{ PreviewAnimationMode::Rest };
    bool _animationInitialized{ false };

private:
    void Set_PlayerVisible(bool visible);
    void Reset_PreviewPose();
    Shared<ModelCom> Get_BodyModel() const;
    void Ensure_AnimationMode(PreviewAnimationMode mode);
    void Apply_AnimationSpeed(const Shared<ModelCom>& bodyModel) const;
    bool Has_Animation(const Shared<ModelCom>& model, const char* animationName) const;

public:
    static Shared<TrailPreviewController> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
