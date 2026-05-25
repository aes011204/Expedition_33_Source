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
    // PlayerWorldBase 에서 옮겨옴
    void    Start_SwitchDissolve(float durationSec, const string& noiseName);
	// PlayerablePart::Start_SwitchDisolve에서 옮김
    HRESULT Bind_DrawFeatures(ShaderCom* shader, uint32 supportFeatureMask);

    // Render 쪽에서는 owner만 세팅
    static HRESULT Bind_DrawFeatures(
                    const Shared<GameObject>& owner,
                    ShaderCom* shader,
                    uint32 supportFeatureMask);

	// Ready_Layer_Effect의 EffectInstance 직접 생성 코드 옮김
    HRESULT Play_AttachedEffect(
                uint32 levelIndex,
				const wstring& effectName,
				const wstring& partTag,
				const string& socketName,
				const wstring& layerTag,
				const Vec3& localOffset = Vec3::Zero);

private:
	// Level_GamePlay::Sync_LuneRightHandleEffect에서 Move
    void    Update_AttachedEffects();

    bool    Try_GetSocketWorldPosition(
				const wstring& partTag,
				const string& socketName,
				Vec3& outWorldPosition) const;

private:
    SwitchDissolveRuntime       _switchDissolve{};		// 디졸브
    vector<AttachedEffect>      _attachedEffects{};		// 소켓에 붙일 이팩트

public:
    static Shared<EffectCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;

};

NS_END
