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
#define SKIN_PROFILE_DIP 0
#define SKIN_PROFILE_NETWORK 1
#define SKIN_PROFILE_PLATE 2
#define SKIN_PROFILE_LINE 3
#define SKIN_CELL_RECORD 33         // A count, then up to 32 primitives of 8 bytes
#define SKIN_CELL_PROBE_STEPS 4
#define SKIN_CHART_MAP_SIZE 512
#define SKIN_CURVE_STEPS 6              // Half-cell steps each way from a curve's seed
#define SKIN_CURVE_REACH 4              // Lattice cells a curve can reach from its seed

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

// The scale the weights compare with texels and pixels: a noise band's wavelength, and the profile
// width of points and curves, which are far narrower than their spacing. A profile is about four
// widths across, so it is baked only once it spans 8 to 16 texels, which the parent upsampling
// reproduces closely; baked any narrower, children would inherit its upsampling error.
float BandScale(SkinBand band)
{
    return band.Primitive == SKIN_PRIMITIVE_NOISE ? band.Wavelength : band.Width;
}

// A feature cell is one lattice cell plus a margin of lattice cells all round, so a texel reads one
// cell: two for an independent band, whose network profiles need the second-nearest point (25
// points), one for a dependent band, which has two points per lattice cell and needs only the
// nearest (18 points).
uint FeatureCellMargin(SkinBand band)
{
    return band.DependsOn >= 0 && band.Primitive == SKIN_PRIMITIVE_POINTS ? 1u : 2u;
}

// A band is baked into a level when its scale spans 4 or more of the level's texels, not at all at
// 2 or fewer, and smoothly between. The pixel weight is the same curve on the pixel footprint.
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

// Mirrors FeatureCellHash in Renderer/Skin/FeatureCache.cpp.
uint FeatureCellHash(uint chart, uint band, int x, int y)
{
    uint hash = chart * 0x9E3779B1u ^ band * 0x85EBCA77u;
    hash ^= uint(x) * 0xC2B2AE3Du;
    hash = (hash ^ (hash >> 15)) * 0x27D4EB2Fu;
    hash ^= uint(y) * 0x165667B1u;
    hash = (hash ^ (hash >> 13)) * 0x9E3779B1u;
    return hash ^ (hash >> 16);
}

// What a band evaluation needs besides the band: the chart, where the point is, and the caches.
struct SkinPlace
{
    uint Chart;
    uint BandIndex;
    float2 ChartMillimetres;
    bool IsPeriodic;
    uint Level;
    uint2 Page;
    float2 Within;
    float2 Direction;           // Unit direction field in the chart's millimetre frame
    float Footprint;            // Millimetres a profile is widened to at least
    uint CellTableBase;         // In 16-byte entries of the ring
    uint CellTableMask;
};

bool FindFeatureCell(StructuredBuffer<float4> ring, SkinPlace place, int2 cell, out uint slot)
{
    uint key = (place.Chart << 8) | place.BandIndex;
    uint index = FeatureCellHash(place.Chart, place.BandIndex, cell.x, cell.y);
    bool isFound = false;
    slot = 0;
    [unroll]
    for (uint step = 0; step < SKIN_CELL_PROBE_STEPS; step++)
    {
        uint4 entry = asuint(ring[place.CellTableBase + ((index + step) & place.CellTableMask)]);
        if (!isFound && entry.z == key && int(entry.x) == cell.x && int(entry.y) == cell.y)
        {
            slot = entry.w;
            isFound = true;
        }
    }
    return isFound;
}

int FloorDivide(int value, int divisor)
{
    return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
}

float2 DecodeFeaturePosition(uint packed, uint margin)
{
    return float2(packed & 0xFFFF, packed >> 16) / 65535.0 * float(1 + 2 * margin) - float(margin);
}

// The direction field at a chart position, read without filtering so the bake and the material see
// the same value.
float2 ChartDirection(Texture2DArray<float4> maps, uint layer, float2 uv)
{
    int2 texel = clamp(int2(uv * SKIN_CHART_MAP_SIZE), 0, SKIN_CHART_MAP_SIZE - 1);
    float2 doubled = maps.Load(int4(texel, layer, 0)).xy * 2.0 - 1.0;
    if (dot(doubled, doubled) < 0.01)
        return float2(1.0, 0.0);
    float angle = 0.5 * atan2(doubled.y, doubled.x);
    return float2(cos(angle), sin(angle));
}

// Points: the nearest and second-nearest points of the band's cell, distances measured with the
// direction field stretched by the band's stretch, turned into a profile. Dip: a hollow at each
// point (pores, freckles). Network: grooves where the two nearest are equally far (furrows, lines).
// Plate: ridges there instead. A missing cell contributes nothing.
float3 EvaluatePointBand(SkinBand band, SkinPlace place, StructuredBuffer<float4> ring, StructuredBuffer<uint2> cells)
{
    LatticePoint located = LocateInBand(band.Wavelength, place.ChartMillimetres, place.IsPeriodic, place.Level, place.Page, place.Within);
    uint margin = FeatureCellMargin(band);
    int2 cell = located.Cell;
    uint slot;
    if (!FindFeatureCell(ring, place, cell, slot))
        return 0.0;

    float2 local = located.Fraction;
    float2 cellMillimetres = place.ChartMillimetres / float2(located.Cells);
    float2 across = float2(-place.Direction.y, place.Direction.x);
    uint record = slot * SKIN_CELL_RECORD;
    uint count = min(cells[record].x, 32u);
    float nearest = 1e9, second = 1e9;
    float2 nearestGradient = 0.0, secondGradient = 0.0;
    for (uint i = 0; i < count; i++)
    {
        float2 delta = (local - DecodeFeaturePosition(cells[record + 1 + i].x, margin)) * cellMillimetres;
        float2 metric = float2(dot(delta, place.Direction) / band.Stretch, dot(delta, across));
        float distance = length(metric);
        float2 gradient = distance > 0.0 ? (metric.x / band.Stretch * place.Direction + metric.y * across) / distance : 0.0;
        if (distance < nearest)
        {
            second = nearest;
            secondGradient = nearestGradient;
            nearest = distance;
            nearestGradient = gradient;
        }
        else if (distance < second)
        {
            second = distance;
            secondGradient = gradient;
        }
    }
    if (count == 0 || (band.Profile != SKIN_PROFILE_DIP && count < 2))
        return 0.0;

    float width = max(band.Width, place.Footprint);
    float distance = band.Profile == SKIN_PROFILE_DIP ? nearest : second - nearest;
    float2 gradient = band.Profile == SKIN_PROFILE_DIP ? nearestGradient : secondGradient - nearestGradient;
    float bump = exp(-(distance * distance) / (width * width));
    float sign = band.Profile == SKIN_PROFILE_PLATE ? 1.0 : -1.0;
    float slope = sign * bump * (-2.0 * distance / (width * width));
    return band.Amplitude * float3(sign * bump, slope * gradient);
}

// The distance from p to the segment a-b, and its gradient with respect to p.
float2 SegmentDistance(float2 p, float2 a, float2 b, out float2 gradient)
{
    float2 ab = b - a;
    float t = saturate(dot(p - a, ab) / max(dot(ab, ab), 1e-12));
    float2 offset = p - (a + t * ab);
    float distance = length(offset);
    gradient = distance > 0.0 ? offset / distance : 0.0;
    return distance;
}

// Curves: each cell holds quadratic segments in two primitive slots (start and middle, then end), and
// a segment's distance is measured as its two straight halves. A groove follows the nearest curve.
float3 EvaluateCurveBand(SkinBand band, SkinPlace place, StructuredBuffer<float4> ring, StructuredBuffer<uint2> cells)
{
    LatticePoint located = LocateInBand(band.Wavelength, place.ChartMillimetres, place.IsPeriodic, place.Level, place.Page, place.Within);
    uint slot;
    if (!FindFeatureCell(ring, place, located.Cell, slot))
        return 0.0;

    uint margin = FeatureCellMargin(band);
    float2 cellMillimetres = place.ChartMillimetres / float2(located.Cells);
    float2 position = located.Fraction * cellMillimetres;
    uint record = slot * SKIN_CELL_RECORD;
    uint count = min(cells[record].x, 32u);
    float nearest = 1e9;
    float2 nearestGradient = 0.0;
    for (uint i = 0; i + 1 < count; i += 2)
    {
        uint2 first = cells[record + 1 + i];
        uint2 second = cells[record + 2 + i];
        float2 a = DecodeFeaturePosition(first.x, margin) * cellMillimetres;
        float2 middle = DecodeFeaturePosition(first.y, margin) * cellMillimetres;
        float2 b = DecodeFeaturePosition(second.x, margin) * cellMillimetres;
        float2 gradient;
        float distance = SegmentDistance(position, a, middle, gradient);
        if (distance < nearest)
        {
            nearest = distance;
            nearestGradient = gradient;
        }
        distance = SegmentDistance(position, middle, b, gradient);
        if (distance < nearest)
        {
            nearest = distance;
            nearestGradient = gradient;
        }
    }
    if (nearest >= 1e9)
        return 0.0;

    float width = max(band.Width, place.Footprint);
    float bump = exp(-(nearest * nearest) / (width * width));
    float slope = bump * (2.0 * nearest / (width * width));
    return band.Amplitude * float3(-bump, slope * nearestGradient);
}

// One band's contribution at a point: x height or pigment, yz slope in millimetres per millimetre
// along U and V.
float3 EvaluateBand(SkinBand band, SkinPlace place, StructuredBuffer<float4> ring, StructuredBuffer<uint2> cells)
{
    if (band.Primitive == SKIN_PRIMITIVE_POINTS)
        return EvaluatePointBand(band, place, ring, cells);
    if (band.Primitive == SKIN_PRIMITIVE_CURVES)
        return EvaluateCurveBand(band, place, ring, cells);
    LatticePoint located = LocateInBand(band.Wavelength, place.ChartMillimetres, place.IsPeriodic, place.Level, place.Page, place.Within);
    float3 noise = GradientNoise(located, band.Seed);
    float2 cellsPerMillimetre = float2(located.Cells) / place.ChartMillimetres;
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
