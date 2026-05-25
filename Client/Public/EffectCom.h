#pragma once

#include "Component.h"
#include "SwitchDissolveFeature.h"

NS_BEGIN(Engine)
class GameObject;
class ShaderCom;
NS_END

NS_BEGIN(Client)

class EffectCom : public Component
{
    GENERATED_COMPONENT(EffectCom);

private:
    struct AttachedEffect
    {
        Shared<GameObject> effectObject{};      // 이팩트 오브젝트
        wstring partTag{};                      // 어디 파츠에 붙일건지
        string socketName{};                    // 소켓 이름
        Vec3 localOffset{ Vec3::Zero };       // offset
    };

public:
    EffectCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ~EffectCom() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;
    void    Update(float timeDelta);

public:
    void    Start_SwitchDissolve(float durationSec, const string& noiseName);
    HRESULT Bind_DrawFeatures(ShaderCom* shader, uint32 supportFeatureMask);

    HRESULT Play_AttachedEffect(
        const wstring& effectName,
        const wstring& partTag,
        const string& socketName,
        const wstring& layerTag,
        const Vec3& localOffset = Vec3::Zero);

private:
    void    Update_AttachedEffects();
    bool    Try_GetSocketWorldPosition(
        const wstring& partTag,
        const string& socketName,
        Vec3& outWorldPosition) const;

private:
    SwitchDissolveRuntime       _switchDissolve{};
    vector<AttachedEffect>      _attachedEffects{};

public:
    static Shared<EffectCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;

};

NS_END
