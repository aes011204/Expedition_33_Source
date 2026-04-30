#pragma once

#include "Engine_Typedef.h"

namespace Engine
{
    typedef struct tagVertexPositionTexcoord
    {
        Vec3            vPosition;
        Vec2            vTexcoord;

        static const unsigned int        iNumElements = { 2 };

        static constexpr D3D11_INPUT_ELEMENT_DESC    Elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
    }VTXTEX;

    typedef struct tagVertexCube
    {
        Vec3            vPosition;
        Vec3            vTexcoord;

        static const unsigned int        iNumElements = { 2 };

        static constexpr D3D11_INPUT_ELEMENT_DESC    Elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
    }VTXCUBE;

    typedef struct tagVertexPositionNormalTexcoord
    {
        Vec3            vPosition;
        Vec3            vNormal;
        Vec2            vTexcoord;

        static const unsigned int        iNumElements = { 3 };

        static constexpr D3D11_INPUT_ELEMENT_DESC    Elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
    }VTXNORTEX;

    typedef struct tagVertexMesh
    {
        Vec3            vPosition;
        Vec3            vNormal;
        Vec3            vTangent;
        Vec2            vTexcoord;

        static const unsigned int        iNumElements = { 4 };

        static constexpr D3D11_INPUT_ELEMENT_DESC    Elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
    }VTXMESH;

    typedef struct tagVertexAnimationMesh
    {
        Vec3            vPosition;
        Vec3            vNormal;
        Vec3            vTangent;
        Vec2            vTexcoord;

        Vec4            vBlendIndex;
        Vec4            vBlendWeight;

        static const unsigned int        iNumElements = { 6 };

        static constexpr D3D11_INPUT_ELEMENT_DESC    Elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BLENDINDEX", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 60, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
    }VTXANIMMESH;
}
