#version 450

// G-buffer inputs (matching your pipeline bindings)
layout(set = 0, binding = 0) uniform sampler2D subBaseColour;           // Albedo
layout(set = 0, binding = 1) uniform sampler2D subNormal;               // Normal
layout(set = 0, binding = 2) uniform sampler2D subMetalicRoughnessAO;   // Material
layout(set = 0, binding = 3) uniform sampler2D subPosition;             // Position

layout(location = 0) out vec4 outColour;

#define PI 3.14159265358979323846

// Define a simple point light
struct Light
{
    vec3 Position;
    vec3 Colour;
    float Intensity;
    float Radius;
};

// Variables
vec3 albedo;
vec3 normal;
vec3 fragPosition;
float roughness;
float metallic;
float ambientOcclusion;
vec3 viewVector;
vec2 screenPos;

// Initialize variables from G-buffer
void init()
{
    // Screen position in 0-1 range
    screenPos = gl_FragCoord.xy / vec2(1280.0, 720.0);

    // Sample G-buffers
    albedo = texture(subBaseColour, screenPos).rgb;
    normal = normalize(texture(subNormal, screenPos).rgb);
    fragPosition = texture(subPosition, screenPos).rgb;

    vec3 materialData = texture(subMetalicRoughnessAO, screenPos).rgb;
    metallic = materialData.r;
    roughness = materialData.g;
    roughness = max(roughness * roughness, 0.001); // Square and clamp
    ambientOcclusion = materialData.b;

    // Camera is at origin for this test (adjust as needed)
    vec3 camPosition = vec3(0.0, 5.0, 10.0);
    viewVector = normalize(camPosition - fragPosition);
}

// GGX/Trowbridge-Reitz normal distribution
float NormalDistribution(vec3 halfwayVector)
{
    float roughness2 = roughness * roughness;
    float nDotH = max(dot(normal, halfwayVector), 0.0);
    float nDotH2 = nDotH * nDotH;

    float denominator = nDotH2 * (roughness2 - 1.0) + 1.0;
    denominator = PI * denominator * denominator;
    denominator = max(denominator, 0.000001);

    return roughness2 / denominator;
}

// Schlick-Beckmann geometry shadowing
float GeometryShadowingSchlickGGX(float nDotV)
{
    float k = roughness * 0.5;
    float denominator = nDotV * (1.0 - k) + k;
    return nDotV / max(denominator, 0.000001);
}

float GeometryShadowing(vec3 lightVector)
{
    float nDotV = max(dot(normal, viewVector), 0.0);
    float nDotL = max(dot(normal, lightVector), 0.0);
    return GeometryShadowingSchlickGGX(nDotV) * GeometryShadowingSchlickGGX(nDotL);
}

// Fresnel-Schlick
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    float f = pow(1.0 - cosTheta, 5.0);
    return F0 + (vec3(1.0) - F0) * f;
}

// Attenuation for point light
vec3 AttenuateLight(Light light, vec3 fragPos)
{
    vec3 lightVector = light.Position - fragPos;
    float distance = length(lightVector);

    // Smooth attenuation
    float attenuation = 1.0 - clamp(distance / light.Radius, 0.0, 1.0);
    attenuation = attenuation * attenuation * light.Intensity;

    return attenuation * light.Colour;
}

// PBR lighting calculation
vec3 LightPBR(Light light)
{
    vec3 lightDirection = normalize(light.Position - fragPosition);
    vec3 halfwayVector = normalize(lightDirection + viewVector);

    // Calculate PBR terms
    float nDotL = max(dot(normal, lightDirection), 0.0);
    float hDotV = max(dot(halfwayVector, viewVector), 0.0);

    // Fresnel (F0 for dielectrics is 0.04, for metals use albedo)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = FresnelSchlick(hDotV, F0);

    // Distribution and Geometry terms
    float D = NormalDistribution(halfwayVector);
    float G = GeometryShadowing(lightDirection);

    // Cook-Torrance specular BRDF
    vec3 numerator = D * G * F;
    float denominator = 4.0 * max(dot(viewVector, normal), 0.0) * nDotL;
    denominator = max(denominator, 0.000001);
    vec3 specular = numerator / denominator;

    // Diffuse component (kD)
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    // Combine with light attenuation
    vec3 radiance = AttenuateLight(light, fragPosition);

    return (diffuse + specular) * radiance * nDotL;
}

void main()
{
    vec2 screenPos = gl_FragCoord.xy / vec2(1280.0, 720.0);
    vec4 sampledColor = texture(subBaseColour, screenPos);
    outColour = vec4(sampledColor.rgb, 1.0);  // Output exactly what we sampled
}

//void main()
//{
//    init();
//
//    // Define a simple point light in the scene
//    Light light;
//    light.Position = vec3(0.0, 5.0, 0.0);      // Light position in world space
//    light.Colour = vec3(1.0, 1.0, 1.0);        // White light
//    light.Intensity = 20.0;                     // Light intensity
//    light.Radius = 50.0;                        // Light radius
//
//    // Calculate lighting
//    vec3 outGoingLight = LightPBR(light);
//
//    // Add ambient term (very simple ambient occlusion)
//    vec3 ambient = vec3(0.03) * albedo * ambientOcclusion;
//    outGoingLight += ambient;
//
//    // Output final color (no clamp needed, handled by render target)
//    //outColour = vec4(outGoingLight, 1.0);
//    outColour = vec4(albedo, 1.0);
//}
