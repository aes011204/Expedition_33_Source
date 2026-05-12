#ifndef ENGINE_SHADER_PASSES_HLSL
#define ENGINE_SHADER_PASSES_HLSL

#define PASS_RS_DS_BS_VP(PassName, RS, DS, BS, VS, PS)              \
pass PassName                                                       \
{                                                                   \
    SetRasterizerState(RS);                                         \
    SetDepthStencilState(DS, 0);                                    \
    SetBlendState(BS, float4(0.f, 0.f, 0.f, 0.f), 0xFFFFFFFF);      \
    SetVertexShader(CompileShader(vs_5_0, VS()));                  \
    GeometryShader = NULL;                                          \
    SetPixelShader(CompileShader(ps_5_0, PS()));                   \
}
#define PASS_RS_DS_BS_VGP(PassName, RS, DS, BS, VS, GS, PS)              \
pass PassName                                                       \
{                                                                   \
    SetRasterizerState(RS);                                         \
    SetDepthStencilState(DS, 0);                                    \
    SetBlendState(BS, float4(0.f, 0.f, 0.f, 0.f), 0xFFFFFFFF);      \
    SetVertexShader(CompileShader(vs_5_0, VS()));                  \
    SetGeometryShader(CompileShader(gs_5_0, GS()));                \
    SetPixelShader(CompileShader(ps_5_0, PS()));                   \
}
#endif
