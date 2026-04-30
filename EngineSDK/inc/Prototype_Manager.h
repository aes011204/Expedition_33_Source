#pragma once

#include "Model.h"
#include "Shader.h"
#include "Texture.h"
#include "VIBuffer_Rect.h"
#include "VIBuffer_Cube.h"
#include "VIBuffer_Terrain.h"

NS_BEGIN(Engine)

class Prototype_Manager final : public Base
{
private:
	Prototype_Manager();

public:
	virtual ~Prototype_Manager() = default;

public:
	HRESULT Initialize(uint32 iNumLevels);
	HRESULT Add_Prototype(uint32 iLevelIndex, const wstring& strPrototypeTag, Shared<Base> pPrototype);
	Shared<Base> Clone_Prototype(Prototype ePrototype, uint32 iLevelIndex, const wstring& strPrototypeTag, void* pArg);
	HRESULT Clear_Prototypes(uint32 iLevelIndex);

private:
	vector<map<const wstring, Shared<Base>>> _prototypes;
	uint32 _numLevels = {};

private:
	Shared<Base> Find_Prototype(uint32 iLevelIndex, const wstring& strPrototypeTag);

public:
	static Unique<Prototype_Manager> Create(uint32 iNumLevels);
	virtual void Free() override;
};

NS_END

