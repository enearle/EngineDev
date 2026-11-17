#pragma once
#include <span>
#include <vector>
#include <DirectXMath.h>
#include "Mesh.h"

class Skeleton;

// Measures a mesh's UV chart for procedural surfaces: its size in millimetres along U and V, the
// median over triangles of the length one UV unit spans, and for a skinned mesh its chart map
// (see MeshChart).
class ChartBuilder
{
public:
    struct ChartVertex
    {
        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT3 Normal;
        DirectX::XMFLOAT2 TexCoord;
        int32_t MainJoint = -1;         // The joint with the largest weight, or -1
    };

    // millimetresPerUnit converts positions, which are in the file's unit. Without a skeleton only
    // the size is measured.
    static MeshChart Build(std::span<const ChartVertex> vertices, std::span<const uint32_t> indices, const Skeleton* skeleton,
                           float millimetresPerUnit);
};
