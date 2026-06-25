#pragma once
#include "GameObject.h"

NS_BEGIN(Client)

class Trigger_BGMZone final : public GameObject
{
    GENERATED_GAMEOBJECT(Trigger_BGMZone, ObjectType::Trigger)

public:
    enum class GestralBGMTrack
    {
        Market,
        GolgraThrone,
    };

public:
    Trigger_BGMZone(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Trigger_BGMZone(const Trigger_BGMZone& prototype);
    ~Trigger_BGMZone() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Late_Update(float timeDelta) override;
    void    OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void    OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;

private:
    HRESULT Ready_Components();
    const wchar_t* Resolve_BGMTrack() const;
    bool Is_ActivePlayerCollider(const Shared<ColliderCom>& other) const;
    void Play_OverrideBGM();
    void Restore_PreviousBGM();

private:
    Shared<ColliderCom> _colliderCom{};
    GestralBGMTrack _bgmTrack = GestralBGMTrack::GolgraThrone;
    float _fadeOutDuration = 1.f;
    wstring _savedBGM{};
    bool _hasSavedBGM = false;
    int32 _playerOverlapCount = 0;

public:
    static Shared<Trigger_BGMZone> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
