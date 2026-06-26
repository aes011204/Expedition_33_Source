// Engine 원본 경로: Engine/Bin/ShaderFiles/Engine_Shader_RenderState.hlsl
// Client 복사본 경로: Client/Bin/ShaderFiles/Engine_Shader_RenderState.hlsl - 먼저 이 원본의 의도를 확인하세요.
#ifndef ENGINE_SHADER_RENDERSTATE_HLSL
#define ENGINE_SHADER_RENDERSTATE_HLSL

RasterizerState RS_Default
{
    FillMode = Solid;
    CullMode = Back;
    FrontCounterClockwise = false;
};

RasterizerState RS_CullFront
{
    FillMode = Solid;
    CullMode = front;
    FrontCounterClockwise = false;
};

RasterizerState RS_UI
{
    FillMode = Solid;
    CullMode = none;
    FrontCounterClockwise = false;
};

DepthStencilState DSS_Default
{
    DepthEnable = true;
    DepthWriteMask = All;
    DepthFunc = less_equal;
};

DepthStencilState DSS_DepthRead
{
    DepthEnable = true;
    DepthWriteMask = zero;
    DepthFunc = less_equal;
};

DepthStencilState DSS_None
{
    DepthEnable = false;
    DepthWriteMask = zero;    
};

DepthStencilState DSS_ScreenFxExclude
{
    DepthEnable = false;
    DepthWriteMask = zero;
    StencilEnable = true;
    StencilReadMask = 0x02;
    StencilWriteMask = 0x00;
    FrontFaceStencilFail = keep;
    FrontFaceStencilDepthFail = keep;
    FrontFaceStencilPass = keep;
    FrontFaceStencilFunc = equal;
    BackFaceStencilFail = keep;
    BackFaceStencilDepthFail = keep;
    BackFaceStencilPass = keep;
    BackFaceStencilFunc = equal;
};

BlendState BS_Default
{
    BlendEnable[0] = false;    
};

BlendState BS_AlphaBlend
{
    BlendEnable[0] = true;
    BlendEnable[1] = true;

    SrcBlend = Src_Alpha;
    DestBlend = Inv_Src_Alpha;
    BlendOp = Add;

    // 이거 안하면, UI 검은색 뒷배경 남아있음
    SrcBlendAlpha = Zero;
    DestBlendAlpha = One;
    BlendOpAlpha = Add;
};

BlendState BS_EffectAlphaBlend
{
    BlendEnable[0] = true;
    BlendEnable[1] = true;

    SrcBlend = Src_Alpha;
    DestBlend = Inv_Src_Alpha;
    BlendOp = Add;

    SrcBlendAlpha = Zero;
    DestBlendAlpha = One;
    BlendOpAlpha = Add;
};

BlendState BS_EffectAdditive
{
    BlendEnable[0] = true;
    BlendEnable[1] = true;

    SrcBlend = One;
    DestBlend = One;
    BlendOp = Add;

    SrcBlendAlpha = Zero;
    DestBlendAlpha = One;
    BlendOpAlpha = Add;
};

BlendState BS_Blend
{
    BlendEnable[0] = true;
    BlendEnable[1] = true;
    BlendEnable[2] = true;

    SrcBlend = one;
    DestBlend = one;
    BlendOp = Add;
};

BlendState BS_Multiply
{
    BlendEnable[0] = true;
    BlendEnable[1] = true;

    SrcBlend = Dest_Color;
    DestBlend = Zero;
    BlendOp = Add;

    SrcBlendAlpha = Zero;
    DestBlendAlpha = One;
    BlendOpAlpha = Add;
};

RasterizerState RS_CullNone
{
    FillMode = Solid;
    CullMode = None;
    FrontCounterClockwise = false;
};
#endif
