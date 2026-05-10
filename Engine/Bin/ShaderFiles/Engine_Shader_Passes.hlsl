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

#endif
