// Skins one mesh into its range of the vertex pool. Vertices are read and written as 32-bit words,
// because DXC may lay out a struct of float3 fields differently for SPIR-V than the packed C++
// Vertex and SkinnedVertex. Word offsets match the static_asserts in Renderer/Skinning.cpp.
#define SOURCE_WORDS 22         // SkinnedVertex: position, normal, tangent, bitangent, uv, weights, joints
#define DESTINATION_WORDS 14    // Vertex: position, normal, tangent, bitangent, uv

struct PaletteMatrix
{
    float4x4 Transform;
};

StructuredBuffer<uint> Source : register(t0, space1);
StructuredBuffer<PaletteMatrix> Palette : register(t1, space1);
RWStructuredBuffer<uint> Destination : register(u2, space1);

struct SkinningConstants
{
    uint VertexCount;
    uint DestinationBaseVertex;
    uint PaletteIndex;
    uint Padding;
};

[[vk::push_constant]]
ConstantBuffer<SkinningConstants> Constants : register(b0, space999);

float3 LoadFloat3(uint word)
{
    return asfloat(uint3(Source[word], Source[word + 1], Source[word + 2]));
}

void StoreFloat3(uint word, float3 value)
{
    uint3 bits = asuint(value);
    Destination[word] = bits.x;
    Destination[word + 1] = bits.y;
    Destination[word + 2] = bits.z;
}

[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= Constants.VertexCount)
        return;

    uint source = id.x * SOURCE_WORDS;
    float4 weights = asfloat(uint4(Source[source + 14], Source[source + 15], Source[source + 16], Source[source + 17]));
    uint4 joints = uint4(Source[source + 18], Source[source + 19], Source[source + 20], Source[source + 21]);

    // A vertex with no weights keeps its bind position.
    float4x4 skin = (float4x4)0;
    if (dot(weights, 1.0) > 0.0)
    {
        for (int i = 0; i < 4; i++)
            if (weights[i] > 0.0)
                skin += weights[i] * Palette[Constants.PaletteIndex + joints[i]].Transform;
    }
    else
        skin = float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);

    // Positions take the whole matrix; directions its upper 3 x 3, which is exact for rotation and
    // uniform scale only.
    float3x3 basis = (float3x3)skin;
    uint destination = (Constants.DestinationBaseVertex + id.x) * DESTINATION_WORDS;
    StoreFloat3(destination, mul(skin, float4(LoadFloat3(source), 1.0)).xyz);
    StoreFloat3(destination + 3, normalize(mul(basis, LoadFloat3(source + 3))));
    StoreFloat3(destination + 6, normalize(mul(basis, LoadFloat3(source + 6))));
    StoreFloat3(destination + 9, normalize(mul(basis, LoadFloat3(source + 9))));
    Destination[destination + 12] = Source[source + 12];
    Destination[destination + 13] = Source[source + 13];
}
