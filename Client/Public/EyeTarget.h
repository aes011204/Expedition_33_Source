#pragma once
#include "GameObject.h"

NS_BEGIN(Client)
class EffectCom;

class EyeTarget : public GameObject
{
    GENERATED_GAMEOBJECT(EyeTarget, ObjectType::PuzzleTarget)

    public:
        struct EYETARGE_DESC : GameObject::GAMEOBJECT_DESC
        {

        };

    public:
        EyeTarget(const ComPtr<Device>& device, const ComPtr<Context>& context);
        EyeTarget(const EyeTarget& prototype);
        virtual ~EyeTarget() override = default;

    public:
        virtual HRESULT Initialize_Prototype() override;
        virtual HRESULT Initialize(void* arg) override;
        virtual void    BeginPlay() override;
        virtual void    Priority_Update(float timeDelta) override;
        virtual void    Update(float timeDelta) override;
        virtual void    Late_Update(float timeDelta) override;
        virtual HRESULT Render() override;
        virtual HRESULT Render_Shadow() override;
        virtual void    From_Json(const json& data) override;

    public:
        bool            Try_Consume_EyeTargetHit();
        void            Reset_Target();

    private:
        HRESULT         Ready_EyeTargetCollider();
        HRESULT         Ready_EyeTargetEffectSetting();
        void            Apply_EyeTargetEffectSettings(bool preserveInspectorTransform = false);
        void            Play_EyeTargetEffect();

    public:
         Delegate<>          OnDestroyed;

    private:
         Shared<ModelCom>    _model = nullptr;
         Shared<ShaderCom>   _shader = nullptr;
         Shared<EffectCom>   _effectCom = nullptr;
         Shared<ColliderCom> _colliderCom = nullptr;
         float               _effectScale = 0.4f;
         bool                _isEnable = true;

    public:
        static Shared<EyeTarget> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
        virtual Shared<GameObject> Clone(void* arg) override;
        virtual void Free() override;
};
NS_END

