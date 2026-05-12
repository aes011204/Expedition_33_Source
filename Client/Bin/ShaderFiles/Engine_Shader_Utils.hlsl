#ifndef ENGINE_SHADER_UTILS_HLSL
#define ENGINE_SHADER_UTILS_HLSL

float Remap(float value, float inMin, float inMax, float outMin, float outMax)
{
    return outMin + (value - inMin) * (outMax - outMin) / (inMax - inMin);
}

float SafeDivide(float a, float b, float fallback)
{
    return abs(b) <= 0.00001f ? fallback : a / b;
}

float3 DecodeNormal(float3 encodedNormal)
{
    return encodedNormal * 2.f - 1.f;
}

float3 EncodeNormal(float3 normal)
{
    return normal * 0.5f + 0.5f;
}

#endif
