//// 모델 인스턴싱을 위한 셰이더
//#include "../../../Engine/Bin/ShaderFiles/Engine_Shader_Defines.hlsli"

//float4x4 g_ViewMatrix, g_ProjMatrix;

//Texture2D g_Texture;

//float g_fFar = 0.f;

//sampler DefaultSampler = sampler_state
//{
//    Filter = MIN_MAG_MIP_LINEAR;
//    AddressU = wrap;
//    AddressV = wrap;
//};

//struct VS_IN
//{
//    // slot 0 : 메쉬 정점 (VTXMESH)
//    float3 vPosition : POSITION;
//    float3 vNormal : NORMAL;
//    float3 vTangent : TANGENT;
//    float3 vBinormal : BINORMAL;
//    float2 vTexcoord : TEXCOORD0;

//    // slot 1 : 인스턴스 데이터 (MESHINSTANCEDATA)
//    float3 iPosition : INST_POSITION;
//    float iScale : INST_SCALE;
//    float iWindPhase : INST_WINDPHASE;
//    float iYRotation : INST_YROT;
//    float iColor : INST_COLOR;
//    float iLodIndex : INST_LOD;
//};

//struct VS_OUT
//{
//    float4 vPosition : SV_POSITION;
//    float4 vNormal : NORMAL;
//    float2 vTexcoord : TEXCOORD0;
//    float4 vWorldPos : TEXCOORD1;
//    float4 vProjPos : TEXCOORD2;
//    float vColor : TEXCOORD3;
//};

//VS_OUT VS_MAIN(VS_IN In)
//{
//    VS_OUT Out;

//    float cy = cos(In.iYRotation);
//    float sy = sin(In.iYRotation);
//    float scl = In.iScale;

//    // Scale + RotateY + Translate
//    float4x4 matWorld =
//    {
//        cy * scl, 0.f, sy * scl, 0.f,
//        0.f, scl, 0.f, 0.f,
//       -sy * scl, 0.f, cy * scl, 0.f,
//        In.iPosition.x, In.iPosition.y, In.iPosition.z, 1.f
//    };

//    float4x4 matWVP = mul(mul(matWorld, g_ViewMatrix), g_ProjMatrix);

//    Out.vPosition = mul(float4(In.vPosition, 1.f), matWVP);
//    Out.vNormal = normalize(mul(float4(In.vNormal, 0.f), matWorld));
//    Out.vTexcoord = In.vTexcoord;
//    Out.vWorldPos = mul(float4(In.vPosition, 1.f), matWorld);
//    Out.vProjPos = Out.vPosition;
//    Out.vColor = In.iColor;

//    return Out;
//}

//struct PS_IN
//{
//    float4 vPosition : SV_POSITION;
//    float4 vNormal : NORMAL;
//    float2 vTexcoord : TEXCOORD0;
//    float4 vWorldPos : TEXCOORD1;
//    float4 vProjPos : TEXCOORD2;
//    float vColor : TEXCOORD3;
//};

//struct PS_OUT
//{
//    vector vDiffuse : SV_TARGET0;
//    vector vNormal : SV_TARGET1;
//    vector vDepth : SV_TARGET2;
//};

//PS_OUT PS_MAIN(PS_IN In)
//{
//    PS_OUT Out;

//    vector vDiffuse = g_Texture.Sample(DefaultSampler, In.vTexcoord);

//    if (vDiffuse.a < 0.3f)
//        discard;

//    float brightness = lerp(0.85f, 1.15f, In.vColor);
//    Out.vDiffuse = vector(vDiffuse.rgb * brightness, vDiffuse.a);
//    Out.vNormal = vector(In.vNormal.xyz * 0.5f + 0.5f, 1.f);
//    Out.vDepth = vector(In.vProjPos.z / In.vProjPos.w, In.vProjPos.w / g_fFar, 0.f, 1.f);

//    return Out;
//}

//technique11 DefaultTechnique
//{
//    pass DefaultPass
//    {
//        SetRasterizerState(RS_Default);
//        SetDepthStencilState(DSS_Default, 0);
//        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

//        VertexShader = compile vs_5_0 VS_MAIN();
//        GeometryShader = NULL;
//        PixelShader = compile ps_5_0 PS_MAIN();
//    }
//}
