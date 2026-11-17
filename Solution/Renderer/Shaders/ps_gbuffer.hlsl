// Compiled once per file in Surfaces, with SURFACE naming it, into ps_gbuffer_<name>.
#include "Surface.hlsli"

#define SURFACE_PATH_STRING(path) #path
#define SURFACE_PATH(name) SURFACE_PATH_STRING(Surfaces/name.hlsli)
#include SURFACE_PATH(SURFACE)

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    float2 uv : TEXCOORD;
    float3 worldPosition : TEXCOORD1;
    float2 uv1 : TEXCOORD2;
    float2 patch : TEXCOORD3;
};

struct PSOutput
{
    float4 albedo : SV_TARGET0;
    float2 normal : SV_TARGET1;
    float4 material : SV_TARGET2;
};

// A surface that writes a buffer would lose early depth testing unless it asks for it.
#ifdef SURFACE_WRITES_FEEDBACK
[earlydepthstencil]
#endif
PSOutput main(PSInput input)
{
    SurfaceInput surfaceInput;
    surfaceInput.WorldPosition = input.worldPosition;
    surfaceInput.Normal = input.normal;
    surfaceInput.Tangent = input.tangent;
    surfaceInput.Bitangent = input.binormal;
    surfaceInput.UV = input.uv;
    surfaceInput.UV1 = input.uv1;
    surfaceInput.PatchWeight = input.patch.x;
    surfaceInput.PatchChart = input.patch.y;
    surfaceInput.PixelPosition = input.position;
    Surface surface = EvaluateSurface(surfaceInput);

    PSOutput output;
    output.albedo = float4(EncodeSrgb(surface.Albedo), 1.0);
    output.normal = EncodeNormal(surface.Normal);
    output.material = float4(surface.Metallic, surface.Roughness, surface.Occlusion, surface.Subsurface);
    return output;
}
