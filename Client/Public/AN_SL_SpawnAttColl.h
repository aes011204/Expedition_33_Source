#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Client)

class ANS_SL_SpawnAttColl final : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_SL_SpawnAttColl)

public:
    ANS_SL_SpawnAttColl() = default;
    ~ANS_SL_SpawnAttColl() override = default;

public:
    string Get_TypeName() const override { return "ANS_SL_SpawnAttColl"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    Shared<GameObject> Resolve_SpawnOwner(GameObject* notifyOwner) const;
    Vec3 Resolve_SpawnWorldPosition(const AnimNotifyContext& context, const Shared<GameObject>& spawnOwner) const;
    void Remove_SpawnedObject(GameObject* notifyOwner) const;

private:
    string _prefabName = "";
    wstring _layerTag = L"Layer_GameObject";
    bool _useCachedPlayerPosition = false;
    Vec3 _localOffset = Vec3::Zero;
    Vec3 _spawnRotation = Vec3::Zero;
    Vec3 _spawnScale = Vec3::One;
    bool _copyOwnerRotation = false;
};

NS_END
