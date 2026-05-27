#pragma once

#include "Component.h"
#include "EffectInstancePool.h"
#include "SwitchDissolveFeature.h"

NS_BEGIN(Engine)
class GameObject;
class IEffectTrailSampleProvider;
class ShaderCom;
NS_END

NS_BEGIN(Client)

class EffectCom : public Component
{
    GENERATED_COMPONENT(EffectCom);

private:
    struct AttachedEffect
    {
        EffectInstancePool::AttachedHandle poolHandle{}; // pool acquire/release 추적과 effect root 접근을 함께 담는다.
        wstring partTag{};                               // 어느 파츠 소켓을 따라갈지 나타낸다.
        string socketName{};                             // follow 기준이 되는 소켓 이름이다.
        Vec3 localOffset{ Vec3::Zero };                  // socket 월드 위치에 더할 local offset이다.
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
    void    Stop_AllAttachedEffects(); // 현재 이 컴포넌트가 소유한 attached effect를 모두 pool로 반환한다.

    HRESULT Bind_DrawFeatures(ShaderCom* shader, uint32 supportFeatureMask);

    // Render 쪽에서는 owner만 세팅
    static HRESULT Bind_DrawFeatures(const Shared<GameObject>& owner, ShaderCom* shader, uint32 supportFeatureMask);

    HRESULT Play_AttachedEffect(
                uint32 levelIndex,
				const wstring& effectName,
				const wstring& partTag,
				const string& socketName,
				const wstring& layerTag,
				const Vec3& localOffset = Vec3::Zero,
                const Weak<IEffectTrailSampleProvider>& trailSampleProvider = {});

private:
    void Update_AttachedEffects();                                           // socket follow와 자동 release 후보 정리를 함께 수행한다.
    void Release_AttachedEffect(AttachedEffect& attached);                   // 명시 반환이 필요한 attached effect를 pool로 되돌린다.
    bool Should_ReleaseAttachedEffect(const AttachedEffect& attached) const; // auto/manual release 정책을 판정한다.
    void Prune_ReleasedAttachedEffects();                                    // 반환이 끝난 attached entry를 목록에서 제거한다.

    bool Try_GetSocketWorldPosition(
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
