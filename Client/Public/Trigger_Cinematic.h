#pragma once
#include "GameObject.h"

NS_BEGIN(Client)

class Trigger_Cinematic : public GameObject
{
    GENERATED_GAMEOBJECT(Trigger_Cinematic, ObjectType::Trigger_Cinematic)

public:
    Trigger_Cinematic(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Trigger_Cinematic(const Trigger_Cinematic& prototype);
    virtual ~Trigger_Cinematic() = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;
    void    OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void    Set_SequenceFileId(const wstring& sequenceFileId);

private:
    HRESULT Ready_Components();

private:
    Shared<ColliderCom> _colliderCom{};
    wstring             _sequenceFileId = L"";
public:
    static Shared<Trigger_Cinematic> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
