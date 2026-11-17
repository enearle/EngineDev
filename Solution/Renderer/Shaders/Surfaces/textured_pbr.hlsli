Texture2D AlbedoTexture : register(t1, space2);
Texture2D NormalTexture : register(t2, space2);
Texture2D MetallicRoughnessOcclusionTexture : register(t3, space2);

Surface EvaluateSurface(SurfaceInput input)
{
    float3 tangentNormal = NormalTexture.Sample(MaterialSampler, input.UV).rgb * 2.0 - 1.0;
    float3x3 tangentFrame = float3x3(normalize(input.Tangent), -normalize(input.Bitangent), normalize(input.Normal));
    float3 metallicRoughnessOcclusion = MetallicRoughnessOcclusionTexture.Sample(MaterialSampler, input.UV).rgb;

    Surface surface;
    surface.Albedo = DecodeSrgb(AlbedoTexture.Sample(MaterialSampler, input.UV).rgb);
    surface.Normal = normalize(mul(tangentNormal, tangentFrame));
    surface.Metallic = metallicRoughnessOcclusion.r;
    surface.Roughness = metallicRoughnessOcclusion.g;
    surface.Occlusion = metallicRoughnessOcclusion.b;
    surface.Subsurface = 0.0;
    return surface;
}
