#include "SeamPatches.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>
#include <DirectXMath.h>

using namespace DirectX;
using RHIStructures::SkinnedVertex;

struct EdgeSide
{
    uint64_t Key = 0;                   // Welded endpoints, smaller first
    uint32_t Mesh = 0;
    XMFLOAT2 TexCoordLow = {};          // At the smaller welded endpoint
    XMFLOAT2 TexCoordHigh = {};
};

struct Curve
{
    std::vector<XMFLOAT3> Points;       // Model space
    std::vector<float> Along;           // Cumulative length at each point, model units
    bool IsClosed = false;
};

struct CurvePlace
{
    float Distance = 1e30f;             // Model units
    float Along = 0.0f;
    float Signed = 0.0f;
};

struct SegmentData
{
    float Start[3] = {};
    float Step[3] = {};                 // End minus start
    float LengthSquared = 0.0f;
    float Along = 0.0f;                 // Curve length at the start
    uint32_t Curve = 0;
};

static int64_t GridKey(int64_t x, int64_t y, int64_t z)
{
    return ((x & 0x1FFFFF) << 42) | ((y & 0x1FFFFF) << 21) | (z & 0x1FFFFF);
}

static bool IsSameTexCoord(const XMFLOAT2& a, const XMFLOAT2& b)
{
    return std::abs(a.x - b.x) < 1e-5f && std::abs(a.y - b.y) < 1e-5f;
}

// Seam edges chained into curves between vertices where other than two seam edges meet; what is
// left after that is closed loops.
static std::vector<Curve> ChainCurves(const std::vector<std::pair<uint32_t, uint32_t>>& seams, const std::vector<XMFLOAT3>& weldPositions)
{
    std::unordered_map<uint32_t, std::vector<uint32_t>> neighbours;
    for (const auto& [a, b] : seams)
    {
        neighbours[a].push_back(b);
        neighbours[b].push_back(a);
    }
    std::map<std::pair<uint32_t, uint32_t>, bool> isUsed;
    auto edgeKey = [](uint32_t a, uint32_t b) { return a < b ? std::make_pair(a, b) : std::make_pair(b, a); };

    std::vector<Curve> curves;
    auto walk = [&](uint32_t start, uint32_t next)
    {
        Curve curve;
        std::vector<uint32_t> welds = { start };
        uint32_t previous = start, current = next;
        isUsed[edgeKey(previous, current)] = true;
        while (true)
        {
            welds.push_back(current);
            if (current == start)
            {
                curve.IsClosed = true;
                break;
            }
            const std::vector<uint32_t>& around = neighbours[current];
            if (around.size() != 2)
                break;
            uint32_t following = around[0] == previous ? around[1] : around[0];
            if (isUsed[edgeKey(current, following)])
                break;
            isUsed[edgeKey(current, following)] = true;
            previous = current;
            current = following;
        }
        if (curve.IsClosed)
            welds.pop_back();
        float along = 0.0f;
        for (size_t i = 0; i < welds.size(); i++)
        {
            if (i > 0)
                along += XMVectorGetX(XMVector3Length(XMLoadFloat3(&weldPositions[welds[i]]) - XMLoadFloat3(&weldPositions[welds[i - 1]])));
            curve.Points.push_back(weldPositions[welds[i]]);
            curve.Along.push_back(along);
        }
        if (curve.IsClosed)
        {
            along += XMVectorGetX(XMVector3Length(XMLoadFloat3(&curve.Points.front()) - XMLoadFloat3(&curve.Points.back())));
            curve.Points.push_back(curve.Points.front());
            curve.Along.push_back(along);
        }
        curves.push_back(std::move(curve));
    };

    for (const auto& [weld, around] : neighbours)
        if (around.size() != 2)
            for (uint32_t next : around)
                if (!isUsed[edgeKey(weld, next)])
                    walk(weld, next);
    for (const auto& [weld, around] : neighbours)
        for (uint32_t next : around)
            if (!isUsed[edgeKey(weld, next)])
                walk(weld, next);
    return curves;
}

static float FadedWeight(const CurvePlace& place, const Curve& curve, float radius)
{
    float weight = 1.0f - std::clamp(std::abs(place.Signed) / radius, 0.0f, 1.0f);
    weight = weight * weight * (3.0f - 2.0f * weight);
    if (!curve.IsClosed)
    {
        float length = curve.Along.back();
        float fromEnd = std::clamp((std::min)(place.Along, length - place.Along) / (0.5f * radius), 0.0f, 1.0f);
        weight *= fromEnd * fromEnd * (3.0f - 2.0f * fromEnd);
    }
    return weight;
}

std::vector<PatchChart> SeamPatches::Generate(const std::vector<Mesh*>& skinMeshes, float millimetresPerUnit, bool isAuthored)
{
    // Open edges of each mesh's own index topology, those met by one triangle: seams and openings are
    // among them, since a UV seam splits vertices. Only their endpoints are welded, to a hundredth of
    // a millimetre, and a welded edge that two open edges share is a seam if its sides differ in mesh
    // or UV; an open edge alone is an opening.
    float weldStep = 0.01f / millimetresPerUnit;
    std::unordered_map<int64_t, uint32_t> weldByKey;
    std::vector<XMFLOAT3> weldPositions;
    auto weld = [&](const XMFLOAT3& p)
    {
        int64_t key = GridKey(static_cast<int64_t>(std::floor(p.x / weldStep)), static_cast<int64_t>(std::floor(p.y / weldStep)),
                              static_cast<int64_t>(std::floor(p.z / weldStep)));
        auto [found, isNew] = weldByKey.try_emplace(key, static_cast<uint32_t>(weldPositions.size()));
        if (isNew)
            weldPositions.push_back(p);
        return found->second;
    };

    std::vector<EdgeSide> sides;
    for (size_t m = 0; m < skinMeshes.size(); m++)
    {
        const SkinnedVertexCache* cache = skinMeshes[m]->GetSkinnedCache();
        std::vector<uint64_t> edges;
        edges.reserve(cache->Indices.size());
        for (size_t t = 0; t + 2 < cache->Indices.size(); t += 3)
            for (int e = 0; e < 3; e++)
            {
                uint64_t a = cache->Indices[t + e], b = cache->Indices[t + (e + 1) % 3];
                edges.push_back(a < b ? (a << 32) | b : (b << 32) | a);
            }
        std::sort(edges.begin(), edges.end());
        for (size_t i = 0; i < edges.size();)
        {
            size_t next = i + 1;
            while (next < edges.size() && edges[next] == edges[i])
                next++;
            if (next - i == 1)
            {
                uint32_t a = static_cast<uint32_t>(edges[i] >> 32), b = static_cast<uint32_t>(edges[i] & 0xFFFFFFFF);
                uint32_t weldA = weld(cache->Vertices[a].Position), weldB = weld(cache->Vertices[b].Position);
                if (weldA > weldB)
                {
                    std::swap(weldA, weldB);
                    std::swap(a, b);
                }
                if (weldA != weldB)
                    sides.push_back({ (static_cast<uint64_t>(weldA) << 32) | weldB, static_cast<uint32_t>(m), cache->Vertices[a].TexCoord, cache->Vertices[b].TexCoord });
            }
            i = next;
        }
    }
    std::sort(sides.begin(), sides.end(), [](const EdgeSide& a, const EdgeSide& b) { return a.Key < b.Key; });
    std::vector<std::pair<uint32_t, uint32_t>> seams;
    for (size_t i = 0; i + 1 < sides.size(); i++)
    {
        if (sides[i].Key != sides[i + 1].Key)
            continue;
        const EdgeSide& one = sides[i];
        const EdgeSide& other = sides[i + 1];
        if (one.Mesh != other.Mesh || !IsSameTexCoord(one.TexCoordLow, other.TexCoordLow) || !IsSameTexCoord(one.TexCoordHigh, other.TexCoordHigh))
            seams.push_back({ static_cast<uint32_t>(one.Key >> 32), static_cast<uint32_t>(one.Key & 0xFFFFFFFF) });
        while (i + 1 < sides.size() && sides[i + 1].Key == one.Key)
            i++;
    }

    std::vector<Curve> curves;
    for (Curve& curve : ChainCurves(seams, weldPositions))
        if (curve.Along.back() * millimetresPerUnit >= MinimumCurveMillimetres)
            curves.push_back(std::move(curve));
    if (curves.empty())
        return {};

    // Curve segments in a flat grid of strip-width cells over the curves' bounds, for nearest-curve
    // queries, listed per cell in one array; a cell also records whether any cell around it holds a
    // segment.
    float radius = StripMillimetres / millimetresPerUnit;
    XMVECTOR low = XMLoadFloat3(&curves[0].Points[0]), high = low;
    for (const Curve& curve : curves)
        for (const XMFLOAT3& point : curve.Points)
        {
            low = XMVectorMin(low, XMLoadFloat3(&point));
            high = XMVectorMax(high, XMLoadFloat3(&point));
        }
    low -= XMVectorReplicate(2.0f * radius);
    high += XMVectorReplicate(2.0f * radius);
    XMFLOAT3 origin, extent;
    XMStoreFloat3(&origin, low);
    XMStoreFloat3(&extent, high - low);
    int32_t cells[3] = { static_cast<int32_t>(extent.x / radius) + 1, static_cast<int32_t>(extent.y / radius) + 1, static_cast<int32_t>(extent.z / radius) + 1 };
    size_t cellCount = static_cast<size_t>(cells[0]) * cells[1] * cells[2];
    auto cellOf = [&](const XMFLOAT3& p, int32_t cell[3])
    {
        cell[0] = static_cast<int32_t>(std::floor((p.x - origin.x) / radius));
        cell[1] = static_cast<int32_t>(std::floor((p.y - origin.y) / radius));
        cell[2] = static_cast<int32_t>(std::floor((p.z - origin.z) / radius));
        return cell[0] >= 0 && cell[1] >= 0 && cell[2] >= 0 && cell[0] < cells[0] && cell[1] < cells[1] && cell[2] < cells[2];
    };
    auto indexOf = [&](int32_t x, int32_t y, int32_t z) { return (static_cast<size_t>(z) * cells[1] + y) * cells[0] + x; };

    std::vector<SegmentData> segments;
    std::vector<uint32_t> segmentCells;
    for (uint32_t c = 0; c < curves.size(); c++)
        for (size_t i = 0; i + 1 < curves[c].Points.size(); i++)
        {
            const XMFLOAT3& a = curves[c].Points[i];
            const XMFLOAT3& b = curves[c].Points[i + 1];
            XMFLOAT3 middle = { 0.5f * (a.x + b.x), 0.5f * (a.y + b.y), 0.5f * (a.z + b.z) };
            int32_t cell[3];
            if (!cellOf(middle, cell))
                continue;
            SegmentData segment;
            segment.Start[0] = a.x;
            segment.Start[1] = a.y;
            segment.Start[2] = a.z;
            segment.Step[0] = b.x - a.x;
            segment.Step[1] = b.y - a.y;
            segment.Step[2] = b.z - a.z;
            segment.LengthSquared = segment.Step[0] * segment.Step[0] + segment.Step[1] * segment.Step[1] + segment.Step[2] * segment.Step[2];
            segment.Along = curves[c].Along[i];
            segment.Curve = c;
            segments.push_back(segment);
            segmentCells.push_back(static_cast<uint32_t>(indexOf(cell[0], cell[1], cell[2])));
        }
    std::vector<uint32_t> cellStarts(cellCount + 1, 0);
    for (uint32_t cell : segmentCells)
        cellStarts[cell + 1]++;
    for (size_t i = 0; i < cellCount; i++)
        cellStarts[i + 1] += cellStarts[i];
    std::vector<uint32_t> cellSegments(segments.size());
    {
        std::vector<uint32_t> filled(cellStarts.begin(), cellStarts.end() - 1);
        for (uint32_t i = 0; i < segments.size(); i++)
            cellSegments[filled[segmentCells[i]]++] = i;
    }
    std::vector<uint8_t> isNearCurve(cellCount, 0);
    for (uint32_t cell : segmentCells)
    {
        int32_t x = static_cast<int32_t>(cell % cells[0]), y = static_cast<int32_t>((cell / cells[0]) % cells[1]), z = static_cast<int32_t>(cell / (cells[0] * cells[1]));
        for (int dz = -1; dz <= 1; dz++)
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    if (x + dx >= 0 && y + dy >= 0 && z + dz >= 0 && x + dx < cells[0] && y + dy < cells[1] && z + dz < cells[2])
                        isNearCurve[indexOf(x + dx, y + dy, z + dz)] = 1;
    }

    // Raw pointers: this runs for every skin vertex, and checked iterators make Debug builds crawl.
    const SegmentData* segmentData = segments.data();
    const uint32_t* starts = cellStarts.data();
    const uint32_t* listed = cellSegments.data();
    auto place = [&](const XMFLOAT3& point, const XMFLOAT3& normal, int32_t onlyCurve, uint32_t& nearestCurve) -> CurvePlace
    {
        CurvePlace best;
        int32_t cell[3];
        if (!cellOf(point, cell) || !isNearCurve[indexOf(cell[0], cell[1], cell[2])])
            return best;
        float bestSquared = 1e30f;
        const SegmentData* bestSegment = nullptr;
        float bestT = 0.0f;
        for (int dz = -1; dz <= 1; dz++)
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                {
                    int32_t x = cell[0] + dx, y = cell[1] + dy, z = cell[2] + dz;
                    if (x < 0 || y < 0 || z < 0 || x >= cells[0] || y >= cells[1] || z >= cells[2])
                        continue;
                    size_t index = indexOf(x, y, z);
                    for (uint32_t i = starts[index]; i < starts[index + 1]; i++)
                    {
                        const SegmentData& segment = segmentData[listed[i]];
                        if (onlyCurve >= 0 && segment.Curve != static_cast<uint32_t>(onlyCurve))
                            continue;
                        float px = point.x - segment.Start[0], py = point.y - segment.Start[1], pz = point.z - segment.Start[2];
                        float t = 0.0f;
                        if (segment.LengthSquared > 0.0f)
                        {
                            t = (px * segment.Step[0] + py * segment.Step[1] + pz * segment.Step[2]) / segment.LengthSquared;
                            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
                        }
                        float ex = px - t * segment.Step[0], ey = py - t * segment.Step[1], ez = pz - t * segment.Step[2];
                        float squared = ex * ex + ey * ey + ez * ez;
                        if (squared < bestSquared)
                        {
                            bestSquared = squared;
                            bestSegment = &segment;
                            bestT = t;
                        }
                    }
                }
        if (!bestSegment)
            return best;
        const SegmentData& segment = *bestSegment;
        float ex = point.x - segment.Start[0] - bestT * segment.Step[0];
        float ey = point.y - segment.Start[1] - bestT * segment.Step[1];
        float ez = point.z - segment.Start[2] - bestT * segment.Step[2];
        float sx = segment.Step[1] * normal.z - segment.Step[2] * normal.y;
        float sy = segment.Step[2] * normal.x - segment.Step[0] * normal.z;
        float sz = segment.Step[0] * normal.y - segment.Step[1] * normal.x;
        best.Distance = std::sqrt(bestSquared);
        best.Along = segment.Along + bestT * std::sqrt(segment.LengthSquared);
        best.Signed = ex * sx + ey * sy + ez * sz >= 0.0f ? best.Distance : -best.Distance;
        nearestCurve = segment.Curve;
        return best;
    };

    if (isAuthored)
    {
        std::vector<float> lengthsU, lengthsV;
        for (Mesh* mesh : skinMeshes)
        {
            SkinnedVertexCache* cache = mesh->GetSkinnedCache();
            for (SkinnedVertex& vertex : cache->Vertices)
            {
                uint32_t curveIndex = 0;
                CurvePlace nearest = place(vertex.Position, vertex.Normal, -1, curveIndex);
                vertex.PatchWeight = nearest.Distance <= radius ? FadedWeight(nearest, curves[curveIndex], radius) : 0.0f;
                vertex.PatchChart = 0.0f;
            }
            for (size_t t = 0; t + 2 < cache->Indices.size(); t += 3)
            {
                const SkinnedVertex& a = cache->Vertices[cache->Indices[t]];
                const SkinnedVertex& b = cache->Vertices[cache->Indices[t + 1]];
                const SkinnedVertex& c = cache->Vertices[cache->Indices[t + 2]];
                XMVECTOR edge1 = XMLoadFloat3(&b.Position) - XMLoadFloat3(&a.Position), edge2 = XMLoadFloat3(&c.Position) - XMLoadFloat3(&a.Position);
                float du1 = b.TexCoord1.x - a.TexCoord1.x, dv1 = b.TexCoord1.y - a.TexCoord1.y;
                float du2 = c.TexCoord1.x - a.TexCoord1.x, dv2 = c.TexCoord1.y - a.TexCoord1.y;
                float determinant = du1 * dv2 - du2 * dv1;
                if (std::abs(determinant) < 1e-12f)
                    continue;
                lengthsU.push_back(XMVectorGetX(XMVector3Length((edge1 * dv2 - edge2 * dv1) / determinant)) * millimetresPerUnit);
                lengthsV.push_back(XMVectorGetX(XMVector3Length((edge2 * du1 - edge1 * du2) / determinant)) * millimetresPerUnit);
            }
        }
        if (lengthsU.empty())
            return {};
        std::nth_element(lengthsU.begin(), lengthsU.begin() + lengthsU.size() / 2, lengthsU.end());
        std::nth_element(lengthsV.begin(), lengthsV.begin() + lengthsV.size() / 2, lengthsV.end());
        return { { lengthsU[lengthsU.size() / 2], lengthsV[lengthsV.size() / 2], false } };
    }

    // Each triangle with a corner within the strip width goes to the curve nearest its nearest
    // corner; its vertices are duplicated into that curve's patch, and on a closed curve a triangle
    // straddling the start is unwrapped. Every vertex is placed once against all curves, and again
    // against its triangle's curve only where that is a different one.
    // A patch chart is about square in millimetres, so its pages are: a closed curve's spans the
    // curve along U, which wraps, and an open curve's a square with the curve along the middle of U.
    // The strip runs along the middle of V either way.
    std::vector<XMFLOAT3> chartSizes;           // U and V size in model units, then the U offset
    for (const Curve& curve : curves)
    {
        float length = curve.Along.back();
        if (curve.IsClosed)
            chartSizes.push_back({ length, (std::max)(length, 4.0f * radius), 0.0f });
        else
        {
            float size = (std::max)(length + 2.0f * radius, 4.0f * radius);
            chartSizes.push_back({ size, size, 0.5f - 0.5f * length / size });
        }
    }

    for (size_t m = 0; m < skinMeshes.size(); m++)
    {
        SkinnedVertexCache* cache = skinMeshes[m]->GetSkinnedCache();
        std::vector<SkinnedVertex>& vertices = cache->Vertices;
        size_t originalCount = vertices.size();
        std::vector<CurvePlace> nearestPlaces(originalCount);
        std::vector<uint32_t> nearestCurves(originalCount, 0);
        for (size_t v = 0; v < originalCount; v++)
            nearestPlaces[v] = place(vertices[v].Position, vertices[v].Normal, -1, nearestCurves[v]);

        std::unordered_map<uint64_t, uint32_t> duplicates;
        uint32_t* indices = cache->Indices.data();
        for (size_t t = 0; t + 2 < cache->Indices.size(); t += 3)
        {
            int nearestCorner = 0;
            for (int k = 1; k < 3; k++)
                if (nearestPlaces[indices[t + k]].Distance < nearestPlaces[indices[t + nearestCorner]].Distance)
                    nearestCorner = k;
            if (nearestPlaces[indices[t + nearestCorner]].Distance > radius)
                continue;

            uint32_t curveIndex = nearestCurves[indices[t + nearestCorner]];
            const Curve& curve = curves[curveIndex];
            float length = curve.Along.back();
            CurvePlace corners[3];
            for (int k = 0; k < 3; k++)
            {
                uint32_t original = indices[t + k];
                if (nearestCurves[original] == curveIndex && nearestPlaces[original].Distance <= 2.0f * radius)
                    corners[k] = nearestPlaces[original];
                else
                {
                    uint32_t ignored = 0;
                    corners[k] = place(vertices[original].Position, vertices[original].Normal, static_cast<int32_t>(curveIndex), ignored);
                    if (corners[k].Distance > 2.0f * radius)
                        corners[k] = { 2.0f * radius, nearestPlaces[indices[t + nearestCorner]].Along, radius };
                }
            }
            float lowest = (std::min)({ corners[0].Along, corners[1].Along, corners[2].Along });
            float highest = (std::max)({ corners[0].Along, corners[1].Along, corners[2].Along });
            bool isUnwrapped = curve.IsClosed && highest - lowest > 0.5f * length;

            for (int k = 0; k < 3; k++)
            {
                uint32_t original = indices[t + k];
                bool isShifted = isUnwrapped && corners[k].Along < 0.5f * length;
                uint64_t key = (static_cast<uint64_t>(original) << 32) | (static_cast<uint64_t>(curveIndex) << 1) | (isShifted ? 1u : 0u);
                auto [found, isNew] = duplicates.try_emplace(key, static_cast<uint32_t>(vertices.size()));
                if (isNew)
                {
                    SkinnedVertex vertex = vertices[original];
                    float along = corners[k].Along + (isShifted ? length : 0.0f);
                    const XMFLOAT3& size = chartSizes[curveIndex];
                    vertex.TexCoord1 = { size.z + along / size.x, 0.5f + std::clamp(corners[k].Signed, -radius, radius) / size.y };
                    vertex.PatchWeight = FadedWeight(corners[k], curve, radius);
                    vertex.PatchChart = static_cast<float>(curveIndex);
                    vertices.push_back(vertex);
                }
                indices[t + k] = found->second;
            }
        }
        skinMeshes[m]->RefreshCounts();
    }

    std::vector<PatchChart> charts;
    for (size_t c = 0; c < curves.size(); c++)
        charts.push_back({ chartSizes[c].x * millimetresPerUnit, chartSizes[c].y * millimetresPerUnit, curves[c].IsClosed });
    return charts;
}
