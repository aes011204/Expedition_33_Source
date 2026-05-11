#pragma once

#include "Engine_Typedef.h"

namespace Engine
{
    typedef struct tagVertexPositionTexcoord
    {
        Vec3            position;
        Vec2            texcoord;

        static const unsigned int        numElements = { 2 };

        static constexpr D3D11_INPUT_ELEMENT_DESC    Elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
    }VTXTEX;

    typedef struct tagVertexCube
    {
        Vec3            position;
        Vec3            texcoord;

        static const unsigned int        numElements = { 2 };

        static constexpr D3D11_INPUT_ELEMENT_DESC    Elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
    }VTXCUBE;

    typedef struct tagVertexPositionNormalTexcoord
    {
        Vec3            position;
        Vec3            normal;
        Vec2            texcoord;

        static const unsigned int        numElements = { 3 };

        static constexpr D3D11_INPUT_ELEMENT_DESC    Elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
    }VTXNORTEX;

    typedef struct tagVertexMesh
    {
        Vec3            position;
        Vec3            normal;
        Vec3            tangent;
        Vec3            binormal;
        Vec2            texcoord;

        static const unsigned int        numElements = { 5 };

        static constexpr D3D11_INPUT_ELEMENT_DESC    Elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
    }VTXMESH;

    typedef struct tagVertexAnimationMesh
    {
        Vec3            position;
        Vec3            normal;
        Vec3            tangent;
        Vec3            binormal;
        Vec2            texcoord;
        DirectX::XMUINT4        blendIndex;
        Vec4                    blendWeight;

        static const unsigned int        numElements = { 7 };

        static constexpr D3D11_INPUT_ELEMENT_DESC    Elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BLENDINDEX", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 56, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 72, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
    }VTXANIMMESH;

    typedef struct tagMeshInstanceData
    {
        Vec3 position; // 12
        float scale; // 4
        float windPhase; // 4
        float yRotation; // 4
        float colorVariation; // 4
        float lodIndex; // 4

        static const unsigned int numElements = { 6 };
        static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
        { "INST_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_SCALE",    0, DXGI_FORMAT_R32_FLOAT,       1, 12, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_WINDPHASE",0, DXGI_FORMAT_R32_FLOAT,       1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_YROT",     0, DXGI_FORMAT_R32_FLOAT,       1, 20, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_COLOR",    0, DXGI_FORMAT_R32_FLOAT,       1, 24, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_LOD",      0, DXGI_FORMAT_R32_FLOAT,       1, 28, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        };
    } MESHINSTANCEDATA;

    // for shader_VtxMeshInstance
    struct VtxMeshInstanceLayout
    {
        static constexpr uint32 numElements = 11;

        static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
            // slot 0: VTXMESH
            { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA,   0 },
            { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA,   0 },
            { "TANGENT",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA,   0 },
            { "BINORMAL",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA,   0 },
            { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 48, D3D11_INPUT_PER_VERTEX_DATA,   0 },
            // slot 1: MESHINSTANCEDATA
            { "INST_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1,  0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "INST_SCALE",    0, DXGI_FORMAT_R32_FLOAT,       1, 12, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "INST_WINDPHASE",0, DXGI_FORMAT_R32_FLOAT,       1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "INST_YROT",     0, DXGI_FORMAT_R32_FLOAT,       1, 20, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "INST_COLOR",    0, DXGI_FORMAT_R32_FLOAT,       1, 24, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "INST_LOD",      0, DXGI_FORMAT_R32_FLOAT,       1, 28, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        };
    };
}
