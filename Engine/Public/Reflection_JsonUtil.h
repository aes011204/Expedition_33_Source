#pragma once

#include "Reflection_Types.h"
#include "Engine_Typedef.h"

namespace Engine::Reflection
{
    static void Write_ReflectionProperty
    (
        json& root,
        const PropertyInfo& prop,
        const void* memberPtr)
    {
        const string key = prop.jsonKey.empty() ? prop.name : prop.jsonKey;

        switch (prop.type)
        {
        case PropertyType::Bool:
            root[key] = *static_cast<const bool*>(memberPtr);
            break;

        case PropertyType::Int:
            root[key] = *static_cast<const int*>(memberPtr);
            break;

        case PropertyType::Float:
            root[key] = *static_cast<const float*>(memberPtr);
            break;

        case PropertyType::Vec2:
        {
            const Vec2* v = static_cast<const Vec2*>(memberPtr);
            root[key] = { v->x, v->y };
            break;
        }

        case PropertyType::Vec3:
        {
            const Vec3* v = static_cast<const Vec3*>(memberPtr);
            root[key] = { v->x, v->y, v->z };
            break;
        }

        case PropertyType::Vec4:
        {
            const Vec4* v = static_cast<const Vec4*>(memberPtr);
            root[key] = { v->x, v->y, v->z, v->w };
            break;
        }

        case PropertyType::Color:
        {
            const Color* c = static_cast<const Color*>(memberPtr);
            root[key] = { c->x, c->y, c->z, c->w };
            break;
        }

        case PropertyType::String:
            root[key] = *static_cast<const string*>(memberPtr);
            break;

        case PropertyType::Enum:
            root[key] = *reinterpret_cast<const int*>(memberPtr);
            break;

        default:
            break;
        }
    }

    inline void Read_ReflectionProperty(
        const json& root,
        const PropertyInfo& prop,
        void* memberPtr)
    {
        const string key = prop.jsonKey.empty() ? prop.name : prop.jsonKey;
        if (!root.contains(key))
            return;

        switch (prop.type)
        {
        case PropertyType::Bool:
            *static_cast<bool*>(memberPtr) = root[key].get<bool>();
            break;

        case PropertyType::Int:
            *static_cast<int*>(memberPtr) = root[key].get<int>();
            break;

        case PropertyType::Float:
            *static_cast<float*>(memberPtr) = root[key].get<float>();
            break;

        case PropertyType::Vec2:
        {
            const json& arr = root[key];
            if (!arr.is_array() || arr.size() < 2)
                break;

            Vec2* v = static_cast<Vec2*>(memberPtr);
            v->x = arr[0].get<float>();
            v->y = arr[1].get<float>();
            break;
        }

        case PropertyType::Vec3:
        {
            const json& arr = root[key];
            if (!arr.is_array() || arr.size() < 3)
                break;

            Vec3* v = static_cast<Vec3*>(memberPtr);
            v->x = arr[0].get<float>();
            v->y = arr[1].get<float>();
            v->z = arr[2].get<float>();
            break;
        }

        case PropertyType::Vec4:
        {
            const json& arr = root[key];
            if (!arr.is_array() || arr.size() < 4)
                break;

            Vec4* v = static_cast<Vec4*>(memberPtr);
            v->x = arr[0].get<float>();
            v->y = arr[1].get<float>();
            v->z = arr[2].get<float>();
            v->w = arr[3].get<float>();
            break;
        }

        case PropertyType::Color:
        {
            const json& arr = root[key];
            if (!arr.is_array() || arr.size() < 4)
                break;

            Color* c = static_cast<Color*>(memberPtr);
            c->x = arr[0].get<float>();
            c->y = arr[1].get<float>();
            c->z = arr[2].get<float>();
            c->w = arr[3].get<float>();
            break;
        }

        case PropertyType::String:
            *static_cast<string*>(memberPtr) = root[key].get<string>();
            break;

        case PropertyType::Enum:
            *reinterpret_cast<int*>(memberPtr) = root[key].get<int>();
            break;

        default:
            break;
        }
    }
}
