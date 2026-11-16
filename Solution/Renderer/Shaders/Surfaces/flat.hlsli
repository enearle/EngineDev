cbuffer FlatParameters : register(b0, space2)
{
    float3 Albedo;
    float Metallic;
    float Roughness;
};

Surface EvaluateSurface(SurfaceInput input)
{
    Surface surface;
    surface.Albedo = Albedo;
    surface.Normal = normalize(input.Normal);
    surface.Metallic = Metallic;
    surface.Roughness = Roughness;
    surface.Occlusion = 1.0;
    return surface;
}
