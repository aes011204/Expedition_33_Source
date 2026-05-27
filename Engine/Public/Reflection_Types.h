#pragma once

namespace Engine
{
    // 이거 세개면 될거같은데
    enum class RegisteredType
    {
        None,
        Component,
        GameObject,
        BTNode,
        AnimNotify,
        AnimNotifyState,
    };

    // 리플렉션 프로퍼티 종류
    enum class PropertyType
    {
        Bool,
        Int,
        Float,
        Vec2,
        Vec3,
        Vec4,
        Color,
        String,
        WString,
        Enum,
        ReadOnly, // 수정 불가
    };


    enum class BlackboardValueType
    {
        None,
        Bool,
        Int,
        Float,
        Vec3
    };

    enum class BTNodeState
    {
        RUN,
        SUCCESS,
        FAILED
    };



    struct PropertyInfo
    {
        std::string name = {};
        std::string displayName = {};
        std::string jsonKey = {};
        PropertyType type = PropertyType::ReadOnly;
        size_t offset = 0;

        float minVal = 0.f;
        float maxVal = 0.f;
        float dragSpeed = 0.1f;

        std::vector<std::string> enumNames = {};
        std::vector<int> enumValues = {};

        bool isBlackboardKey = false;
        BlackboardValueType blackboardValueType = BlackboardValueType::None;
    };

    struct ClassReflectionInfo
    {
        std::string className = {};
        std::string displayName = {};
        std::string category = {};
        RegisteredType type = RegisteredType::Component;
        std::vector<PropertyInfo> properties = {};
    };

    

    struct BTBlackboardKeyView
    {
        std::string name;
        Engine::BlackboardValueType type = Engine::BlackboardValueType::None;
    };
    

}
