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
    HRESULT RenderInstanced(Buffer* instanceVB, uint32 instanceStride, uint32 instanceCount);
    HRESULT Render_ComputeSkinned(Buffer* vertexBuffer, uint32 vertexStride);
    const string& Get_Name() { return _name;  }
    uint32 Get_MaterialIndex() { return _materialIndex;  }

public:
    virtual HRESULT Bind_BoneMatrices(ShaderCom* shader, const char* constantName, const vector<Shared<Bone>>& bones) PURE;
    virtual HRESULT Bind_BoneMatrices(ShaderCom* shader, const char* constantName, const vector<Matrix>& combinedMatrices) PURE;

    // combined bone matrix 목록을 기준으로 이 mesh가 참조하는 bone matrix 배열 생성
    virtual HRESULT Build_MeshBoneMatrices(
        const vector<Matrix>& combinedMatrices,
        vector<Matrix>& outBoneMatrices) const PURE;

    virtual uint32 Get_NumBoneRefs() const { return 0; }

    virtual const vector<uint32>& Get_BoneIndices() const;

protected:
    string	_name;
    uint32	_materialIndex = {};
    Matrix  _preTransformMatrix{};

public:
    virtual Shared<Component> Clone(void* arg) override PURE;
    virtual void Free() override;

};

NS_END
