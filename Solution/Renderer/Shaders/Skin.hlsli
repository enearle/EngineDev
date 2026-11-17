// Procedural skin: band tables, the band lattice, noise and weights. The bake (cs_skin_bake) and
// the material (Surfaces/procedural_skin) both evaluate bands through these functions, which is what
// makes the baked and live parts of a band identical. See Renderer/Skin/Skin.h.
#ifndef SKIN_HLSLI
#define SKIN_HLSLI

#define SKIN_FIELD_HEIGHT 0
#define SKIN_FIELD_MELANIN 1
#define SKIN_FIELD_HEMOGLOBIN 2
#define SKIN_FIELD_PIGMENT 3
#define SKIN_PRIMITIVE_NOISE 0
#define SKIN_PRIMITIVE_POINTS 1
#define SKIN_PRIMITIVE_CURVES 2
#define SKIN_LIVE_BANDS 3
#define SKIN_TABLE_HEADER 2
#define SKIN_BAND_ENTRIES 3

// Mirrors the band table Renderer/Skin/Skin.cpp writes into the frame ring: two header entries, then
// three per band, coarse to fine.
struct SkinTableHeader
{
    uint BandCount;
    float HeightRange;      // Height in millimetres is stored as 0.5 + 0.5 * height / HeightRange
    float SlopeRange;
    float PigmentRange;
    float TotalSlopeVariance;
};

struct SkinBand
{
    float Wavelength;       // Millimetres on the bind pose
    float Amplitude;
    uint Field;
    uint Primitive;
    uint Seed;
    float Density;
    float Width;
    float Stretch;
    uint Profile;
    int DependsOn;
    float SlopeVariance;
    float FinerSlopeVariance;   // Of every finer band
};

SkinTableHeader LoadSkinTableHeader(StructuredBuffer<float4> ring, uint table)
{
    float4 first = ring[table];
    float4 second = ring[table + 1];
    SkinTableHeader header;
    header.BandCount = (uint)first.x;
    header.HeightRange = first.y;
    header.SlopeRange = first.z;
    header.PigmentRange = first.w;
    header.TotalSlopeVariance = second.x;
    return header;
}

SkinBand LoadSkinBand(StructuredBuffer<float4> ring, uint table, uint band)
{
    uint entry = table + SKIN_TABLE_HEADER + band * SKIN_BAND_ENTRIES;
    float4 a = ring[entry];
    float4 b = ring[entry + 1];
    float4 c = ring[entry + 2];
    SkinBand result;
    result.Wavelength = a.x;
    result.Amplitude = a.y;
    result.Field = (uint)a.z;
    result.Primitive = (uint)a.w;
    result.Seed = asuint(b.x);
    result.Density = b.y;
    result.Width = b.z;
    result.Stretch = b.w;
    result.Profile = (uint)c.x;
    result.DependsOn = (int)c.y;
    result.SlopeVariance = c.z;
    result.FinerSlopeVariance = c.w;
    return result;
}

// A band is baked into a level when its wavelength spans 4 or more of the level's texels, not at all
// at 2 or fewer, and smoothly between. The pixel weight is the same curve on the pixel footprint.
float BandWeight(float wavelength, float footprintMillimetres)
{
    return smoothstep(2.0, 4.0, wavelength / max(footprintMillimetres, 1e-12));
}

float LevelTexelMillimetres(float2 chartMillimetres, uint level)
{
    return max(chartMillimetres.x, chartMillimetres.y) / (128.0 * float(1u << level));
}

float BakeWeight(float wavelength, float2 chartMillimetres, uint level)
{
    return BandWeight(wavelength, LevelTexelMillimetres(chartMillimetres, level));
}

// Cells of a band across the chart along each axis: a whole number, so the lattice repeats exactly
// at the chart's edge.
uint2 BandCells(float wavelength, float2 chartMillimetres)
{
    return max(uint2(round(chartMillimetres / wavelength)), 1u);
}

// The cell and the fraction within it along one axis, for a point at `within` (0 to 1 across the
// page, beyond that in the gutter) of page `page` at `level`. page * cells can exceed 32 bits, so it
// is formed in two words from 16-bit halves and shifted down by the level; no 64-bit integers.
void LatticeAxis(uint page, uint level, float within, uint cells, out int cell, out float fraction)
{
    uint pageLow = page & 0xFFFF, pageHigh = page >> 16;
    uint cellsLow = cells & 0xFFFF, cellsHigh = cells >> 16;
    uint low = pageLow * cellsLow;
    uint middle = pageHigh * cellsLow + pageLow * cellsHigh;
    uint high = pageHigh * cellsHigh;
    uint lowSum = low + (middle << 16);
    high += (middle >> 16) + (lowSum < low ? 1u : 0u);
    low = lowSum;

    uint whole = level == 0 ? low : ((high << (32 - level)) | (low >> level));
    uint remainder = level == 0 ? 0u : (low & ((1u << level) - 1u));
    float position = (float(remainder) + within * float(cells)) / float(1u << level);
    float steps = floor(position);
    cell = int(whole) + int(steps);
    fraction = position - steps;
}

int WrapCell(int cell, uint cells, bool isPeriodic)
{
    if (!isPeriodic)
        return cell;
    int wrapped = cell % int(cells);
    return wrapped < 0 ? wrapped + int(cells) : wrapped;
}

uint HashCell(int2 cell, uint seed)
{
    uint hash = uint(cell.x) * 0x8DA6B343u ^ uint(cell.y) * 0xD8163841u ^ seed * 0xCB1AB31Fu;
    hash ^= hash >> 16;
    hash *= 0x7FEB352Du;
    hash ^= hash >> 15;
    hash *= 0x846CA68Bu;
    hash ^= hash >> 16;
    return hash;
}

float2 CellGradient(int2 cell, uint seed)
{
    float angle = float(HashCell(cell, seed) & 0xFFFF) * (6.28318530718 / 65536.0);
    return float2(cos(angle), sin(angle));
}

// Where a band's lattice is at a point: cell, fraction, cells across the chart, periodic flag.
struct LatticePoint
{
    int2 Cell;
    float2 Fraction;
    uint2 Cells;
    bool IsPeriodic;
};

LatticePoint LocateInBand(float wavelength, float2 chartMillimetres, bool isPeriodic, uint level, uint2 page, float2 within)
{
    LatticePoint located;
    located.Cells = BandCells(wavelength, chartMillimetres);
    located.IsPeriodic = isPeriodic;
    LatticeAxis(page.x, level, within.x, located.Cells.x, located.Cell.x, located.Fraction.x);
    LatticeAxis(page.y, level, within.y, located.Cells.y, located.Cell.y, located.Fraction.y);
    return located;
}

// Gradient noise with its analytic derivative, per cell: xy derivative, z value.
float3 GradientNoise(LatticePoint located, uint seed)
{
    int x0 = WrapCell(located.Cell.x, located.Cells.x, located.IsPeriodic);
    int x1 = WrapCell(located.Cell.x + 1, located.Cells.x, located.IsPeriodic);
    int y0 = located.Cell.y;
    int y1 = located.Cell.y + 1;
    float2 f = located.Fraction;

    float2 g00 = CellGradient(int2(x0, y0), seed);
    float2 g10 = CellGradient(int2(x1, y0), seed);
    float2 g01 = CellGradient(int2(x0, y1), seed);
    float2 g11 = CellGradient(int2(x1, y1), seed);
    float n00 = dot(g00, f);
    float n10 = dot(g10, f - float2(1.0, 0.0));
    float n01 = dot(g01, f - float2(0.0, 1.0));
    float n11 = dot(g11, f - float2(1.0, 1.0));

    float2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    float2 du = 30.0 * f * f * (f * (f - 2.0) + 1.0);
    float k = n00 - n10 - n01 + n11;
    float value = n00 + u.x * (n10 - n00) + u.y * (n01 - n00) + u.x * u.y * k;
    float2 gradient = g00 + u.x * (g10 - g00) + u.y * (g01 - g00) + u.x * u.y * (g00 - g10 - g01 + g11)
                    + du * float2(n10 - n00 + u.y * k, n01 - n00 + u.x * k);
    return float3(gradient, value);
}

// One band's contribution at a point: x height or pigment, yz slope in millimetres per millimetre
// along U and V. Points and curves are added in later steps; until then they contribute nothing.
float3 EvaluateBand(SkinBand band, float2 chartMillimetres, bool isPeriodic, uint level, uint2 page, float2 within)
{
    if (band.Primitive != SKIN_PRIMITIVE_NOISE)
        return 0.0;
    LatticePoint located = LocateInBand(band.Wavelength, chartMillimetres, isPeriodic, level, page, within);
    float3 noise = GradientNoise(located, band.Seed);
    float2 cellsPerMillimetre = float2(located.Cells) / chartMillimetres;
    return band.Amplitude * float3(noise.z, noise.xy * cellsPerMillimetre);
}

// Fields of a texel: height and slope (atlas image 1) and the three pigments (atlas image 0).
struct SkinFields
{
    float Height;
    float2 Slope;
    float3 Pigments;
};

void AddBand(inout SkinFields fields, SkinBand band, float3 contribution, float weight)
{
    if (band.Field == SKIN_FIELD_HEIGHT)
    {
        fields.Height += weight * contribution.x;
        fields.Slope += weight * contribution.yz;
    }
    else
        fields.Pigments[band.Field - 1] += weight * contribution.x;
}

float4 EncodeHeightSlope(SkinFields fields, SkinTableHeader header)
{
    return saturate(float4(0.5 + 0.5 * fields.Height / header.HeightRange, 0.5 + 0.5 * fields.Slope / header.SlopeRange, 0.5));
}

float4 EncodePigments(SkinFields fields, SkinTableHeader header)
{
    return saturate(float4(0.5 + 0.5 * fields.Pigments / header.PigmentRange, 0.5));
}

SkinFields DecodeFields(float4 heightSlope, float4 pigments, SkinTableHeader header)
{
    SkinFields fields;
    fields.Height = (heightSlope.x * 2.0 - 1.0) * header.HeightRange;
    fields.Slope = (heightSlope.yz * 2.0 - 1.0) * header.SlopeRange;
    fields.Pigments = (pigments.xyz * 2.0 - 1.0) * header.PigmentRange;
    return fields;
}

#endif
