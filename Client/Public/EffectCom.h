#pragma once

#include "Component.h"
#include "EffectInstancePool.h"
#include "SwitchDissolveFeature.h"
#include "TrailAnchor_Asset.h"

NS_BEGIN(Engine)
class GameObject;
class IEffectTrailSampleProvider;
class ShaderCom;
NS_END

NS_BEGIN(Client)

class Weapon;

class EffectCom : public Component, public IEffectTrailSampleProvider
{
    GENERATED_COMPONENT(EffectCom);

private:
    struct AttachedEffect
    {
        EffectInstancePool::AttachedHandle poolHandle{};

        wstring effectName{};
        wstring partTag{};

        string  socketName{};
        wstring layerTag{};
        Vec3    localOffset{ Vec3::Zero };
        bool    isTrail = false;
        TrailAnchorAsset trailAnchorAsset{};
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

public:
#pragma region Effect 특정 파츠, 소켓에 붙이기
    HRESULT Play_AttachedEffect(
                uint32 levelIndex,
				const wstring& effectName,
				const wstring& partTag,
				const string& socketName,
				const wstring& layerTag,
				const Vec3& localOffset = Vec3::Zero,
                const Weak<IEffectTrailSampleProvider>& trailSampleProvider = {});
#pragma endregion

#pragma region 이팩트 재생
    HRESULT Play_Effect(
                uint32 levelIndex,
                const wstring& effectName,
                const wstring& layerTag,
                const Vec3& localOffset = Vec3::Zero);
#pragma endregion


#pragma region ANS_Trail에서 사용할 Play, Stop Trail
     HRESULT Play_TrailEffect(
                const wstring& effectName,
                const wstring& partTag,
                const wstring& layerTag);

    void    Stop_TrailEffect(
                const wstring& effectName,
                const wstring& partTag);

    bool    Try_GetTrailSample(EffectTrailSample& outSample) const override;
#pragma endregion


private:
    void Update_AttachedEffects();                                           // socket follow와 자동 release 후보 정리를 함께 수행한다.
    void Release_AttachedEffect(AttachedEffect& attached);                   // 명시 반환이 필요한 attached effect를 pool로 되돌린다.
    bool Should_ReleaseAttachedEffect(const AttachedEffect& attached) const; // auto/manual release 정책을 판정한다.
    void Prune_ReleasedAttachedEffects();                                    // 반환이 끝난 attached entry를 목록에서 제거한다.

    bool Try_GetSocketWorldPosition(
            const wstring& partTag,
            const string& socketName,
            Vec3& outWorldPosition) const;

    bool Try_BuildTrailSample(
        const AttachedEffect& attached,
        EffectTrailSample& outSample) const;

    bool Try_ResolveTrailAnchorWorldPosition(
        const Weapon& weaponPart,
        const TrailAnchorPointDesc& anchor,
        Vec3& outWorldPosition) const;

    const AttachedEffect* Find_ActiveTrailEffect() const;


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
