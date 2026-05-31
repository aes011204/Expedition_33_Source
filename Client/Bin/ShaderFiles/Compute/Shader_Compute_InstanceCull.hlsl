cbuffer CullParams : register(b0)
{
    float4 g_FrustumPlanes[6];
    uint g_TotalCount;
    float g_MaxDist;
    float2 g_Pad;
    float3 g_CamPos;
    float g_Pad2;
};

StructuredBuffer<float4x4> g_Instances : register(t0);
StructuredBuffer<float4> g_Spheres : register(t1);

AppendStructuredBuffer<float4x4> g_Visible : register(u0);

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
    {
        //if (dot(g_FrustumPlanes[i].xyz, center) + g_FrustumPlanes[i].w + radius < 0.f)
        //    return;
        visible *= step(0.f, dot(g_FrustumPlanes[i].xyz, center) + g_FrustumPlanes[i].w + radius);
    }

    float dist = length(center - g_CamPos);
    visible *= step(dist, g_MaxDist + radius);

    if (visible > 0.5f)
        g_Visible.Append(g_Instances[id.x]);
}
