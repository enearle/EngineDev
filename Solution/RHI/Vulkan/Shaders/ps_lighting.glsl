#version 450

layout(location = 0) in vec2 inUV;

layout(set = 0, binding = 0) uniform VPData {
    mat4 viewProjection;
    vec4 cameraPosition;
} vpData;

layout(set = 1, binding = 0) uniform sampler2DArray shadowMaps;

layout(set = 2, binding = 0) uniform sampler2D subAlbedo;
layout(set = 2, binding = 1) uniform sampler2D subNormal;
layout(set = 2, binding = 2) uniform sampler2D subMetalicRoughnessAO;
layout(set = 2, binding = 3) uniform sampler2D subPosition;

struct Light
{
    vec3 Position;
    vec3 Colour;
    float Intensity;
    float Radius;
    uint Type;
    float Angle;
    uint ShadowIndex;
};

layout(set = 3, binding = 0) uniform LightData
{
    Light Lights[4];
} lightData;

layout(set = 4, binding = 0) uniform LightMatrices {
    mat4 lightVP[4];
} lightMatrices;

layout(location = 0) out vec4 outColour;

#define PI 3.14159265358979323846

vec3 albedo;
vec3 normal;
vec3 fragPosition;
float roughness;
float metallic;
float ambientOcclusion;
vec3 viewVector;
vec3 materialData;

void init()
{
    albedo = texture(subAlbedo, inUV).rgb;
    normal = texture(subNormal, inUV).rgb;
    materialData = texture(subMetalicRoughnessAO, inUV).rgb;
    fragPosition = texture(subPosition, inUV).rgb;
    
    metallic = materialData.r;
    roughness = max(materialData.g, 0.04);
    roughness = max(roughness * roughness, 0.001);
    ambientOcclusion = materialData.b;
    
    vec3 camPosition = vpData.cameraPosition.rgb;
    viewVector = normalize(camPosition - fragPosition);
}

// GGX/Throwbridge-Reitz normal distribution
float NormalDistribution(vec3 inHalfwayVector)
{
    float roughness2 = roughness * roughness;
    float nDotH2 = max(dot(normal, inHalfwayVector), 0.0001);
    nDotH2 *= nDotH2;
    float denominator = nDotH2 * (roughness2 - 1) + 1;
    denominator = max(denominator * denominator * PI, 0.0001);

    return roughness2 / denominator;
}

// Schlick-Beckman geometry shadowing
float GeomertryShadowingSupport(vec3 inVector)
{
    float nDotV = max(dot(normal, inVector), 0.0001);

    float halfRoughness = roughness * 0.5;
    float denominator = nDotV * (1.0 - halfRoughness) + halfRoughness;
    denominator = max(denominator, 0.0001);

    return nDotV / denominator;
}

float GeometryShadowing(vec3 lightVector)
{
    return GeomertryShadowingSupport(viewVector) * GeomertryShadowingSupport(lightVector);
}

// Fresnel
vec3 Fresnel(vec3 inHalfwayVector)
{
    float f5 = 1 - max(dot(viewVector, inHalfwayVector), 0.0);
    f5 = f5 * f5 * f5 * f5 * f5;

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    return F0 + (vec3(1) - F0) * f5;
}

// Attenuation for point light
vec3 AttenuateLight(Light light)
{
    
    vec3 lightVector = light.Position - fragPosition;
    float distance = length(lightVector);

    // Smooth attenuation
    float attenuation = 1.0 - clamp(distance / light.Radius, 0.0, 1.0);
    attenuation = attenuation * attenuation * light.Intensity;

    return attenuation * light.Colour;
}

float Shadow(Light light, vec3 lightDirection)
{
    vec4 samplePos = lightMatrices.lightVP[light.ShadowIndex] * vec4(fragPosition, 1);
    
    vec3 ndc = samplePos.xyz / samplePos.w;
    ndc.y = -ndc.y;
    vec2 sampleUV = ndc.xy * 0.5 + 0.5;

    if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
    return 1.0;
    
    vec2 m1m2 = texture(shadowMaps, vec3(sampleUV, light.ShadowIndex)).xy;
    
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
vec3 LightPBR(Light light)
{
    vec3 lightColour = AttenuateLight(light);

    vec3 lightDirection = normalize(light.Position.xyz - fragPosition);
    vec3 halfwayVector = normalize(lightDirection + viewVector);

    vec3 fresnel = Fresnel(halfwayVector);
    vec3 lambert = albedo / PI;

    vec3 cookTorranceNumerator = NormalDistribution(halfwayVector) * GeometryShadowing(lightDirection) * fresnel;
    float cookTorranceDenominator = 4.0 * max(dot(viewVector, normal), 0.0001) * max(dot(lightDirection, normal), 0.0001);
    cookTorranceDenominator = max(cookTorranceDenominator, 0.0001);
    vec3 cookTorrance = cookTorranceNumerator / cookTorranceDenominator;

    vec3 bRDF = ((vec3(1) - fresnel) * (1.0 - metallic)) * lambert + cookTorrance;

    return  bRDF * lightColour * max(dot(lightDirection, normal), 0.0001) * Shadow(light, lightDirection);
}

void main()
{
    init();

    vec3 outGoingLight = vec3(0, 0, 0);
    for(uint i = 0; i < 4; i++)
    {
        if(lightData.Lights[i].Type == 1) 
        {
            outGoingLight += LightPBR(lightData.Lights[i]);
        }
    }

    outColour = vec4(outGoingLight, 1.0);
}
