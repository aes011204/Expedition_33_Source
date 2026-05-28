#pragma once
#include <cmath>

namespace NoiseUtil
{
    inline float Perlin_Fade(float t)
    {
        return t * t * t * (t * (t * 6.f - 15.f) + 10.f);
    }

    inline float Perlin_Lerp(float a, float b, float t)
    {
        return a + t * (b - a);
    }

    inline float Perlin_Grad(int hash, float x, float y)
    {
        switch (hash & 3)
        {
        case 0:
            return x + y;
        case 1:
            return -x + y;
        case 2:
            return x - y;
        default:
            return -x - y;
        }
    }

    inline float Perlin_Noise2D(float x, float y, const int* perm)
    {
        const int xi = static_cast<int>(floorf(x)) & 255;
        const int yi = static_cast<int>(floorf(y)) & 255;
        const float xf = x - floorf(x);
        const float yf = y - floorf(y);
        const float u = Perlin_Fade(xf);
        const float v = Perlin_Fade(yf);

        const int aa = perm[perm[xi] + yi];
        const int ab = perm[perm[xi] + yi + 1];
        const int ba = perm[perm[xi + 1] + yi];
        const int bb = perm[perm[xi + 1] + yi + 1];
         
        return Perlin_Lerp(
            Perlin_Lerp(Perlin_Grad(aa, xf, yf), Perlin_Grad(ba, xf - 1.f, yf), u),
            Perlin_Lerp(Perlin_Grad(ab, xf, yf - 1.f), Perlin_Grad(bb, xf - 1.f, yf - 1.f), u),
            v);
    }
};

