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
    void    Set_UIRequest(
        const wstring& uiJsonPath,
        const wstring& rootUIName,
        const wstring& animationPath = L"",
        float autoCloseDelaySec = -1.f);
    void    Set_TriggerOptions(bool triggerOnce, bool closeOnExit);

private:
    HRESULT Ready_Components();
    bool    Is_ActivePlayerCollider(const Shared<ColliderCom>& other) const;
    bool    Can_OpenUI() const;
    bool    Has_PlayerOverlap() const { return !_playerOverlapColliders.empty(); }
    bool    Try_OpenUI();
    void    Close_UI();
    void    Disable_TriggerCollider();

private:
    Shared<ColliderCom> _colliderCom{};
    uset<ColliderCom*>  _playerOverlapColliders{};

    wstring _uiJsonPath = L"UI/UI_Json/CommonSystem/UI_WorldDialogue_GestralVillage.json";
    wstring _rootUIName = L"Panel_WorldDialogue_GestralVillage";
    wstring _animationPath = L"UI/UI_Animations/Common/WorldDialogue_GestralVillage";
    float _autoCloseDelaySec = 10.f;
    bool  _triggerOnce = true;
    bool  _closeOnExit = false;

    bool  _openedForCurrentOverlap = false;
    bool  _played = false;

public:
    static Shared<Trigger_WorldDialogue> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
