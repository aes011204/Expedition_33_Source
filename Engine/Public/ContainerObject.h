#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)

class PartObject;

class ENGINE_DLL ContainerObject abstract : public GameObject
{
public:
    struct ContainerObjectDesc : public GameObject::GAMEOBJECT_DESC
    {

    };
public:
    ContainerObject(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ContainerObject(const ContainerObject& prototype);

public:
    virtual ~ContainerObject() = default;

public:
    virtual HRESULT Initialize_Prototype();
    virtual HRESULT Initialize(void* arg);
    virtual void BeginPlay() override;

    json To_Json() const override;
    void From_Json(const json& data) override;

    virtual void Priority_Update(float timeDelta);
    virtual void Update(float timeDelta);
    virtual void Late_Update(float timeDelta);
    virtual void Post_PhysX_Update(float timeDelta);
    virtual HRESULT Render();

public:
    PartObject* Get_PartObject(const wstring& partTag) const;
    const map<const wstring, Shared<PartObject>>& Get_PartObjects() const { return _partObjects; }

protected:
    HRESULT Add_PartObject(uint32 prototypeLevelIndex, const wstring& prototypeTag,
        const wstring& partTag, void* arg);

    PartObject* Find_PartObject(const wstring& partTag);

    virtual bool Should_ApplySerializedPart(const wstring& partTag, const json& partObjectJson) const { return true; }

    bool Remove_PartObject(const wstring& partTag);

protected:
    map<const wstring, Shared<PartObject>>	_partObjects;

    // partObject 업데이트 순서 보장. map이여서 Hair -> Head 순으로 될 수도 있음
    // 업데이트 순서가 다르면 문제점이, Hair다음으로 Head가 업데이트돼서
    // Hair 가 이전 프레임 위치를 가져서 업데이트가 돼서 한 프레임 늦게 따라올 수도 있다.
    vector<Shared<PartObject>>                      _partObjectUpdateOrder;

public:
    virtual Shared<GameObject> Clone(void* arg) = 0;
    virtual void Free() override;
};

NS_END



