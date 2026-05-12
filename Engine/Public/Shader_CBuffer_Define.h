#pragma once

NS_BEGIN(Engine)

/* ------------------------------------------ */
/*             Material CBuffer               */
/* ------------------------------------------ */
struct alignas(16) MaterialCB
{
    static constexpr ConstantBuffer CBType = ConstantBuffer::Material;

    Vec4 baseColorTint{ 1.f, 1.f, 1.f, 1.f };
    Vec4 emissiveColor{ 1.f, 1.f, 1.f, 1.f };

    // MI json의 Scalars.Value_Glow 계열
    float emissiveIntensity{ 0.f };

    // MI json의 Scalars.Glow_Mask_Mult 계열
    float emissiveMaskMul{ 1.f };

    // MI json의 Normal/Bump Multiplier 또는 NormalIntensity 계열
    float normalStrength{ 1.f };

    // 셰이더에서 clip 기준으로 사용
    float opacityMaskClipValue{ 0.3333f };

    // ORM 보정값
    float aoScale{ 1.f };           // ORM.r = AO
    float roughnessScale{ 1.f };    // ORM.g = Roughness
    float metalnessScale{ 1.f };    // ORM.b = Metalness
    float _pad0{ 0.f };

    Vec2 uvScale{ 1.f, 1.f };
    Vec2 uvOffset{ 0.f, 0.f };
    float uvRotation{ 0.f };

    uint32 materialMask{ 0 };
    float _pad1[2]{};
};

/* ------------------------------------------ */
/*             Camera CBuffer                 */
/* ------------------------------------------ */
struct alignas(16) CameraCB
{
    static constexpr ConstantBuffer CBType = ConstantBuffer::Camera;

    Matrix  viewMatrix;             // 64
    Matrix  projMatrix;             // 64
    Matrix  viewInverseMatrix;      // 64
    Matrix  projInverseMatrix;      // 64

    Vec4    cameraPosition;         // 16
            
    float   farPlane = 0.f;         // 4
    float   _pad0[3]{};             // 12
};

/* ------------------------------------------ */
/*             Object CBuffer                 */
/* ------------------------------------------ */
struct alignas(16) ObjectCB
{
    static constexpr ConstantBuffer CBType = ConstantBuffer::Object;

    Matrix worldMatrix;
};

/* ------------------------------------------ */
/*             Shadow CBuffer                 */
/* ------------------------------------------ */
struct alignas(16) ShadowCB
{
    static constexpr ConstantBuffer CBType = ConstantBuffer::Shadow;

    Matrix  lightViewMatrix;
    Matrix  lightProjMatrix;

    float   lightFar = 0.f;
    float   bias = 0.f;
    float   intensity = 0.f;
    float   _pad0 = 0.f;
};

/* ------------------------------------------ */
/*             Deferred CBuffer               */
/* ------------------------------------------ */
struct alignas(16) DeferredCB
{
    static constexpr ConstantBuffer CBType = ConstantBuffer::Deferred;

    Matrix  screenViewMatrix;
    Matrix  screenProjMatrix;

    Matrix  viewInverseMatrix;
    Matrix  projInverseMatrix;

    Vec4    cameraPosition;

    float   farPlane = 0.f;
    float   _pad0[3]{};
};

/* ------------------------------------------ */
/*             Light CBuffer                  */
/* ------------------------------------------ */
struct alignas(16) LightCB
{
    static constexpr ConstantBuffer CBType = ConstantBuffer::Light;

    Vec4 direction;
    Vec4 position;

    Vec4 diffuse;
    Vec4 ambient;
    Vec4 specular;

    float range = 0.f;
    float _pad0[3]{};
};

// 패딩이 제대로 됐나 마지막 확인용
static_assert(sizeof(MaterialCB) % 16 == 0);
static_assert(sizeof(CameraCB) % 16 == 0);
static_assert(sizeof(ObjectCB) % 16 == 0);
static_assert(sizeof(ShadowCB) % 16 == 0);
static_assert(sizeof(DeferredCB) % 16 == 0);
static_assert(sizeof(LightCB) % 16 == 0);

NS_END

