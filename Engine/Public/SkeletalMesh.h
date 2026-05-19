#pragma once
#include "Mesh.h"

NS_BEGIN(Engine)

struct AnimComputeMeshBoneRef;

class ENGINE_DLL SkeletalMesh final : public Mesh
{
public:
    SkeletalMesh(const ComPtr<Device>& device, const ComPtr<Context>& context);
    SkeletalMesh(const SkeletalMesh& prototype);
    ~SkeletalMesh() override = default;

public:
    HRESULT Bind_BoneMatrices(ShaderCom* shader, const char* constantName, const vector<Shared<Bone>>& bones) override;
    HRESULT Bind_BoneMatrices(ShaderCom* shader, const char* constantName, const vector<Matrix>& combinedMatrices) override;

public:
    HRESULT Initialize_Prototype(const ModelMeshData& meshData, const Matrix& preTransformMatrix);
    HRESULT Initialize(void* arg) override;

    HRESULT Build_MeshBoneMatrices(const vector<Matrix>& combinedMatrices,
        vector<Matrix>& outBoneMatrices) const override;

    HRESULT Build_ComputeMeshBoneRefs(vector<AnimComputeMeshBoneRef>& outRefs) const;

    uint32 Get_NumBoneRefs() const override { return _numBones; }
    const vector<uint32>& Get_BoneIndices() const override { return _boneIndices; }

    const vector<VTXANIMMESH>& Get_SourceVertices() const { return _sourceVertices; }

private:
    uint32              _numBones{};
    vector<uint32>      _boneIndices;
    vector<Matrix>	    _offsetMatrices;
    Matrix              _boneMatrices[maxBoneCount];

    vector<VTXANIMMESH> _sourceVertices;

public:
    static Shared<SkeletalMesh> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const ModelMeshData& meshData, const Matrix& preTransformMatrix);
    Shared<Component> Clone(void* arg) override;
    void Free() override;

};

NS_END
