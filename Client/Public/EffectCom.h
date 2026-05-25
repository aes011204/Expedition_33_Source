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
        Shared<GameObject> effectObject{};      // 이펙트 오브젝트
        wstring partTag{};                      // 어디 파츠에 붙일건지
        string socketName{};                    // 이펙트가 따라갈 소켓 이름
        Vec3 localOffset{ Vec3::Zero };       // socket 위치 + offset
    };

public:
    EffectCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ~EffectCom() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;
    void    Update(float timeDelta);

public:
    // World에서 Active 중인 플레이어 바꿀 때
    void    Start_SwitchDissolve(float durationSec, const string& noiseName);
    // 몬스터 죽을 떄 사라지게
    void    Start_VanishDissolve(float durationSec, const string& noiseName, bool vanishComplete);

    HRESULT Bind_DrawFeatures(ShaderCom* shader, uint32 supportFeatureMask);

    // Render 쪽에서는 owner만 세팅
    static HRESULT Bind_DrawFeatures(const Shared<GameObject>& owner, ShaderCom* shader, uint32 supportFeatureMask);

    HRESULT Play_AttachedEffect(
                uint32 levelIndex,
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
    SwitchDissolveRuntime       _switchDissolve{};		// 디졸브
    vector<AttachedEffect>      _attachedEffects{};		// 소켓에 붙일 이팩트

private:
    bool _vanishComplete = false;

public:
    static Shared<EffectCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;

};

NS_END
