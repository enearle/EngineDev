#pragma once
#include <vector>
#include "Mesh.h"

// A seam patch chart: a strip around one seam curve, parameterised by distance along the curve (U)
// and signed distance from it (V), both in proportion to millimetres. The chart is about square, the
// strip running along the middle of V; a closed curve spans U exactly, so the chart wraps along U,
// and TexCoord1.x runs past 1 on triangles that straddle the curve's start.
struct PatchChart
{
    float MillimetresU = 0.0f;
    float MillimetresV = 0.0f;
    bool IsPeriodic = false;            // A closed curve, such as the neck, wraps along U
};

// Generated seam patches for skin meshes with no second UV map. A seam edge is one whose two sides
// differ in UV or in mesh; edges with one face are openings. Seam edges are chained into curves,
// split where three or more meet, and each curve gets a strip of the faces within StripMillimetres,
// assigned to their nearest curve. Vertices on a strip's border are duplicated, so a triangle
// never mixes two patch charts.
//
// Each vertex in a strip gets TexCoord1 (the patch UV), PatchChart (the strip's index) and
// PatchWeight: 1 on the seam, 0 at the strip's edge, faded to 0 over a quarter of the strip's width
// around the ends of open curves, where no two-layer blend is continuous. Distances are straight
// lines to the seam polyline rather than along the surface.
//
// With an authored second UV map nothing is generated: TexCoord1 is the authored UV, every vertex
// is in patch 0, and the weights come from the same seam curves.
class SeamPatches
{
public:
    static constexpr float StripMillimetres = 30.0f;
    static constexpr float MinimumCurveMillimetres = 5.0f;

    static std::vector<PatchChart> Generate(const std::vector<Mesh*>& skinMeshes, float millimetresPerUnit, bool isAuthored);
};
