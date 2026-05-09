#pragma once

#include <memory>
#include <wrl/client.h>
#include <directxtk/SimpleMath.h>

namespace Engine
{
    template <typename T>
    using Shared = std::shared_ptr<T>;
    template <typename T>
    using Weak = std::weak_ptr<T>;
    template <typename T>
    using Unique = std::unique_ptr<T>;
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    template<typename Key, typename Value>
    using umap = std::unordered_map<Key, Value>;

    template<typename T>
    using uset = std::unordered_set<T>;

    using int8 = int8_t;
    using uint8 = uint8_t;

    using int16 = int16_t;
    using uint16 = uint16_t;

    using int32 = int32_t;
    using uint32 = uint32_t;

    using int64 = int64_t;
    using uint64 = uint64_t;

    using Vec2 = DirectX::SimpleMath::Vector2;

    struct Vec3 : public DirectX::SimpleMath::Vector3
    {
        using Vector3::Vector3;

        Vec3(const DirectX::SimpleMath::Vector3& v)
            : Vector3(v)
        {
        } // 변환 생성자

        static const Vec3 Look; // LH (DirectX Forward)
    };

    inline const Vec3 Vec3::Look{ 0.f, 0.f, 1.f };

    using Vec4 = DirectX::SimpleMath::Vector4;
    //using Matrix = DirectX::SimpleMath::Matrix;

    using Quat = DirectX::SimpleMath::Quaternion;
    using Color = DirectX::SimpleMath::Color;
    using Ray = DirectX::SimpleMath::Ray;
    using Plane = DirectX::SimpleMath::Plane;

    using Device = ID3D11Device;
    using Context = ID3D11DeviceContext;
    using SwapChain = IDXGISwapChain;
    using RenderTargetView = ID3D11RenderTargetView;
    using DepthStencilView = ID3D11DepthStencilView;
    using Texture2D = ID3D11Texture2D;
    using Buffer = ID3D11Buffer;
    using ShaderResourceView = ID3D11ShaderResourceView;
}
