#pragma once

#include "ContainerObject.h"
#include "EffectCom.h"
#include "GameInstance.h"
#include "ModelCom.h"
#include "Monster_BattleBase.h"
#include "Monster_Part.h"
#include "Monster_WorldBase.h"
#include "Player_BattleBase.h"
#include "Player_WorldBase.h"
#include <functional>

NS_BEGIN(MainEditor)
namespace PreviewObjectHelpers
{
    inline void Disable_PreviewAnimNotifyExecution(const Shared<GameObject>& gameObject)
    {
        if (gameObject == nullptr)
            return;

        gameObject->Visit_Components([&](const wstring&, const Shared<Component>& component)
        {
            Shared<ModelCom> modelCom = dynamic_pointer_cast<ModelCom>(component);
            if (modelCom != nullptr)
                modelCom->Set_AnimNotifyExecutionEnabled(false);
        });

        Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(gameObject);
        if (container == nullptr)
            return;

        for (const auto& [partTag, partObject] : container->Get_PartObjects())
        {
            UNREFERENCED_PARAMETER(partTag);
            Disable_PreviewAnimNotifyExecution(partObject);
        }
    }

    inline Shared<GameObject> Find_GameObjectByKey(const Shared<GameObject>& rootObject, const wstring& objectKey)
    {
        if (rootObject == nullptr)
            return nullptr;

        if (objectKey.empty() || objectKey == L"Root")
            return rootObject;

        std::function<Shared<GameObject>(const Shared<GameObject>&, const wstring&)> findRecursive =
            [&](const Shared<GameObject>& currentObject, const wstring& currentKey) -> Shared<GameObject>
        {
            if (currentObject == nullptr)
                return nullptr;

            if (currentKey == objectKey)
                return currentObject;

            Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(currentObject);
            if (container == nullptr)
                return nullptr;

            for (const auto& [partTag, partObject] : container->Get_PartObjects())
            {
                if (partObject == nullptr)
                    continue;

                const wstring childKey = currentKey + L"/" + partTag;
                Shared<GameObject> foundObject = findRecursive(partObject, childKey);
                if (foundObject != nullptr)
                    return foundObject;
            }

            return nullptr;
        };

        return findRecursive(rootObject, L"Root");
    }

    inline bool Try_BuildGameObjectKey(
        const Shared<GameObject>& rootObject,
        const Shared<GameObject>& targetObject,
        wstring& outObjectKey)
    {
        outObjectKey.clear();

        if (rootObject == nullptr || targetObject == nullptr)
            return false;

        std::function<bool(const Shared<GameObject>&, const wstring&)> findRecursive =
            [&](const Shared<GameObject>& currentObject, const wstring& currentKey) -> bool
        {
            if (currentObject == nullptr)
                return false;

            if (currentObject == targetObject)
            {
                outObjectKey = currentKey;
                return true;
            }

            Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(currentObject);
            if (container == nullptr)
                return false;

            for (const auto& [partTag, partObject] : container->Get_PartObjects())
            {
                if (partObject == nullptr)
                    continue;

                const wstring childKey = currentKey + L"/" + partTag;
                if (findRecursive(partObject, childKey))
                    return true;
            }

            return false;
        };

        return findRecursive(rootObject, L"Root");
    }

    inline Shared<ModelCom> Find_ModelComponentBySourcePath(
        const Shared<GameObject>& gameObject,
        const wstring& sourceModelPath)
    {
        if (gameObject == nullptr)
            return nullptr;

        Shared<ModelCom> foundModel{};
        gameObject->Visit_Components([&](const wstring&, const Shared<Component>& component)
        {
            if (foundModel != nullptr)
                return;

            Shared<ModelCom> modelCom = dynamic_pointer_cast<ModelCom>(component);
            if (modelCom == nullptr)
                return;

            if (sourceModelPath.empty() || modelCom->Get_SourceModelPath() == sourceModelPath)
                foundModel = modelCom;
        });

        return foundModel;
    }

    inline void Prime_PreviewObject(const Shared<GameObject>& gameObject)
    {
        if (gameObject == nullptr)
            return;

        gameObject->Try_BeginPlay();

        Shared<Client::Monster_WorldBase> monsterWorld = dynamic_pointer_cast<Client::Monster_WorldBase>(gameObject);
        if (monsterWorld == nullptr)
            return;

        Client::Monster_Part* bodyPart = dynamic_cast<Client::Monster_Part*>(monsterWorld->Get_PartObject(L"Part_Body"));
        if (bodyPart == nullptr)
            return;

        bodyPart->Set_AnimationByState(ETOI(Client::Monster_WorldBase::MonsterWorldAnim::Idle), true, 0.f);
        bodyPart->Play_Animation(0.f);
    }

    inline void Advance_MonsterPreviewAnimation(const Shared<GameObject>& gameObject, float timeDelta)
    {
        if (gameObject == nullptr)
            return;

        if (Shared<Client::Monster_Part> monsterPart = dynamic_pointer_cast<Client::Monster_Part>(gameObject))
            monsterPart->Play_Animation(timeDelta);

        Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(gameObject);
        if (container == nullptr)
            return;

        for (const auto& [partTag, partObject] : container->Get_PartObjects())
        {
            UNREFERENCED_PARAMETER(partTag);
            Advance_MonsterPreviewAnimation(partObject, timeDelta);
        }
    }

    inline void Tick_AttachedPreviewEffects(const Shared<GameObject>& gameObject, float timeDelta)
    {
        if (gameObject == nullptr)
            return;

        gameObject->Visit_Components([&](const wstring&, const Shared<Component>& component)
        {
            Shared<Client::EffectCom> effectCom = dynamic_pointer_cast<Client::EffectCom>(component);
            if (effectCom)
                effectCom->Tick_AttachedEffectInstances(timeDelta);
        });

        Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(gameObject);
        if (container == nullptr)
            return;

        for (const auto& [partTag, partObject] : container->Get_PartObjects())
        {
            UNREFERENCED_PARAMETER(partTag);
            Tick_AttachedPreviewEffects(partObject, timeDelta);
        }
    }

    inline void Update_PreviewObject(const Shared<GameObject>& gameObject, float timeDelta)
    {
        if (gameObject == nullptr)
            return;

        gameObject->Try_BeginPlay();

        if (Shared<Client::Monster_WorldBase> monsterWorld = dynamic_pointer_cast<Client::Monster_WorldBase>(gameObject))
        {
            monsterWorld->Engine::ContainerObject::Priority_Update(timeDelta);
            Advance_MonsterPreviewAnimation(gameObject, timeDelta);
            monsterWorld->Engine::ContainerObject::Update(timeDelta);
            monsterWorld->Engine::ContainerObject::Late_Update(timeDelta);
            Tick_AttachedPreviewEffects(gameObject, timeDelta);
            return;
        }

        if (Shared<Client::Monster_BattleBase> monsterBattle = dynamic_pointer_cast<Client::Monster_BattleBase>(gameObject))
        {
            monsterBattle->Engine::ContainerObject::Priority_Update(timeDelta);
            Advance_MonsterPreviewAnimation(gameObject, timeDelta);
            monsterBattle->Engine::ContainerObject::Update(timeDelta);
            monsterBattle->Engine::ContainerObject::Late_Update(timeDelta);
            Tick_AttachedPreviewEffects(gameObject, timeDelta);
            return;
        }

        if (Shared<Client::Player_WorldBase> playerWorld = dynamic_pointer_cast<Client::Player_WorldBase>(gameObject))
        {
            playerWorld->Engine::ContainerObject::Priority_Update(timeDelta);
            playerWorld->Engine::ContainerObject::Update(timeDelta);
            playerWorld->Engine::ContainerObject::Late_Update(timeDelta);
            Tick_AttachedPreviewEffects(gameObject, timeDelta);
            return;
        }

        if (Shared<Client::Player_BattleBase> playerBattle = dynamic_pointer_cast<Client::Player_BattleBase>(gameObject))
        {
            playerBattle->Engine::ContainerObject::Priority_Update(timeDelta);
            playerBattle->Engine::ContainerObject::Update(timeDelta);
            playerBattle->Engine::ContainerObject::Late_Update(timeDelta);
            Tick_AttachedPreviewEffects(gameObject, timeDelta);
            return;
        }

        gameObject->Priority_Update(timeDelta);
        gameObject->Update(timeDelta);
        gameObject->Late_Update(timeDelta);
        Tick_AttachedPreviewEffects(gameObject, timeDelta);
    }
}
NS_END
