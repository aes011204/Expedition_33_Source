#pragma once

#include "ModelCom.h"
#include "ShaderCom.h"
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

    bool Has_Prototype(uint32 levelIndex, const wstring& prototypeTag) const;
    // Profile/debug summary처럼 prototype registry를 읽기만 하는 진단 코드에서 사용한다.
    void Visit_Prototypes(const function<void(uint32 levelIndex, const wstring& prototypeTag, const Shared<Base>& prototype)>& visitor) const;

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

