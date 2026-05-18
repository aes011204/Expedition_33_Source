#include "../Engine_Shader_ComputeAnimTypes.hlsli"

StructuredBuffer<BoneMatrixElement> g_GlobalBones : register(t0);
StructuredBuffer<MeshBoneRef> g_MeshBoneRefs : register(t1);
RWStructuredBuffer<BoneMatrixElement> g_OutputBones : register(u0);

[numthreads(64, 1, 1)]
void CS_Main(uint3 id : SV_DispatchThreadID)
{
    uint refCount = 0;
    uint stride = 0;

    g_MeshBoneRefs.GetDimensions(refCount, stride);

    if (id.x >= refCount)
        return;

    MeshBoneRef refData = g_MeshBoneRefs[id.x];
    g_OutputBones[id.x].BoneMatrix = mul(refData.offsetMatrix, g_GlobalBones[refData.sourceBoneIndex].BoneMatrix);
}
