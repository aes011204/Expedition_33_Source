#include "EffectAuthoringModuleMetadata.h"

NS_BEGIN(EffectEditor)

const AuthoringModuleMetadata& Get_AuthoringModuleMetadata(AuthoringModuleType type)
{
    static const AuthoringModuleMetadata required{
        "Required",
        L"필수",
        L"텍스처, 머티리얼",
        false,
        false,
        true,
        true
    };
    static const AuthoringModuleMetadata spawn{
        "Spawn",
        L"스폰",
        L"생성량 / 루프 기준",
        false,
        false
    };
    static const AuthoringModuleMetadata lifetime{
        "Lifetime",
        L"수명",
        L"파티클 수명 범위",
        true,
        true,
        true,
        true
    };
    static const AuthoringModuleMetadata initialLocation{
        "Initial Location",
        L"초기 위치",
        L"생성 시점 위치 분포"
    };
    static const AuthoringModuleMetadata sphereLocation{
        "Sphere Location",
        L"구 위치",
        L"구 표면/부피 생성 위치"
    };
    static const AuthoringModuleMetadata initialSize{
        "Initial Size",
        L"초기 크기",
        L"생성 시점 크기"
    };
    static const AuthoringModuleMetadata initialMeshSize{
        "Initial Mesh Size",
        L"초기 메시 크기",
        L"생성 시점 메시 XYZ 크기"
    };
    static const AuthoringModuleMetadata initialVelocity{
        "Initial Velocity",
        L"초기 속도",
        L"생성 시점 벡터 속도"
    };
    static const AuthoringModuleMetadata initialRadialVelocity{
        "Initial Radial Velocity",
        L"초기 방사성 속도",
        L"생성 위치 기준 방사성 속도"
    };
    static const AuthoringModuleMetadata acceleration{
        "Acceleration",
        L"가속",
        L"시간에 따라 더해지는 속도"
    };
    static const AuthoringModuleMetadata drag{
        "Drag",
        L"드래그",
        L"속도 감쇠"
    };
    static const AuthoringModuleMetadata velocityOverLife{
        "Velocity Over Life",
        L"수명에 따른 속도",
        L"수명 진행률 기준 속도 배율"
    };
    static const AuthoringModuleMetadata initialRotation{
        "Initial Rotation",
        L"초기 회전",
        L"생성 시점 회전각"
    };
    static const AuthoringModuleMetadata rotationOverLife{
        "Rotation Over Life",
        L"수명에 따른 회전",
        L"수명 진행률 기준 추가 회전"
    };
    static const AuthoringModuleMetadata initialRotationRate{
        "Initial Rotation Rate",
        L"초기 회전 속도",
        L"초당 회전 속도"
    };
    static const AuthoringModuleMetadata rotationRateScaleByLife{
        "Rotation Rate Scale By Life",
        L"수명에 따른 회전 속도",
        L"수명 진행률 기준 회전 속도 배율"
    };
    static const AuthoringModuleMetadata initialMeshRotation{
        "Initial Mesh Rotation",
        L"초기 메시 회전",
        L"생성 시점 메시 XYZ 회전"
    };
    static const AuthoringModuleMetadata meshRotationOverLife{
        "Mesh Rotation Over Life",
        L"수명에 따른 메시 회전",
        L"수명 진행률 기준 메시 XYZ 추가 회전"
    };
    static const AuthoringModuleMetadata initialMeshRotationRate{
        "Initial Mesh Rotation Rate",
        L"초기 메시 회전 속도",
        L"초당 메시 XYZ 회전 속도"
    };
    static const AuthoringModuleMetadata meshRotationRateScaleByLife{
        "Mesh Rotation Rate Scale By Life",
        L"수명에 따른 메시 회전 속도",
        L"수명 진행률 기준 메시 XYZ 회전 속도 배율"
    };
    static const AuthoringModuleMetadata initialColor{
        "Initial Color",
        L"초기 컬러",
        L"생성 시점 색상 / 알파",
        true,
        true,
        true,
        true
    };
    static const AuthoringModuleMetadata colorOverLife{
        "Color Over Life",
        L"수명에 따른 색상",
        L"수명 진행률 기준 색 변화",
        true,
        true,
        true,
        true
    };
    static const AuthoringModuleMetadata subUVFrameOverLife{
        "SubUV Frame Over Life",
        L"서브UV 프레임 선택/재생",
        L"수명 진행률 기준 frame 선택"
    };
    static const AuthoringModuleMetadata sizeByLife{
        "Size By Life",
        L"수명에 따른 크기",
        L"수명 진행률 기준 크기 배율",
        true,
        true,
        true,
        true
    };
    static const AuthoringModuleMetadata meshSizeByLife{
        "Mesh Size By Life",
        L"수명에 따른 메시 크기",
        L"수명 진행률 기준 메시 XYZ 크기 배율",
        true,
        true,
        true,
        false
    };
    static const AuthoringModuleMetadata spawnPerUnit{
        "Spawn Per Unit",
        L"거리당 스폰",
        L"Trail sample 삽입 밀도",
        true,
        true,
        false,
        true
    };
    static const AuthoringModuleMetadata materialScalarModulation{
        "MaterialScalarModulation",
        L"머티리얼 변조",
        L"머티리얼 scalar 시간 변화",
        true,
        true,
        true,
        true
    };

    switch (type)
    {
    case AuthoringModuleType::Required:
        return required;
    case AuthoringModuleType::Spawn:
        return spawn;
    case AuthoringModuleType::Lifetime:
        return lifetime;
    case AuthoringModuleType::InitialLocation:
        return initialLocation;
    case AuthoringModuleType::SphereLocation:
        return sphereLocation;
    case AuthoringModuleType::InitialSize:
        return initialSize;
    case AuthoringModuleType::InitialMeshSize:
        return initialMeshSize;
    case AuthoringModuleType::InitialVelocity:
        return initialVelocity;
    case AuthoringModuleType::InitialRadialVelocity:
        return initialRadialVelocity;
    case AuthoringModuleType::Acceleration:
        return acceleration;
    case AuthoringModuleType::Drag:
        return drag;
    case AuthoringModuleType::VelocityOverLife:
        return velocityOverLife;
    case AuthoringModuleType::InitialRotation:
        return initialRotation;
    case AuthoringModuleType::RotationOverLife:
        return rotationOverLife;
    case AuthoringModuleType::InitialRotationRate:
        return initialRotationRate;
    case AuthoringModuleType::RotationRateScaleByLife:
        return rotationRateScaleByLife;
    case AuthoringModuleType::InitialMeshRotation:
        return initialMeshRotation;
    case AuthoringModuleType::MeshRotationOverLife:
        return meshRotationOverLife;
    case AuthoringModuleType::InitialMeshRotationRate:
        return initialMeshRotationRate;
    case AuthoringModuleType::MeshRotationRateScaleByLife:
        return meshRotationRateScaleByLife;
    case AuthoringModuleType::InitialColor:
        return initialColor;
    case AuthoringModuleType::ColorOverLife:
        return colorOverLife;
    case AuthoringModuleType::SubUVFrameOverLife:
        return subUVFrameOverLife;
    case AuthoringModuleType::SizeByLife:
        return sizeByLife;
    case AuthoringModuleType::MeshSizeByLife:
        return meshSizeByLife;
    case AuthoringModuleType::SpawnPerUnit:
        return spawnPerUnit;
    case AuthoringModuleType::MaterialScalarModulation:
        return materialScalarModulation;
    }

    return required;
}

void Apply_AuthoringModuleMetadata(AuthoringModule& module)
{
    const AuthoringModuleMetadata& metadata = Get_AuthoringModuleMetadata(module.type);
    module.key = metadata.key;
    module.displayName = metadata.displayName;
    module.summary = metadata.summary;
    module.movable = metadata.movable;
    module.removable = metadata.removable;
}

NS_END
