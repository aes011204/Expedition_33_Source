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
	HRESULT Initialize(uint32 iNumLevels);
	HRESULT Add_GameObject(uint32 iPrototypeLevelIndex, const wstring& strPrototypeTag, uint32 iLayerLevelIndex, const wstring& strLayerTag, void* pArg);
	void Priority_Update(float fTimeDelta);
	void Update(float fTimeDelta);
	void Late_Update(float fTimeDelta);
	HRESULT Clear_Layers(uint32 iLevelIndex);

private:
	uint32 _numLevels = {};
	vector<map<const wstring, Shared<class Layer>>> _layers;
private:
	Shared<class Layer> Find_Layer(uint32 iLayerLevelIndex, const wstring& strLayerTag);

public:
	static Unique<Object_Manager> Create(uint32 iNumLevels);
	virtual void Free() override;
};

NS_END



