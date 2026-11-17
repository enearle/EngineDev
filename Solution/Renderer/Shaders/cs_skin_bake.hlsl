// Bakes one skin page into a scratch slot: every texel of the 136 x 136 slot, gutter included, is
// the parent's quadrant upsampled with a 16-tap Catmull-Rom filter (zero at level 0) plus each band
// whose bake weight grew between the parent's level and this one, scaled by the growth.
#include "Skin.hlsli"

#define SLOT_SIZE 136
#define GUTTER 4

Texture2D<float4> AtlasPigments : register(t0, space1);
Texture2D<float4> AtlasHeightSlope : register(t1, space1);
[[vk::image_format("rgba8")]]
RWTexture2D<float4> ScratchPigments : register(u2, space1);
#ifdef ATLAS_HEIGHT_FLOAT
[[vk::image_format("rgba16f")]]
#else
[[vk::image_format("rgba16")]]
#endif
RWTexture2D<float4> ScratchHeightSlope : register(u3, space1);
StructuredBuffer<float4> Ring : register(t4, space1);
Texture2DArray<float4> ChartMaps : register(t5, space1);
StructuredBuffer<uint2> FeatureCells : register(t6, space1);

struct BakeConstants
{
    uint Level;
    uint Flags;                 // 1: has a parent, 2: periodic along U
    uint2 Page;
    uint2 Scratch;
    uint2 Parent;               // Top-left of the parent's slot in the atlas
    float2 ChartMillimetres;
    uint BandTable;
    uint ChartLayer;
    uint Chart;
    uint CellTableBase;
    uint CellTableMask;
    uint Padding;
};

[[vk::push_constant]]
ConstantBuffer<BakeConstants> Bake : register(b0, space999);

float4 CatmullRomWeights(float t)
{
    return float4(t * (-0.5 + t * (1.0 - 0.5 * t)),
                  1.0 + t * t * (-2.5 + 1.5 * t),
                  t * (0.5 + t * (2.0 - 1.5 * t)),
                  t * t * (-0.5 + 0.5 * t));
}

// The parent sampled at a continuous slot coordinate, where texel k has its centre at k + 0.5.
void SampleParent(float2 position, out float4 pigments, out float4 heightSlope)
{
    float2 base = floor(position - 0.5);
    float2 t = position - 0.5 - base;
    float4 wx = CatmullRomWeights(t.x);
    float4 wy = CatmullRomWeights(t.y);
    pigments = 0.0;
    heightSlope = 0.0;
    for (int j = 0; j < 4; j++)
        for (int i = 0; i < 4; i++)
        {
            int2 texel = int2(Bake.Parent) + int2(base) + int2(i - 1, j - 1);
            float weight = wx[i] * wy[j];
            pigments += weight * AtlasPigments.Load(int3(texel, 0));
            heightSlope += weight * AtlasHeightSlope.Load(int3(texel, 0));
        }
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= SLOT_SIZE || id.y >= SLOT_SIZE)
        return;

    SkinTableHeader header = LoadSkinTableHeader(Ring, Bake.BandTable);
    bool hasParent = (Bake.Flags & 1) != 0;
    bool isPeriodic = (Bake.Flags & 2) != 0;
    float2 within = (float2(id.xy) - GUTTER + 0.5) / 128.0;

    SkinPlace place;
    place.Chart = Bake.Chart;
    place.ChartMillimetres = Bake.ChartMillimetres;
    place.IsPeriodic = isPeriodic;
    place.Level = Bake.Level;
    place.Page = Bake.Page;
    place.Within = within;
    place.Direction = ChartDirection(ChartMaps, Bake.ChartLayer, (float2(Bake.Page) + within) / float(1u << Bake.Level));
    place.Footprint = LevelTexelMillimetres(Bake.ChartMillimetres, Bake.Level);
    place.CellTableBase = Bake.CellTableBase;
    place.CellTableMask = Bake.CellTableMask;

    SkinFields fields;
    fields.Height = 0.0;
    fields.Slope = 0.0;
    fields.Pigments = 0.0;
    if (hasParent)
    {
        float2 parentPosition = (float2(Bake.Page & 1u) * 128.0 + float2(id.xy) - GUTTER + 0.5) * 0.5 + GUTTER;
        float4 pigments, heightSlope;
        SampleParent(parentPosition, pigments, heightSlope);
        fields = DecodeFields(heightSlope, pigments, header);
    }

    for (uint b = 0; b < header.BandCount; b++)
    {
        SkinBand band = LoadSkinBand(Ring, Bake.BandTable, b);
        float weight = BakeWeight(BandScale(band), Bake.ChartMillimetres, Bake.Level);
        if (hasParent)
            weight -= BakeWeight(BandScale(band), Bake.ChartMillimetres, Bake.Level - 1);
        if (weight <= 0.0)
            continue;
        place.BandIndex = b;
        AddBand(fields, band, EvaluateBand(band, place, Ring, FeatureCells), weight);
    }

    ScratchPigments[Bake.Scratch + id.xy] = EncodePigments(fields, header);
    ScratchHeightSlope[Bake.Scratch + id.xy] = EncodeHeightSlope(fields, header);
}
