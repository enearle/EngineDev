// Skin from a chart's baked page plus at most three live bands, coarsest first. A band contributes
// with its pixel weight minus its bake weight at the level hit, so the image does not depend on
// which level is resident.
//
// Near a seam the vertices carry a seam patch: a second chart in UV1 that runs across the seam, and
// a weight, 1 on the seam and 0 at the strip's edge. Both charts are evaluated and blended by
// lerp(primary, patch, w) / sqrt((1 - w)^2 + w^2), which keeps the variance of the zero-mean fields
// constant across the blend; slopes are blended as normal offsets in the world, each from its own
// chart's frame. Feedback goes to one chart or the other by block and frame.
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
    float PatchTable;               // Negative for none
};

// Mirrors SkinConstants in Renderer/Skin/Skin.cpp.
cbuffer SkinConstants : register(b0, space4)
{
    uint4 SkinTableOffsets[4];      // Band table i starts at entry SkinTableOffsets[i / 4][i % 4] of the ring
    uint SkinCellTableBase;
    uint SkinCellTableMask;
    uint SkinIsPatched;             // 0 draws from the primary chart alone (Skin::SetPatchesEnabled)
    uint SkinView;                  // SkinView in Renderer/Skin/Skin.h
    uint4 SkinPatchTableOffsets[4]; // Likewise for patch tables, one entry per patch chart
};
StructuredBuffer<float4> SkinRing : register(t1, space4);
Texture2DArray<float4> SkinChartMaps : register(t2, space4);
StructuredBuffer<uint2> SkinFeatureCells : register(t3, space4);

// Absorption per unit of melanin and of hemoglobin in red, green and blue.
static const float3 MelaninAbsorption = float3(0.35, 0.8, 1.6);
static const float3 HemoglobinAbsorption = float3(0.05, 1.2, 0.9);

// Screen derivatives of a chart's UV and of the position, taken where every pixel of the quad runs.
struct ChartDerivatives
{
    float2 UV;
    float2 UVDdx;
    float2 UVDdy;
    float3 PositionDdx;
    float3 PositionDdy;
};

ChartDerivatives DeriveChart(float2 uv, float3 worldPosition)
{
    ChartDerivatives derived;
    derived.UV = uv;
    derived.UVDdx = ddx(uv);
    derived.UVDdy = ddy(uv);
    derived.PositionDdx = ddx(worldPosition);
    derived.PositionDdy = ddy(worldPosition);
    return derived;
}

// The surface's frame from screen derivatives: unit directions of increasing U and V.
void ChartFrame(ChartDerivatives derived, float3 normal, out float3 alongU, out float3 alongV)
{
    float3 dp2perp = cross(derived.PositionDdy, normal);
    float3 dp1perp = cross(normal, derived.PositionDdx);
    float3 u = dp2perp * derived.UVDdx.x + dp1perp * derived.UVDdy.x;
    float3 v = dp2perp * derived.UVDdx.y + dp1perp * derived.UVDdy.y;
    alongU = dot(u, u) > 0.0 ? normalize(u) : float3(1.0, 0.0, 0.0);
    alongV = dot(v, v) > 0.0 ? normalize(v) : float3(0.0, 0.0, 1.0);
}

struct ChartResult
{
    float3 Pigments;
    float3 NormalOffset;            // World space
    float UnresolvedVariance;
    uint Level;
    uint WantedLevel;
    uint2 WantedPage;
    uint LiveBands;
};

ChartResult EvaluateChart(uint texture, ChartDerivatives derived, float3 normal)
{
    PagedTextureInfo info = LoadPagedTexture(texture);
    float2 uv = info.IsPeriodic ? float2(frac(derived.UV.x), derived.UV.y) : derived.UV;
    PageResolve resolved = ResolvePage(texture, uv, derived.UVDdx, derived.UVDdy);

    uint table = SkinTableOffsets[info.BandTable / 4][info.BandTable % 4];
    SkinTableHeader header = LoadSkinTableHeader(SkinRing, table);
    SkinFields fields = DecodeFields(SamplePagingAtlas1(resolved), SamplePagingAtlas0(resolved), header);

    float footprint = max(length(derived.UVDdx * info.Millimetres), length(derived.UVDdy * info.Millimetres));
    uint level = resolved.IsFound ? resolved.Level : 0;
    SkinPlace place;
    place.Chart = texture;
    place.ChartMillimetres = info.Millimetres;
    place.IsPeriodic = info.IsPeriodic;
    place.Level = level;
    place.Page = resolved.Page;
    place.Within = resolved.WithinPage;
    place.Direction = ChartDirection(SkinChartMaps, info.ChartMapLayer, uv);
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

    float3 alongU, alongV;
    ChartFrame(derived, normal, alongU, alongV);

    ChartResult result;
    result.Pigments = fields.Pigments;
    result.NormalOffset = -fields.Slope.x * alongU - fields.Slope.y * alongV;
    result.UnresolvedVariance = unresolvedVariance;
    result.Level = resolved.Level;
    result.WantedLevel = resolved.WantedLevel;
    result.WantedPage = resolved.WantedPage;
    result.LiveBands = live;
    return result;
}

Surface EvaluateSurface(SurfaceInput input)
{
    float3 normal = normalize(input.Normal);
    ChartDerivatives primaryDerived = DeriveChart(input.UV, input.WorldPosition);
    ChartDerivatives patchDerived = DeriveChart(input.UV1, input.WorldPosition);

    uint texture = (uint)Chart;
    ChartResult result = EvaluateChart(texture, primaryDerived, normal);
    uint feedbackTexture = texture;
    uint feedbackLevel = result.WantedLevel;
    uint2 feedbackPage = result.WantedPage;

    float w = saturate(input.PatchWeight);
    if (SkinIsPatched != 0 && PatchTable >= 0.0 && w > 0.001)
    {
        uint patchTable = (uint)PatchTable;
        uint entry = SkinPatchTableOffsets[patchTable / 4][patchTable % 4] + (uint)round(input.PatchChart);
        uint patchTexture = (uint)SkinRing[entry].x;
        ChartResult patch = EvaluateChart(patchTexture, patchDerived, normal);
        float normalisation = rsqrt((1.0 - w) * (1.0 - w) + w * w);
        result.Pigments = lerp(result.Pigments, patch.Pigments, w) * normalisation;
        result.NormalOffset = lerp(result.NormalOffset, patch.NormalOffset, w) * normalisation;
        result.UnresolvedVariance = lerp(result.UnresolvedVariance, patch.UnresolvedVariance, w);

        uint2 block = uint2(input.PixelPosition.xy) >> 3;
        if (((block.x ^ block.y ^ PagingJitter.x) & 1) != 0)
        {
            feedbackTexture = patchTexture;
            feedbackLevel = patch.WantedLevel;
            feedbackPage = patch.WantedPage;
        }
    }
    WritePagingFeedback(input.PixelPosition, feedbackTexture, feedbackLevel, feedbackPage);

    float melanin = max(Melanin + result.Pigments.x, 0.0);
    float hemoglobin = max(Hemoglobin + result.Pigments.y, 0.0);

    Surface surface;
    surface.Albedo = saturate(Tint * exp(-(melanin * MelaninAbsorption + hemoglobin * HemoglobinAbsorption)));
    if (PagingIsTinted != 0)
        surface.Albedo = lerp(surface.Albedo, PagingLevelTint(result.Level), 0.6);
    if (SkinView == 1)
        surface.Albedo = PagingLevelTint(result.Level);
    else if (SkinView == 2)
    {
        static const float3 BandColours[4] = { float3(0.0, 0.0, 0.0), float3(1.0, 0.0, 0.0), float3(1.0, 1.0, 0.0), float3(1.0, 1.0, 1.0) };
        surface.Albedo = BandColours[min(result.LiveBands, 3u)];
    }
    else if (SkinView == 3)
        surface.Albedo = SkinIsPatched != 0 && PatchTable >= 0.0 ? w : 0.0;
    else if (SkinView == 4)
        surface.Albedo = result.Level == PAGING_NO_LEVEL ? float3(1.0, 0.0, 0.0) : (result.Level >= result.WantedLevel ? float3(0.0, 1.0, 0.0) : float3(1.0, 1.0, 0.0));
    surface.Normal = normalize(normal + result.NormalOffset);
    surface.Metallic = 0.0;
    surface.Roughness = saturate(sqrt(BaseRoughness * BaseRoughness + result.UnresolvedVariance));
    surface.Occlusion = 1.0;
    surface.Subsurface = SubsurfaceStrength;
    return surface;
}
