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
    void    OnStayOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void    OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;

public:
    void    Set_UIRequest(
        const wstring& uiJsonPath,
        const wstring& rootUIName,
        const wstring& animationPath = L"",
        float autoCloseDelaySec = -1.f);
    void    Set_TriggerOptions(bool triggerOnce, bool closeOnExit);
    const wstring& Get_UIJsonPath() const { return _uiJsonPath; }

private:
    HRESULT Ready_Components();
    bool    Is_ActivePlayerCollider(const Shared<ColliderCom>& other) const;
    const char* Get_OpenBlockReason() const;
    bool    Can_OpenUI() const;
    bool    Has_PlayerOverlap() const { return !_playerOverlapColliders.empty(); }
    bool    Try_OpenUI();
    void    Close_UI();
    void    Disable_TriggerCollider();

#if defined(_DEBUG) || defined(_EDITOR)
    void    Debug_LogOverlapEvent(const char* eventName, const Shared<ColliderCom>& other, bool activePlayer);
    void    Debug_LogOpenBlocked(const char* reason);
#endif

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

#if defined(_DEBUG) || defined(_EDITOR)
    uint32 _debugRejectedOverlapLogCount = 0;
    uint32 _debugStayOverlapLogCount = 0;
    const char* _debugLastOpenBlockReason = nullptr;
#endif

public:
    static Shared<Trigger_WorldDialogue> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
