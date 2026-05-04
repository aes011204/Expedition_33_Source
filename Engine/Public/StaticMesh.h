#pragma once
#include "Mesh.h"

NS_BEGIN(Engine)

class StaticMesh final : public Mesh
{
public:
    StaticMesh(const ComPtr<Device>& device, const ComPtr<Context>& context);
    StaticMesh(const StaticMesh& prototype);
    ~StaticMesh() override = default;

public:
    HRESULT Bind_BoneMatrices(ShaderCom* shader, const char* constantName, const vector<Shared<Bone>>& bones) override;

public:
    HRESULT Initialize_Prototype(const ModelMeshData& meshData, const Matrix& preTransformMatrix);
    virtual HRESULT Initialize(void* arg) override;

private:

public:
    static Shared<StaticMesh> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const ModelMeshData& meshData, const Matrix& preTransformMatrix);
    Shared<Component> Clone(void* arg) override;
    void Free() override;

};

NS_END
