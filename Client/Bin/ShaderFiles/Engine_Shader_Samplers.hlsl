#ifndef ENGINE_SHADER_SAMPLERS_HLSL
#define ENGINE_SHADER_SAMPLERS_HLSL

SamplerState LinearWrapSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

SamplerState LinearClampSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerState PointClampSampler
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerState LinearSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
};


#endif
