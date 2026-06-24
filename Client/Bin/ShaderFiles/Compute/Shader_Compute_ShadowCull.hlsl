struct ShadowCascadeData
{
    float4 planes[32];
    uint planeCount;
    float3 _pad;
};

cbuffer ShadowCullCB : register(b1)
{
    float4 g_Planes[32];
    uint g_PlaneCount;
    uint g_TotalCount;
    float2 _pad;
};

StructuredBuffer<float4x4> g_Instances : register(t0);
StructuredBuffer<float4> g_Spheres : register(t1);
AppendStructuredBuffer<float4x4> g_ShadowVisible : register(u0);

[numthreads(64, 1, 1)]
void CS_Main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= g_TotalCount)
        return;

    float4 sphere = g_Spheres[id.x];
    float3 center = sphere.xyz;
    float radius = sphere.w + 10.f;

    float visible = 1.f;
    for (uint i = 0; i < g_PlaneCount; ++i)
    {
        float4 p = g_Planes[i];
        visible *= step(dot(p.xyz, center) + p.w, radius);
    }

    if (visible >= 0.5f)
        g_ShadowVisible.Append(g_Instances[id.x]);
}
