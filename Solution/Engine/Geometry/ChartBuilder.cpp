#include "ChartBuilder.h"

#include <algorithm>
#include <cmath>
#include "../Animation/Skeleton.h"

using namespace DirectX;

static constexpr uint32_t DilationTexels = 4;

struct TriangleFrame
{
    XMVECTOR AlongU;            // Model-space length per UV unit along U and V
    XMVECTOR AlongV;
    bool IsValid = false;
};

static TriangleFrame MeasureTriangle(const ChartBuilder::ChartVertex& a, const ChartBuilder::ChartVertex& b, const ChartBuilder::ChartVertex& c)
{
    XMVECTOR edge1 = XMLoadFloat3(&b.Position) - XMLoadFloat3(&a.Position);
    XMVECTOR edge2 = XMLoadFloat3(&c.Position) - XMLoadFloat3(&a.Position);
    float du1 = b.TexCoord.x - a.TexCoord.x, dv1 = b.TexCoord.y - a.TexCoord.y;
    float du2 = c.TexCoord.x - a.TexCoord.x, dv2 = c.TexCoord.y - a.TexCoord.y;
    float determinant = du1 * dv2 - du2 * dv1;

    TriangleFrame frame;
    if (std::abs(determinant) < 1e-12f)
        return frame;
    frame.AlongU = (edge1 * dv2 - edge2 * dv1) / determinant;
    frame.AlongV = (edge2 * du1 - edge1 * du2) / determinant;
    frame.IsValid = true;
    return frame;
}

static float Median(std::vector<float>& values)
{
    if (values.empty())
        return 0.0f;
    std::nth_element(values.begin(), values.begin() + values.size() / 2, values.end());
    return values[values.size() / 2];
}

// Lines of the direction field circle the bone a vertex follows: the tangent perpendicular to the
// axis from its main joint to the joint's only child, or from the joint's parent.
static std::vector<XMVECTOR> BuildDirections(std::span<const ChartBuilder::ChartVertex> vertices, const Skeleton& skeleton)
{
    uint32_t jointCount = skeleton.GetJointCount();
    std::vector<XMVECTOR> positions(jointCount);
    std::vector<int32_t> onlyChild(jointCount, -1);
    std::vector<uint32_t> childCount(jointCount, 0);
    for (uint32_t j = 0; j < jointCount; j++)
    {
        positions[j] = XMMatrixInverse(nullptr, XMLoadFloat4x4(&skeleton.GetJoint(j).InverseBind)).r[3];
        int32_t parent = skeleton.GetJoint(j).Parent;
        if (parent >= 0)
        {
            childCount[parent]++;
            onlyChild[parent] = static_cast<int32_t>(j);
        }
    }

    std::vector<XMVECTOR> directions(vertices.size());
    for (size_t i = 0; i < vertices.size(); i++)
    {
        int32_t joint = vertices[i].MainJoint;
        XMVECTOR axis = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        if (joint >= 0 && childCount[joint] == 1)
            axis = positions[onlyChild[joint]] - positions[joint];
        else if (joint >= 0 && skeleton.GetJoint(joint).Parent >= 0)
            axis = positions[joint] - positions[skeleton.GetJoint(joint).Parent];
        XMVECTOR direction = XMVector3Cross(axis, XMLoadFloat3(&vertices[i].Normal));
        directions[i] = XMVectorGetX(XMVector3LengthSq(direction)) > 1e-20f ? XMVector3Normalize(direction) : XMVectorZero();
    }
    return directions;
}

// A direction in model space as an angle in the chart's lattice frame, where both axes are in
// millimetres, returned doubled as (cos, sin) so opposite directions agree and averaging works.
static XMFLOAT2 DoubledAngleInChart(XMVECTOR direction, const TriangleFrame& frame, float millimetresU, float millimetresV)
{
    float uu = XMVectorGetX(XMVector3Dot(frame.AlongU, frame.AlongU));
    float uv = XMVectorGetX(XMVector3Dot(frame.AlongU, frame.AlongV));
    float vv = XMVectorGetX(XMVector3Dot(frame.AlongV, frame.AlongV));
    float du = XMVectorGetX(XMVector3Dot(direction, frame.AlongU));
    float dv = XMVectorGetX(XMVector3Dot(direction, frame.AlongV));
    float determinant = uu * vv - uv * uv;
    if (std::abs(determinant) < 1e-20f)
        return { 0.0f, 0.0f };
    float a = (vv * du - uv * dv) / determinant;
    float b = (uu * dv - uv * du) / determinant;
    float angle = std::atan2(b * millimetresV, a * millimetresU);
    return { std::cos(2.0f * angle), std::sin(2.0f * angle) };
}

static uint8_t ToByte(float value)
{
    return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

MeshChart ChartBuilder::Build(std::span<const ChartVertex> vertices, std::span<const uint32_t> indices, const Skeleton* skeleton,
                              float millimetresPerUnit)
{
    std::vector<TriangleFrame> frames(indices.size() / 3);
    std::vector<float> lengthsU, lengthsV;
    for (size_t t = 0; t < frames.size(); t++)
    {
        frames[t] = MeasureTriangle(vertices[indices[t * 3]], vertices[indices[t * 3 + 1]], vertices[indices[t * 3 + 2]]);
        if (!frames[t].IsValid)
            continue;
        lengthsU.push_back(XMVectorGetX(XMVector3Length(frames[t].AlongU)) * millimetresPerUnit);
        lengthsV.push_back(XMVectorGetX(XMVector3Length(frames[t].AlongV)) * millimetresPerUnit);
    }

    MeshChart chart;
    chart.MillimetresU = Median(lengthsU);
    chart.MillimetresV = Median(lengthsV);
    chart.MillimetresPerUnit = millimetresPerUnit;
    if (!skeleton || chart.MillimetresU <= 0.0f || chart.MillimetresV <= 0.0f)
        return chart;

    // Per texel: the doubled-angle direction, local scale against the median, and coverage.
    const uint32_t size = MeshChart::MapSize;
    std::vector<XMFLOAT4> texels(static_cast<size_t>(size) * size, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
    std::vector<uint8_t> isCovered(texels.size(), 0);
    std::vector<XMVECTOR> directions = BuildDirections(vertices, *skeleton);
    float medianScale = std::sqrt(chart.MillimetresU * chart.MillimetresV);

    for (size_t t = 0; t < frames.size(); t++)
    {
        if (!frames[t].IsValid)
            continue;
        const ChartVertex* corner[3] = { &vertices[indices[t * 3]], &vertices[indices[t * 3 + 1]], &vertices[indices[t * 3 + 2]] };
        XMFLOAT2 doubled = { 0.0f, 0.0f };
        for (int k = 0; k < 3; k++)
        {
            XMFLOAT2 each = DoubledAngleInChart(directions[indices[t * 3 + k]], frames[t], chart.MillimetresU, chart.MillimetresV);
            doubled.x += each.x;
            doubled.y += each.y;
        }
        float length = std::sqrt(doubled.x * doubled.x + doubled.y * doubled.y);
        if (length > 1e-6f)
        {
            doubled.x /= length;
            doubled.y /= length;
        }
        float scale = std::sqrt(XMVectorGetX(XMVector3Length(frames[t].AlongU)) * XMVectorGetX(XMVector3Length(frames[t].AlongV))) *
                      millimetresPerUnit / medianScale;
        float encodedScale = 0.5f + std::log2((std::max)(scale, 1e-3f)) / 4.0f;

        float x[3], y[3];
        for (int k = 0; k < 3; k++)
        {
            x[k] = corner[k]->TexCoord.x * size;
            y[k] = corner[k]->TexCoord.y * size;
        }
        int minX = (std::max)(0, static_cast<int>(std::floor((std::min)({ x[0], x[1], x[2] }))));
        int maxX = (std::min)(static_cast<int>(size) - 1, static_cast<int>(std::ceil((std::max)({ x[0], x[1], x[2] }))));
        int minY = (std::max)(0, static_cast<int>(std::floor((std::min)({ y[0], y[1], y[2] }))));
        int maxY = (std::min)(static_cast<int>(size) - 1, static_cast<int>(std::ceil((std::max)({ y[0], y[1], y[2] }))));
        float area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
        if (std::abs(area) < 1e-12f)
            continue;

        for (int py = minY; py <= maxY; py++)
            for (int px = minX; px <= maxX; px++)
            {
                float cx = px + 0.5f, cy = py + 0.5f;
                float w0 = ((x[1] - cx) * (y[2] - cy) - (x[2] - cx) * (y[1] - cy)) / area;
                float w1 = ((x[2] - cx) * (y[0] - cy) - (x[0] - cx) * (y[2] - cy)) / area;
                float w2 = 1.0f - w0 - w1;
                if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f)
                    continue;
                size_t index = static_cast<size_t>(py) * size + px;
                texels[index] = { doubled.x, doubled.y, encodedScale, 1.0f };
                isCovered[index] = 1;
            }
    }

    // Dilation: each round fills the empty texels beside filled ones with their average, so filtering
    // and the lattice near island borders read sensible values. Coverage stays 0 there. Only the
    // border is visited, which matters for meshes of hundreds of thousands of triangles.
    std::vector<uint8_t> isFilled(isCovered.begin(), isCovered.end());
    std::vector<uint32_t> border;
    auto visitNeighbours = [&](uint32_t index, auto&& visit)
    {
        int px = static_cast<int>(index % size), py = static_cast<int>(index / size);
        for (int oy = -1; oy <= 1; oy++)
            for (int ox = -1; ox <= 1; ox++)
            {
                int nx = px + ox, ny = py + oy;
                if ((ox != 0 || oy != 0) && nx >= 0 && ny >= 0 && nx < static_cast<int>(size) && ny < static_cast<int>(size))
                    visit(static_cast<uint32_t>(ny) * size + static_cast<uint32_t>(nx));
            }
    };
    for (uint32_t index = 0; index < texels.size(); index++)
        if (!isFilled[index])
        {
            bool isBorder = false;
            visitNeighbours(index, [&](uint32_t neighbour) { isBorder |= isFilled[neighbour] != 0; });
            if (isBorder)
                border.push_back(index);
        }

    for (uint32_t round = 0; round < DilationTexels && !border.empty(); round++)
    {
        for (uint32_t index : border)
        {
            XMFLOAT4 sum = { 0.0f, 0.0f, 0.0f, 0.0f };
            int count = 0;
            visitNeighbours(index, [&](uint32_t neighbour)
            {
                if (isFilled[neighbour] != 1)
                    return;
                sum.x += texels[neighbour].x;
                sum.y += texels[neighbour].y;
                sum.z += texels[neighbour].z;
                count++;
            });
            float length = std::sqrt(sum.x * sum.x + sum.y * sum.y);
            texels[index] = { length > 1e-6f ? sum.x / length : 0.0f, length > 1e-6f ? sum.y / length : 0.0f, count > 0 ? sum.z / count : 0.5f, 0.0f };
        }
        for (uint32_t index : border)
            isFilled[index] = 2;
        std::vector<uint32_t> next;
        for (uint32_t index : border)
        {
            isFilled[index] = 1;
            visitNeighbours(index, [&](uint32_t neighbour)
            {
                if (isFilled[neighbour] == 0)
                {
                    isFilled[neighbour] = 3;
                    next.push_back(neighbour);
                }
            });
        }
        for (uint32_t index : next)
            isFilled[index] = 0;
        border = std::move(next);
    }

    chart.Map.resize(texels.size() * 4);
    for (size_t i = 0; i < texels.size(); i++)
    {
        chart.Map[i * 4] = ToByte(texels[i].x * 0.5f + 0.5f);
        chart.Map[i * 4 + 1] = ToByte(texels[i].y * 0.5f + 0.5f);
        chart.Map[i * 4 + 2] = isFilled[i] != 0 ? ToByte(texels[i].z) : 128;
        chart.Map[i * 4 + 3] = isCovered[i] != 0 ? 255 : 0;
    }
    return chart;
}
