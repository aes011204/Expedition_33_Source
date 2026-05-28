#include "IBLBaker.h"

#include <DirectXTex.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>

NS_BEGIN(Engine)

namespace
{
    constexpr float PI = 3.14159265358979323846f;
    constexpr float EPSILON = 1e-6f;

    constexpr size_t BRDF_LUT_SIZE = 512;
    constexpr uint32_t BRDF_SAMPLE_COUNT = 1024;

    struct Float2
    {
        float x = 0.f;
        float y = 0.f;
    };

    struct Float3
    {
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;
    };

    Float3 operator+(const Float3& lhs, const Float3& rhs)
    {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Float3 operator-(const Float3& lhs, const Float3& rhs)
    {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    }

    Float3 operator*(const Float3& lhs, float rhs)
    {
        return { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs };
    }

    Float3 operator/(const Float3& lhs, float rhs)
    {
        const float inv = 1.f / rhs;
        return lhs * inv;
    }

    float Dot(const Float3& lhs, const Float3& rhs)
    {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
    }

    float Length(const Float3& value)
    {
        return std::sqrt(Dot(value, value));
    }

    Float3 Normalize(const Float3& value)
    {
        const float len = Length(value);
        if (len <= EPSILON)
            return { 0.f, 0.f, 1.f };

        return value / len;
    }

    float Saturate(float value)
    {
        return std::clamp(value, 0.f, 1.f);
    }

    float RadicalInverse_VdC(uint32_t bits)
    {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

        return static_cast<float>(bits) * 2.3283064365386963e-10f;
    }

    Float2 Hammersley(uint32_t i, uint32_t sampleCount)
    {
        return
        {
            static_cast<float>(i) / static_cast<float>(sampleCount),
            RadicalInverse_VdC(i)
        };
    }

    Float3 ImportanceSampleGGX(const Float2& xi, const Float3& n, float roughness)
    {
        const float a = roughness * roughness;
        const float a2 = a * a;

        const float phi = 2.f * PI * xi.x;
        const float cosTheta = std::sqrt((1.f - xi.y) / (1.f + (a2 - 1.f) * xi.y));
        const float sinTheta = std::sqrt(std::max(0.f, 1.f - cosTheta * cosTheta));

        Float3 h{};
        h.x = std::cos(phi) * sinTheta;
        h.y = std::sin(phi) * sinTheta;
        h.z = cosTheta;

        /*
            BRDF LUT 적분에서는 N을 (0, 0, 1)로 두고 계산하므로
            tangent space 변환 없이 h를 그대로 써도 됩니다.
        */
        return Normalize(h);
    }

    float GeometrySchlickGGX(float nDotV, float roughness)
    {
        const float a = roughness;
        const float k = (a * a) * 0.5f;

        const float denom = nDotV * (1.f - k) + k;
        return nDotV / std::max(denom, EPSILON);
    }

    float GeometrySmith(float nDotV, float nDotL, float roughness)
    {
        const float ggxV = GeometrySchlickGGX(nDotV, roughness);
        const float ggxL = GeometrySchlickGGX(nDotL, roughness);

        return ggxV * ggxL;
    }

    Float2 IntegrateBRDF(float nDotV, float roughness)
    {
        Float3 v{};
        v.x = std::sqrt(std::max(0.f, 1.f - nDotV * nDotV));
        v.y = 0.f;
        v.z = nDotV;

        const Float3 n = { 0.f, 0.f, 1.f };

        float scale = 0.f;
        float bias = 0.f;

        for (uint32_t i = 0; i < BRDF_SAMPLE_COUNT; ++i)
        {
            const Float2 xi = Hammersley(i, BRDF_SAMPLE_COUNT);

            const Float3 h = ImportanceSampleGGX(xi, n, roughness);
            const Float3 l = Normalize(h * (2.f * Dot(v, h)) - v);

            const float nDotL = Saturate(l.z);
            const float nDotH = Saturate(h.z);
            const float vDotH = Saturate(Dot(v, h));

            if (nDotL > 0.f)
            {
                const float g = GeometrySmith(nDotV, nDotL, roughness);
                const float gVis = (g * vDotH) / std::max(nDotH * nDotV, EPSILON);
                const float fc = std::pow(1.f - vDotH, 5.f);

                scale += (1.f - fc) * gVis;
                bias += fc * gVis;
            }
        }

        const float invSampleCount = 1.f / static_cast<float>(BRDF_SAMPLE_COUNT);

        return
        {
            scale * invSampleCount,
            bias * invSampleCount
        };
    }

    bool BakeBRDFLUT(const std::filesystem::path& outputPath)
    {
        DirectX::ScratchImage brdfLUT{};

        /*
            R32G32_FLOAT로 저장합니다.
            용량을 줄이고 싶으면 나중에 R16G16_FLOAT로 바꿔도 됩니다.
            HLSL에서는 .rg만 읽으면 됩니다.
        */
        HRESULT hr = brdfLUT.Initialize2D(
            DXGI_FORMAT_R32G32_FLOAT,
            BRDF_LUT_SIZE,
            BRDF_LUT_SIZE,
            1,
            1);

        if (FAILED(hr))
            return false;

        DirectX::Image* image = brdfLUT.GetImage(0, 0, 0);
        if (image == nullptr)
            return false;

        for (size_t y = 0; y < BRDF_LUT_SIZE; ++y)
        {
            Float2* dst = reinterpret_cast<Float2*>(image->pixels + image->rowPitch * y);

            for (size_t x = 0; x < BRDF_LUT_SIZE; ++x)
            {
                const float nDotV = (static_cast<float>(x) + 0.5f) / static_cast<float>(BRDF_LUT_SIZE);
                const float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(BRDF_LUT_SIZE);

                dst[x] = IntegrateBRDF(nDotV, roughness);
            }
        }

        hr = DirectX::SaveToDDSFile(
            brdfLUT.GetImages(),
            brdfLUT.GetImageCount(),
            brdfLUT.GetMetadata(),
            DirectX::DDS_FLAGS_NONE,
            outputPath.c_str());

        return SUCCEEDED(hr);
    }
}

bool IBLBaker::BakeBRDFLUTOnly(const wstring& outputPath)
{
    return BakeBRDFLUT(outputPath);
}

NS_END
