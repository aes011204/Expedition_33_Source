#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Layer;

class Object_Manager final : public Base
{
private:
	Object_Manager();

public:
	virtual ~Object_Manager() = default;

public:
	HRESULT Initialize(uint32 numLevels);
	void    Priority_Update(float timeDelta);
	void    Update(float timeDelta);
	void    Late_Update(float timeDelta);
    void    Post_PhysX_Update(float timeDelta);

	HRESULT Clear_Layers(uint32 levelIndex);

	HRESULT Add_GameObject(uint32 prototypeLevelIndex, const wstring& prototypeTag, uint32 layerLevelIndex, const wstring& layerTag, void* arg);

    // OutGameObject로 반환받을 수 있게
    HRESULT Add_GameObject(uint32 prototypeLevelIndex, const wstring& prototypeTag, uint32 layerLevelIndex, const wstring& layerTag, Shared<GameObject>& outGameObject, void* arg = nullptr);

    // 프리펩 소환용
    HRESULT Add_GameObject(uint32 layerLevelIndex, const wstring& layerTag, const Shared<GameObject>& gameObject);
    bool    Remove_GameObject(uint32 levelIndex, const Shared<GameObject>& gameObject);

    Shared<GameObject> Find_FirstObject(uint32 levelIndex, const wstring& layerTag);

    void    Visit_LevelObjects(
                    const function<void(const wstring& layerTag, const Shared<GameObject>& gameObject)>& visitor) const;

    void    Visit_LevelObjects(uint32 levelIndex,
                    const function<void(const wstring& layerTag, const Shared<GameObject>& gameObject)>& visitor) const;

   uint64 Get_ObjectRevision() const { return _objectRevision; }
    void  Flush_RemovedGameObjects();

private:
	uint32 _numLevels = {};

    using LayerType = map<wstring, Shared<Layer>>;
	vector<LayerType> _layers;

    uint64 _objectRevision = 0;

private:
    // 삭제 대기중인 객체들 한번에 삭제하기
    struct PendingRemoveGameObject
    {
        uint32 levelIndex = 0;
        Shared<GameObject> gameObject{};
    };

    vector<PendingRemoveGameObject> _pendingRemoveGameObjects = {};

private:
	Shared<Layer> Find_Layer(uint32 layerLevelIndex, const wstring& layerTag);

public:
	static Unique<Object_Manager> Create(uint32 numLevels);
	virtual void Free() override;
};

NS_END



