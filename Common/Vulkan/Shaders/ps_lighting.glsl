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
    screenPos = gl_FragCoord.xy / textureSize(subNormal, 0);

    // Sample G-buffers
    albedo = texture(subBaseColour, screenPos).rgb;
    normal = normalize(texture(subNormal, screenPos).rgb * 2 - 1);
    fragPosition = texture(subPosition, screenPos).rgb;

    vec3 materialData = texture(subMetalicRoughnessAO, screenPos).rgb;
    metallic = materialData.r;
    roughness = max(materialData.g, 0.04);
    roughness = max(roughness * roughness, 0.001); // Square and clamp
    ambientOcclusion = materialData.b;

    // Camera is at origin for this test (adjust as needed)
    vec3 camPosition = vec3(0.0, 10, 8);
    viewVector = normalize(camPosition - fragPosition);
}

// GGX/Throwbridge-Reitz normal distribution
float NormalDistribution(vec3 inHalfwayVector)
{
    float roughness2 = roughness * roughness;
    float nDotH2 = max(dot(normal, inHalfwayVector), 0.0);
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

    return F0 + (vec3(1.0) - F0) * f5;
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

    return  bRDF * lightColour * max(dot(lightDirection, normal), 0.0001);
}

void main()
{
    init();
    
    // Define a simple point light in the scene
    Light light;
    light.Position = vec3(-10.0, 20.0, 10.0);      // Light position in world space
    light.Colour = vec3(1.0, 1.0, 1.0);         // White light
    light.Intensity = 20.0;                     // Light intensity
    light.Radius = 50.0;                        // Light radius

    // Calculate lighting
    vec3 outGoingLight = LightPBR(light);

    // Add ambient term (very simple ambient occlusion)
    //vec3 ambient = vec3(0.03) * albedo * ambientOcclusion;
    //outGoingLight += ambient;

    // Output final color (no clamp needed, handled by render target)
    outColour = vec4(outGoingLight, 1.0);
    //outColour = vec4(normal, 1);
    
    
}
