cbuffer CullParams : register(b0)
{
    float4 g_FrustumPlanes[6];
    uint g_TotalCount;
    float g_MaxDist;
    float g_LOD1Dist;
    float g_LOD2Dist;
    float3 g_CamPos;
    float g_Pad2;
};

StructuredBuffer<float4x4> g_Instances : register(t0);
StructuredBuffer<float4> g_Spheres : register(t1);

AppendStructuredBuffer<float4x4> g_Visible_LOD0 : register(u0);
AppendStructuredBuffer<float4x4> g_Visible_LOD1 : register(u1);
AppendStructuredBuffer<float4x4> g_Visible_LOD2 : register(u2);

[numthreads(64, 1, 1)]
void CS_Main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= g_TotalCount)
        return;

    float4 sphere = g_Spheres[id.x];
    float3 center = sphere.xyz;
    float radius = sphere.w;

    float visible = 1.f;

    [unroll]
    for (int i = 0; i < 6; ++i)
        visible *= step(0.f, dot(g_FrustumPlanes[i].xyz, center) + g_FrustumPlanes[i].w + radius);

    float dist = length(center - g_CamPos);
    visible *= step(dist, g_MaxDist + radius);

    if (visible < 0.5f)
        return;

    if (dist < g_LOD1Dist)
        g_Visible_LOD0.Append(g_Instances[id.x]);
    else if (dist < g_LOD2Dist)
        g_Visible_LOD1.Append(g_Instances[id.x]);
    else
        g_Visible_LOD2.Append(g_Instances[id.x]);
}
