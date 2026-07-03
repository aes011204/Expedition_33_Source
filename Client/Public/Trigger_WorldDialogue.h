#pragma once
#include "GameObject.h"

NS_BEGIN(Client)

class Trigger_WorldDialogue final : public GameObject
{
    GENERATED_GAMEOBJECT(Trigger_WorldDialogue, ObjectType::Trigger)

public:
    Trigger_WorldDialogue(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Trigger_WorldDialogue(const Trigger_WorldDialogue& prototype);
    ~Trigger_WorldDialogue() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;
    void    OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void    OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;

public:
    void    Set_Line(uint32 index, const wstring& text, const wstring& portraitTexturePath);
    void    Set_Timing(float lineIntervalSec, float autoCloseDelaySec);
    void    Set_TriggerOptions(bool triggerOnce, bool closeOnExit);

private:
    HRESULT Ready_Components();
    bool    Is_ActivePlayerCollider(const Shared<ColliderCom>& other) const;
    bool    Has_AnyLine() const;
    bool    Can_StartSequence() const;
    bool    Has_PlayerOverlap() const { return !_playerOverlapColliders.empty(); }
    void    Start_Sequence();
    bool    Push_NextLine();
    void    Stop_Sequence(bool clearDialogue);
    const wstring& Get_LineText(uint32 index) const;
    const wstring& Get_LinePortrait(uint32 index) const;

private:
    Shared<ColliderCom> _colliderCom{};
    uset<ColliderCom*>  _playerOverlapColliders{};

    wstring _line1{};
    wstring _line2{};
    wstring _line3{};

    wstring _portrait1 = L"../Bin/Resources/Textures/UI/Character/CharacterHUD/T_HUD_Gustave_512x512.png";
    wstring _portrait2 = L"../Bin/Resources/Textures/UI/Character/CharacterHUD/T_HUD_Lune_512x512.png";
    wstring _portrait3 = L"../Bin/Resources/Textures/UI/Character/CharacterHUD/T_HUD_Maelle_512x512.png";

    float _lineIntervalSec = 1.15f;
    float _autoCloseDelaySec = 4.f;
    bool  _triggerOnce = true;
    bool  _closeOnExit = false;

    bool  _sequenceActive = false;
    bool  _played = false;
    uint32 _nextLineIndex = 0;
    float _lineElapsedSec = 0.f;
    float _afterLastLineElapsedSec = 0.f;

public:
    static Shared<Trigger_WorldDialogue> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
