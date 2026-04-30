#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Object_Manager final : public Base
{
private:
	Object_Manager();

public:
	virtual ~Object_Manager() = default;

public:
	HRESULT Initialize(uint32 numLevels);
	HRESULT Add_GameObject(uint32 prototypeLevelIndex, const wstring& prototypeTag, uint32 layerLevelIndex, const wstring& layerTag, void* arg);
	void Priority_Update(float timeDelta);
	void Update(float timeDelta);
	void Late_Update(float timeDelta);
	HRESULT Clear_Layers(uint32 levelIndex);

private:
	uint32 _numLevels = {};
	vector<map<const wstring, Shared<class Layer>>> _layers;
private:
	Shared<class Layer> Find_Layer(uint32 layerLevelIndex, const wstring& layerTag);

public:
	static Unique<Object_Manager> Create(uint32 numLevels);
	virtual void Free() override;
};

NS_END



