struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

#define PI 3.14159265358979323846

// How far the diffuse term wraps past the terminator at subsurface strength 1, per colour channel:
// light scatters furthest in red. One scatter colour for every material for now.
static const float3 ScatterWrap = float3(0.5, 0.2, 0.1);

#include "Colour.hlsli"
#include "Common.hlsli"

Texture2DArray shadowMaps           : register(t0, space3);
Texture2D inAlbedo                  : register(t1, space3);
Texture2D<float2> inNormal          : register(t2, space3);
Texture2D inMetallicRoughnessAO     : register(t3, space3);
Texture2D<float> inDepth            : register(t4, space3);
SamplerState pointClampSampler      : register(s1, space0);
SamplerState linearClampSampler     : register(s2, space0);

static float3 albedo;
static float3 normal;
static float3 fragPosition;
static float roughness;
static float metallic;
static float ambientOcclusion;
static float3 viewVector;
static float4 materialData;
static float subsurface;

void init(VSOutput input, float depth)
{
    albedo = DecodeSrgb(inAlbedo.Sample(pointClampSampler, input.uv).rgb);
    normal = DecodeNormal(inNormal.Sample(pointClampSampler, input.uv));
    materialData = inMetallicRoughnessAO.Sample(pointClampSampler, input.uv);

    float4 clip = float4(input.uv.x * 2.0 - 1.0, 1.0 - input.uv.y * 2.0, depth, 1.0);
    float4 world = mul(InverseViewProjection, clip);
    fragPosition = world.xyz / world.w;
    
    metallic = materialData.r;
    roughness = max(materialData.g, 0.04);
    roughness = max(roughness * roughness, 0.001);
    ambientOcclusion = materialData.b;
    subsurface = materialData.a;
    
    viewVector = normalize(CameraPosition.xyz - fragPosition);
}

// GGX/Throwbridge-Reitz normal distribution
float NormalDistribution(float3 inHalfwayVector)
{
    float roughness2 = roughness * roughness;
    float nDotH2 = max(dot(normal, inHalfwayVector), 0.0001);
    nDotH2 *= nDotH2;
    float denominator = nDotH2 * (roughness2 - 1) + 1;
    denominator = max(denominator * denominator * PI, 0.0001);

    return roughness2 / denominator;
}

// Schlick-Beckman geometry shadowing
float GeomertryShadowingSupport(float3 inVector)
{
    float nDotV = max(dot(normal, inVector), 0.0001);

    float halfRoughness = roughness * 0.5;
    float denominator = nDotV * (1.0 - halfRoughness) + halfRoughness;
    denominator = max(denominator, 0.0001);

    return nDotV / denominator;
}

float GeometryShadowing(float3 lightVector)
{
    return GeomertryShadowingSupport(viewVector) * GeomertryShadowingSupport(lightVector);
}

// Fresnel
float3 Fresnel(float3 inHalfwayVector)
{
    float f5 = 1 - max(dot(viewVector, inHalfwayVector), 0.0);
    f5 = f5 * f5 * f5 * f5 * f5;

    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    return F0 + (float3(1,1,1) - F0) * f5;
}

// Attenuation for point light
float3 AttenuateLight(Light light)
{
    
    float3 lightVector = light.Position - fragPosition;
    float distance = length(lightVector);

    // Smooth attenuation
    float attenuation = 1.0 - clamp(distance / light.Radius, 0.0, 1.0);
    attenuation = attenuation * attenuation * light.Intensity;

    return attenuation * light.Colour;
}

float Shadow(Light light, float3 lightDirection)
{
    float4 samplePos = mul(LightViewProjection[light.ShadowIndex], float4(fragPosition, 1));
    
    float3 ndc = samplePos.xyz / samplePos.w;
    ndc.y = -ndc.y;
    float2 sampleUV = ndc.xy * 0.5 + 0.5;

    if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
        return 1.0;
    
    float2 m1m2 = shadowMaps.Sample(linearClampSampler, float3(sampleUV, light.ShadowIndex)).xy;
    
    float t = abs(samplePos.w) / light.Radius;
    
    // No pcf
    float u = m1m2.x;
    float o2 = max(m1m2.y - u * u, 0.0001);
    
    if (t <= u)
        return 1.0;

    // Chebyshev's Inequality
    float pMax = o2 / (o2 + (t - u) * (t - u));
    
    return pMax;
}

// PBR lighting calculation
float3 LightPBR(Light light)
{
    float3 lightColour = AttenuateLight(light);

    float3 lightDirection = normalize(light.Position.xyz - fragPosition);
    float3 halfwayVector = normalize(lightDirection + viewVector);

    float3 fresnel = Fresnel(halfwayVector);
    float3 lambert = albedo / PI;

    float3 cookTorranceNumerator = NormalDistribution(halfwayVector) * GeometryShadowing(lightDirection) * fresnel;
    float cookTorranceDenominator = 4.0 * max(dot(viewVector, normal), 0.0001) * max(dot(lightDirection, normal), 0.0001);
    cookTorranceDenominator = max(cookTorranceDenominator, 0.0001);
    float3 cookTorrance = cookTorranceNumerator / cookTorranceDenominator;

    float3 diffuse = ((float3(1,1,1) - fresnel) * (1.0 - metallic)) * lambert;
    float nDotL = dot(lightDirection, normal);

    // Subsurface: the cosine wraps by w per channel, (n.l + w) / (1 + w)^2, which integrates over the
    // sphere to the same energy as the plain cosine over the hemisphere. Specular is unchanged.
    float3 diffuseCosine = max(nDotL, 0.0001);
    if (subsurface > 0.0)
    {
        float3 wrap = subsurface * ScatterWrap;
        diffuseCosine = max(nDotL + wrap, 0.0) / ((1.0 + wrap) * (1.0 + wrap));
    }

    return (diffuse * diffuseCosine + cookTorrance * max(nDotL, 0.0001)) * lightColour * Shadow(light, lightDirection);
}

float4 main(VSOutput input) : SV_TARGET
{
    float depth = inDepth.Sample(pointClampSampler, input.uv);
    if (depth <= 0.0)
        return float4(0.0, 0.0, 0.0, 1.0);
    init(input, depth);
    
    float3 outGoingLight = float3(0,0,0);
    for (uint i = 0; i < 4; i++)
    {
        if (Lights[i].Type == 1) outGoingLight += LightPBR(Lights[i]);
    }
    return float4(EncodeSrgb(outGoingLight), 1.0);
}