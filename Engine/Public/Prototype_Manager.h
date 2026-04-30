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
	HRESULT Initialize(uint32 numLevels);
	HRESULT Add_Prototype(uint32 levelIndex, const wstring& prototypeTag, Shared<Base> prototype);
	Shared<Base> Clone_Prototype(Prototype prototype, uint32 levelIndex, const wstring& prototypeTag, void* arg);
	HRESULT Clear_Prototypes(uint32 levelIndex);

private:
	vector<map<const wstring, Shared<Base>>> _prototypes;
	uint32 _numLevels = {};

private:
	Shared<Base> Find_Prototype(uint32 levelIndex, const wstring& prototypeTag);

public:
	static Unique<Prototype_Manager> Create(uint32 numLevels);
	virtual void Free() override;
};

NS_END

