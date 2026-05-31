#pragma once

#include "AnimNotify.h"

NS_BEGIN(Client)

// 특정 클래스 스폰하는 Notify
class AN_SpawnActor : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_SpawnActor)

public:
    AN_SpawnActor() = default;
    ~AN_SpawnActor() override = default;

public:
    string Get_TypeName() const override { return "AN_SpawnActor"; }
    void Execute(const AnimNotifyContext& context) override;

private:
    Shared<GameObject> Resolve_SpawnOwner(GameObject* notifyOwner);

    bool Try_GetSocketWorldPosition(
        const Shared<GameObject>& ownerObject,
        const wstring& partTag,
        const string& socketName,
        Vec3& outWorldPosition);

    bool Try_GetSpawnWorldPosition(
        const Shared<GameObject>& ownerObject,
        bool spawnAtSocket,
        const wstring& partTag,
        const string& socketName,
        const Vec3& localOffset,
        Vec3& outWorldPosition);

private:
    string  _staticClass = "";
    wstring _layerTag = L"Layer_GameObject";

    bool    _spawnAtSocket = false;          // false면 owner root, true면 part/socket 위치에서 생성한다.
    wstring _partTag = L"";                  // socket spawn일 때 대상 파츠를 지정한다.
    string  _socketName = "";                // socket spawn일 때 대상 소켓 이름을 지정한다.
    Vec3    _localOffset = Vec3::Zero;       // 루트/소켓 위치에서 약간 밀어낼 때 쓴다.
    Vec3    _spawnScale = Vec3::One;

    bool    _copyOwnerRotation = false;      // 필요하면 spawn actor가 owner 회전을 그대로 받게 한다.
};

NS_END
