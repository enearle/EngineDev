// Skin from a chart's baked page plus at most three live bands, coarsest first. A band contributes
// with its pixel weight minus its bake weight at the level hit, so the image does not depend on
// which level is resident.
#define SURFACE_WRITES_FEEDBACK
#include "../Paging.hlsli"
#include "../Skin.hlsli"

cbuffer ProceduralSkinParameters : register(b0, space2)
{
    float Chart;
    float Melanin;
    float Hemoglobin;
    float3 Tint;
    float BaseRoughness;
    float SubsurfaceStrength;
};

// Mirrors SkinConstants in Renderer/Skin/Skin.cpp.
cbuffer SkinConstants : register(b0, space4)
{
    uint4 SkinTableOffsets[4];      // Band table i starts at entry SkinTableOffsets[i / 4][i % 4] of the ring
    uint SkinCellTableBase;
    uint SkinCellTableMask;
    uint2 SkinPadding;
};
StructuredBuffer<float4> SkinRing : register(t1, space4);
Texture2DArray<float4> SkinChartMaps : register(t2, space4);
StructuredBuffer<uint2> SkinFeatureCells : register(t3, space4);

// Absorption per unit of melanin and of hemoglobin in red, green and blue.
static const float3 MelaninAbsorption = float3(0.35, 0.8, 1.6);
static const float3 HemoglobinAbsorption = float3(0.05, 1.2, 0.9);

// The surface's frame from screen derivatives: unit directions of increasing U and V.
void ChartFrame(SurfaceInput input, float3 normal, out float3 alongU, out float3 alongV)
{
    float3 dp1 = ddx(input.WorldPosition);
    float3 dp2 = ddy(input.WorldPosition);
    float2 duv1 = ddx(input.UV);
    float2 duv2 = ddy(input.UV);
    float3 dp2perp = cross(dp2, normal);
    float3 dp1perp = cross(normal, dp1);
    float3 u = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 v = dp2perp * duv1.y + dp1perp * duv2.y;
    alongU = dot(u, u) > 0.0 ? normalize(u) : float3(1.0, 0.0, 0.0);
    alongV = dot(v, v) > 0.0 ? normalize(v) : float3(0.0, 0.0, 1.0);
}

Surface EvaluateSurface(SurfaceInput input)
{
    uint texture = (uint)Chart;
    float2 uvDdx = ddx(input.UV);
    float2 uvDdy = ddy(input.UV);
    PageResolve resolved = ResolvePage(texture, input.UV, uvDdx, uvDdy);
    WritePagingFeedback(input.PixelPosition, texture, resolved.WantedLevel, resolved.WantedPage);

    PagedTextureInfo info = LoadPagedTexture(texture);
    uint table = SkinTableOffsets[info.BandTable / 4][info.BandTable % 4];
    SkinTableHeader header = LoadSkinTableHeader(SkinRing, table);
    SkinFields fields = DecodeFields(SamplePagingAtlas1(resolved), SamplePagingAtlas0(resolved), header);

    float footprint = max(length(uvDdx * info.Millimetres), length(uvDdy * info.Millimetres));
    uint level = resolved.IsFound ? resolved.Level : 0;
    SkinPlace place;
    place.Chart = texture;
    place.ChartMillimetres = info.Millimetres;
    place.IsPeriodic = info.IsPeriodic;
    place.Level = level;
    place.Page = resolved.Page;
    place.Within = resolved.WithinPage;
    place.Direction = ChartDirection(SkinChartMaps, info.ChartMapLayer, input.UV);
    place.Footprint = footprint;
    place.CellTableBase = SkinCellTableBase;
    place.CellTableMask = SkinCellTableMask;
    uint live = 0;
    float unresolvedVariance = header.TotalSlopeVariance;
    for (uint b = 0; b < header.BandCount; b++)
    {
        SkinBand band = LoadSkinBand(SkinRing, table, b);
        float pixelWeight = BandWeight(BandScale(band), footprint);
        if (pixelWeight > 0.0)
            unresolvedVariance = band.FinerSlopeVariance;
        float weight = pixelWeight - (resolved.IsFound ? BakeWeight(BandScale(band), info.Millimetres, level) : 0.0);
        if (weight <= 0.001 || live >= SKIN_LIVE_BANDS)
            continue;
        place.BandIndex = b;
        AddBand(fields, band, EvaluateBand(band, place, SkinRing, SkinFeatureCells), weight);
        live++;
    }

    float3 normal = normalize(input.Normal);
    float3 alongU, alongV;
    ChartFrame(input, normal, alongU, alongV);

    float melanin = max(Melanin + fields.Pigments.x, 0.0);
    float hemoglobin = max(Hemoglobin + fields.Pigments.y, 0.0);

    Surface surface;
    surface.Albedo = saturate(Tint * exp(-(melanin * MelaninAbsorption + hemoglobin * HemoglobinAbsorption)));
    if (PagingIsTinted != 0)
        surface.Albedo = lerp(surface.Albedo, PagingLevelTint(resolved.Level), 0.6);
    surface.Normal = normalize(normal - fields.Slope.x * alongU - fields.Slope.y * alongV);
    surface.Metallic = 0.0;
    surface.Roughness = saturate(sqrt(BaseRoughness * BaseRoughness + unresolvedVariance));
    surface.Occlusion = 1.0;
    surface.Subsurface = SubsurfaceStrength;
    return surface;
}
