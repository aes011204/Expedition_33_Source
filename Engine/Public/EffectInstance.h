#pragma once
#include "EffectEmitter.h"
#include "EffectEmitterFactory.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)

class ENGINE_DLL EffectInstance final : public GameObject
{
    GENERATED_GAMEOBJECT(EffectInstance, ObjectType::Effect)

public:
    EffectInstance(const ComPtr<Device>& device, const ComPtr<Context>& context);
    EffectInstance(const EffectInstance& prototype);
    ~EffectInstance() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void BeginPlay() override;
    void Priority_Update(float timeDelta) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

private: //## Data::Definition
    EffectInstanceDesc _desc{};                   // root clone 시 전달받는 runtime desc다.
    Shared<const EffectDefinition> _definition{}; // root가 재생할 정적 effect 정의다.

private: //## Data::Children
    vector<Shared<EffectEmitter>> _emitters{}; // root가 직접 소유하는 child emitter 목록이다.

private: //## Helpers
    HRESULT Ready_Emitters(); // definition emitters를 concrete runtime child로 생성한다.
    void Sync_Emitters();     // root visibility/transform을 child emitter에 fan-out한다.
    void Clear_Emitters();    // root가 소유한 child 목록을 정리한다.

public:
    static Shared<EffectInstance> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
