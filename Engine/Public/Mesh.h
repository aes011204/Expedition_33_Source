#pragma once
#include "VIBuffer.h"
#include "Engine_Model_Struct.h"

NS_BEGIN(Engine)

class Bone;
class ShaderCom;

class ENGINE_DLL Mesh abstract : public VIBuffer
{
public:
    Mesh(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Mesh(const Mesh& prototype);
    ~Mesh() override = default;

public:
    uint32 Get_MaterialIndex() const { return _materialIndex; }

public:
    virtual HRESULT Bind_BoneMatrices(ShaderCom* shader, const char* constantName, const vector<Shared<Bone>>& bones) PURE;
    virtual HRESULT Bind_BoneMatrices(ShaderCom* shader, const char* constantName, const vector<Matrix>& combinedMatrices) PURE;

protected:
    string	_name;
    uint32	_materialIndex = {};
    Matrix  _preTransformMatrix{};

public:
    virtual Shared<Component> Clone(void* arg) override PURE;
    virtual void Free() override;

};

NS_END
