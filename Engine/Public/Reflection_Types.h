#pragma once

namespace Engine
{
    enum class RegisteredTypeKind
    {
        Component,
        GameObject,
        BTNode,
    };

    struct PropertyReflectionInfo
    {
        std::string name = {};
        std::string displayName = {};
        std::string typeName = {};
        size_t offset = 0;
    };

    struct ClassReflectionInfo
    {
        std::string className = {};     // 클래스 이름
        std::string displayName = {};   // 에디터에 띄울 이름
        std::string category = {};      // 카테고리 나누기
        RegisteredTypeKind kind = RegisteredTypeKind::Component;

        std::vector<PropertyReflectionInfo> properties{};
    };

}
