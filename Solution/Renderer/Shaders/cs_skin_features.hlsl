// Generates missing feature cells, one thread per cell. A cell is one lattice cell plus
// FeatureCellMargin lattice cells all round; its points are stored in cell-local 16-bit coordinates.
//
// An independent band has at most one jittered point per lattice cell, kept with its density. A
// dependent band (pores at furrow junctions) shares its parent's lattice and places points at the
// parent network's junctions: the circumcentres of the two Delaunay triangles of each lattice quad,
// whose circles hold no other point of the quad. Its parent's points are a function of the parent's
// lattice and seed, so they are recomputed here rather than read from the parent's cells.
#include "Skin.hlsli"

RWStructuredBuffer<uint2> FeatureCells : register(u0, space1);
StructuredBuffer<float4> Ring : register(t1, space1);
Texture2DArray<float4> ChartMaps : register(t2, space1);

struct FeatureConstants
{
    uint JobBase;               // In 16-byte entries of the ring, two per job
    uint JobCount;
    uint2 Padding;
};

[[vk::push_constant]]
ConstantBuffer<FeatureConstants> Features : register(b0, space999);

bool LatticePointAt(int2 lattice, SkinBand band, uint2 cells, bool isPeriodic, out float2 position)
{
    int2 wrapped = int2(WrapCell(lattice.x, cells.x, isPeriodic), lattice.y);
    uint keep = HashCell(wrapped, band.Seed);
    uint jitter = HashCell(wrapped, band.Seed ^ 0x5BD1E995u);
    position = float2(lattice) + 0.1 + 0.8 * float2(jitter & 0xFFFF, jitter >> 16) / 65535.0;
    return float(keep & 0xFFFF) / 65536.0 < band.Density;
}

float2 Circumcentre(float2 a, float2 b, float2 c)
{
    float2 ab = b - a, ac = c - a;
    float denominator = 2.0 * (ab.x * ac.y - ab.y * ac.x);
    if (abs(denominator) < 1e-12)
        return (a + b + c) / 3.0;
    float lengthB = dot(ab, ab), lengthC = dot(ac, ac);
    return a + float2(ac.y * lengthB - ab.y * lengthC, ab.x * lengthC - ac.x * lengthB) / denominator;
}

void Store(uint record, inout uint count, float2 local, uint margin, uint value)
{
    float2 normalised = (local + float(margin)) / float(1 + 2 * margin);
    if (count >= 32 || any(normalised < 0.0) || any(normalised > 1.0))
        return;
    uint2 quantised = uint2(normalised * 65535.0 + 0.5);
    FeatureCells[record + 1 + count] = uint2(quantised.x | (quantised.y << 16), value);
    count++;
}

uint PackPosition(float2 local, uint margin)
{
    uint2 quantised = uint2(saturate((local + float(margin)) / float(1 + 2 * margin)) * 65535.0 + 0.5);
    return quantised.x | (quantised.y << 16);
}

// A step along the direction field, in lattice cells, keeping the heading of the previous step.
float2 CurveStep(float2 position, float2 heading, uint2 cells, float2 cellMillimetres, uint layer)
{
    float2 direction = ChartDirection(ChartMaps, layer, position / float2(cells));
    float2 step = normalize(direction / cellMillimetres) * 0.5;
    return dot(step, heading) < 0.0 ? -step : step;
}

// Curves: every seed within reach is traced both ways in half-cell steps, and each quadratic
// segment (three points, two steps apart) near the cell is stored in two slots. A seed's curve is a
// function of its lattice cell, so neighbouring cells store the same segments.
void GenerateCurves(SkinBand band, int2 cell, uint2 cells, float2 chartMillimetres, bool isPeriodic, uint layer, uint record, inout uint count)
{
    uint margin = FeatureCellMargin(band);
    float2 cellMillimetres = chartMillimetres / float2(cells);
    for (int y = -SKIN_CURVE_REACH; y <= SKIN_CURVE_REACH; y++)
        for (int x = -SKIN_CURVE_REACH; x <= SKIN_CURVE_REACH; x++)
        {
            float2 seed;
            if (!LatticePointAt(cell + int2(x, y), band, cells, isPeriodic, seed))
                continue;

            float2 points[2 * SKIN_CURVE_STEPS + 1];
            points[SKIN_CURVE_STEPS] = seed;
            float2 start = CurveStep(seed, float2(1.0, 0.0), cells, cellMillimetres, layer);
            float2 heading = start;
            for (int i = 1; i <= SKIN_CURVE_STEPS; i++)
            {
                heading = CurveStep(points[SKIN_CURVE_STEPS + i - 1], heading, cells, cellMillimetres, layer);
                points[SKIN_CURVE_STEPS + i] = points[SKIN_CURVE_STEPS + i - 1] + heading;
            }
            heading = -start;
            for (int j = 1; j <= SKIN_CURVE_STEPS; j++)
            {
                heading = CurveStep(points[SKIN_CURVE_STEPS - j + 1], heading, cells, cellMillimetres, layer);
                points[SKIN_CURVE_STEPS - j] = points[SKIN_CURVE_STEPS - j + 1] + heading;
            }

            for (int k = 0; k + 2 <= 2 * SKIN_CURVE_STEPS; k += 2)
            {
                float2 a = points[k] - float2(cell), middle = points[k + 1] - float2(cell), b = points[k + 2] - float2(cell);
                float2 low = min(a, min(middle, b)), high = max(a, max(middle, b));
                bool isNear = all(high > -1.0) && all(low < 2.0);
                bool isStorable = all(low >= -float(margin)) && all(high <= 1.0 + float(margin));
                if (!isNear || !isStorable || count + 2 > 32)
                    continue;
                FeatureCells[record + 1 + count] = uint2(PackPosition(a, margin), PackPosition(middle, margin));
                FeatureCells[record + 2 + count] = uint2(PackPosition(b, margin), 0);
                count += 2;
            }
        }
}

[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= Features.JobCount)
        return;

    uint4 job = asuint(Ring[Features.JobBase + id.x * 2]);
    uint4 detail = asuint(Ring[Features.JobBase + id.x * 2 + 1]);
    int2 cell = int2(job.xy);
    uint bandIndex = job.z & 0xFF;
    uint record = job.w * SKIN_CELL_RECORD;
    uint table = detail.x;
    float2 chartMillimetres = asfloat(detail.yz);
    bool isPeriodic = (detail.w & 1) != 0;
    uint layer = detail.w >> 1;

    SkinBand band = LoadSkinBand(Ring, table, bandIndex);
    uint margin = FeatureCellMargin(band);
    int extent = 1 + 2 * int(margin);
    uint2 cells = BandCells(band.Wavelength, chartMillimetres);
    int2 first = cell - int(margin);
    uint count = 0;

    if (band.Primitive == SKIN_PRIMITIVE_CURVES)
        GenerateCurves(band, cell, cells, chartMillimetres, isPeriodic, layer, record, count);
    else if (band.DependsOn < 0)
    {
        for (int y = 0; y < extent; y++)
            for (int x = 0; x < extent; x++)
            {
                float2 position;
                int2 lattice = first + int2(x, y);
                if (LatticePointAt(lattice, band, cells, isPeriodic, position))
                    Store(record, count, position - float2(cell), margin, HashCell(lattice, band.Seed ^ 0x27D4EB2Fu));
            }
    }
    else
    {
        SkinBand parent = LoadSkinBand(Ring, table, uint(band.DependsOn));
        float2 cellMillimetres = chartMillimetres / float2(cells);
        for (int y = 0; y < extent; y++)
            for (int x = 0; x < extent; x++)
            {
                int2 quad = first + int2(x, y);
                float2 a, b, c, d;
                if (!LatticePointAt(quad, parent, cells, isPeriodic, a) || !LatticePointAt(quad + int2(1, 0), parent, cells, isPeriodic, b) ||
                    !LatticePointAt(quad + int2(0, 1), parent, cells, isPeriodic, c) || !LatticePointAt(quad + int2(1, 1), parent, cells, isPeriodic, d))
                    continue;

                // Delaunay test in millimetres: the diagonal a-d unless c falls inside the circle of a, b, d.
                float2 centre = Circumcentre(a * cellMillimetres, b * cellMillimetres, d * cellMillimetres);
                bool isDiagonalAD = length(c * cellMillimetres - centre) >= length(a * cellMillimetres - centre);
                float2 junctions[2];
                junctions[0] = isDiagonalAD ? Circumcentre(a * cellMillimetres, b * cellMillimetres, d * cellMillimetres)
                                            : Circumcentre(a * cellMillimetres, b * cellMillimetres, c * cellMillimetres);
                junctions[1] = isDiagonalAD ? Circumcentre(a * cellMillimetres, d * cellMillimetres, c * cellMillimetres)
                                            : Circumcentre(b * cellMillimetres, d * cellMillimetres, c * cellMillimetres);
                for (uint j = 0; j < 2; j++)
                {
                    uint keep = HashCell(quad * 2 + int2(j, 0), band.Seed);
                    if (float(keep & 0xFFFF) / 65536.0 >= band.Density)
                        continue;
                    Store(record, count, junctions[j] / cellMillimetres - float2(cell), margin, keep >> 16);
                }
            }
    }
    FeatureCells[record] = uint2(count, 0);
}
