#define SURFACE_WRITES_FEEDBACK
#include "../Paging.hlsli"

cbuffer PagedAlbedoParameters : register(b0, space2)
{
    float PagedTexture;
    float Metallic;
    float Roughness;
};

Surface EvaluateSurface(SurfaceInput input)
{
    uint texture = (uint)PagedTexture;
    PagedSample paged = SamplePagedTexture(texture, input.UV, ddx(input.UV), ddy(input.UV));
    WritePagingFeedback(input.PixelPosition, texture, paged.WantedLevel, paged.WantedPage);

    Surface surface;
    surface.Albedo = DecodeSrgb(paged.Colour.rgb);
    if (PagingIsTinted != 0)
        surface.Albedo = lerp(surface.Albedo, PagingLevelTint(paged.Level), 0.6);
    surface.Normal = normalize(input.Normal);
    surface.Metallic = Metallic;
    surface.Roughness = Roughness;
    surface.Occlusion = 1.0;
    surface.Subsurface = 0.0;
    return surface;
}
