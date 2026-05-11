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

DepthStencilState DSS_None
{
    DepthEnable = false;
    DepthWriteMask = zero;    
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

BlendState BS_Blend
{
    BlendEnable[0] = true;
    BlendEnable[1] = true;

    SrcBlend = one;
    DestBlend = one;
    BlendOp = Add;
};

RasterizerState RS_CullNone
{
    FillMode = Solid;
    CullMode = None;
    FrontCounterClockwise = false;
};

#endif
