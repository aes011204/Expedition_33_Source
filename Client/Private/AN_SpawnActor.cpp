#include "AN_SpawnActor.h"

#include "ContainerObject.h"
#include "Create_Class.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "PlayableParts.h"
#include "TransformCom.h"
#include "Weapon.h"

IMPLEMENT_REFLECTION(AN_SpawnActor)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_STRING("클래스 이름", _staticClass);
    PROPERTY_WSTRING("레이어 태그", _layerTag);

    PROPERTY_BOOL("소켓 스폰?", _spawnAtSocket);
    PROPERTY_WSTRING("파츠 이름", _partTag);
    PROPERTY_STRING("소켓 이름", _socketName);
    PROPERTY_VEC3("로컬 오프셋", _localOffset);
    PROPERTY_VEC3("스폰 크기", _spawnScale);

    PROPERTY_BOOL("Owner 회전값?", _copyOwnerRotation);

    return true;
}

void AN_SpawnActor::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    if (context.owner == nullptr)
        return;

    if (_staticClass.empty())
        return; // 스폰할 프로토타입 이름이 없으면 실패

    const auto spawnOwner = Resolve_SpawnOwner(context.owner);
    if (spawnOwner == nullptr)
        return;

    Vec3 spawnWorldPosition{};
    if (!Try_GetSpawnWorldPosition(
        spawnOwner,
        _spawnAtSocket,
        _partTag,
        _socketName,
        _localOffset,
        spawnWorldPosition))
    {
        return;
    }

    auto builder = Create_Class::Actor(_staticClass)
        .AtLevel(GAME->Current_LevelIndex())
        .InLayer(_layerTag)
        .Position(spawnWorldPosition)
        .Scale(_spawnScale);

    if (_copyOwnerRotation)
    {
        const auto ownerTransform = spawnOwner->Get_Transform();
        if (ownerTransform)
        {
            // owner 방향을 그대로 받아야 할 때만 회전을 복사해서 사용
            builder.Rotation(ownerTransform->Get_WorldRotationEuler());
        }
    }

    const Shared<GameObject> spawnedActor = builder.Spawn();
    if (spawnedActor == nullptr)
    {
        LOG_ERROR("AN_SpawnActor 실패");
        return;
    }

    // 몬스터 충돌 시 이 충돌체를 누가 만들어줬는지 세팅해야함
    spawnedActor->Set_Owner(spawnOwner);
}

Shared<GameObject> AN_SpawnActor::Resolve_SpawnOwner(GameObject* notifyOwner)
{
    auto current
        = notifyOwner ? notifyOwner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        if (dynamic_pointer_cast<ContainerObject>(current))
            return current;

        // Container가 아니여도 최상위 owner를 spawn 기준으로 사용
        if (current->Get_Owner() == nullptr)
            return current;

        current = current->Get_Owner();
    }

    return nullptr;
}

bool AN_SpawnActor::Try_GetSocketWorldPosition(const Shared<GameObject>& ownerObject, const wstring& partTag,
    const string& socketName, Vec3& outWorldPosition)
{
    // 컨테이너
    const Shared<ContainerObject> ownerContainer =
        dynamic_pointer_cast<ContainerObject>(ownerObject);
    if (ownerContainer == nullptr)
        return false;

    // 파츠
    PartObject* partObject = ownerContainer->Get_PartObject(partTag);
    if (partObject == nullptr)
        return false;

    // 소켓 구하기
    if (auto* playablePart = dynamic_cast<PlayableParts*>(partObject))
    {
        const Matrix* socketMatrix = playablePart->Get_SocketBoneMatrixPtr(socketName);
        if (socketMatrix == nullptr)
            return false;

        const Shared<TransformCom> partTransform = playablePart->Get_Transform();
        const Shared<TransformCom> ownerTransform = ownerContainer->Get_Transform();
        if (partTransform == nullptr || ownerTransform == nullptr)
            return false;

        // EffectCom이 쓰는 socket 월드 위치 계산식과 같은 방식으로 맞춘다.
        const Matrix socketWorldMatrix =
            *socketMatrix * partTransform->Get_WorldMatrix() * ownerTransform->Get_WorldMatrix();

        outWorldPosition = Vec3(socketWorldMatrix._41, socketWorldMatrix._42, socketWorldMatrix._43);
        return true;
    }

    if (auto* weaponPart = dynamic_cast<Weapon*>(partObject))
    {
        const Matrix* socketMatrix = weaponPart->Get_SocketBoneMatrixPtr(socketName);
        if (socketMatrix == nullptr)
            return false;

        const Matrix socketWorldMatrix = *socketMatrix * weaponPart->Get_CombinedWorldMatrix();
        outWorldPosition = Vec3(socketWorldMatrix._41, socketWorldMatrix._42, socketWorldMatrix._43);
        return true;
    }

    return false;
}

bool AN_SpawnActor::Try_GetSpawnWorldPosition(const Shared<GameObject>& ownerObject, bool spawnAtSocket,
    const wstring& partTag, const string& socketName, const Vec3& localOffset, Vec3& outWorldPosition)
{
    if (ownerObject == nullptr)
        return false;

    Vec3 baseWorldPosition{};

    if (spawnAtSocket)
    {
        if (!Try_GetSocketWorldPosition(ownerObject, partTag, socketName, baseWorldPosition))
            return false;
    }
    else
    {
        const Shared<TransformCom> ownerTransform = ownerObject->Get_Transform();
        if (ownerTransform == nullptr)
            return false;

        // root spawn이면 owner 현재 위치를 그대로 쓴다.
        baseWorldPosition = ownerTransform->Get_Position();
    }

    // root/socket 공통으로 마지막에 offset만 더한다.
    outWorldPosition = baseWorldPosition + localOffset;
    return true;
}
