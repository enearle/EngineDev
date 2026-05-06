struct Light
{
    float3 Position;
    float3 Colour;
    float Intensity;
    float Radius;
    uint Type;
    float Angle;
    uint ShadowIndex;
};

cbuffer VPData : register(b0, space0)
{
    row_major float4x4 vpData_viewProjection : packoffset(c0);
    float4 vpData_cameraPosition : packoffset(c4);
};

cbuffer LightMatrices : register(b0, space4)
{
    row_major float4x4 lightMatrices_lightVP[4] : packoffset(c0);
};

cbuffer LightData : register(b0, space3)
{
    Light lightData_Lights[4] : packoffset(c0);
};

Texture2D<float4> subAlbedo : register(t0, space2);
SamplerState _subAlbedo_sampler : register(s0, space2);
Texture2D<float4> subNormal : register(t1, space2);
SamplerState _subNormal_sampler : register(s1, space2);
Texture2D<float4> subMetalicRoughnessAO : register(t2, space2);
SamplerState _subMetalicRoughnessAO_sampler : register(s2, space2);
Texture2D<float4> subPosition : register(t3, space2);
SamplerState _subPosition_sampler : register(s3, space2);
Texture2DArray<float4> shadowMaps : register(t0, space1);
SamplerState _shadowMaps_sampler : register(s0, space1);

static float2 inUV;
static float4 outColour;

struct SPIRV_Cross_Input
{
    float2 inUV : TEXCOORD0;
};

struct SPIRV_Cross_Output
{
    float4 outColour : SV_Target0;
};

static float3 albedo;
static float3 normal;
static float3 materialData;
static float3 fragPosition;
static float metallic;
static float roughness;
static float ambientOcclusion;
static float3 viewVector;

float Shadow(Light light, float3 lightDirection)
{
    float4 _273 = mul(float4(fragPosition, 1.0f), lightMatrices_lightVP[light.ShadowIndex]);
    float3 _277 = _273.xyz / _273.w.xxx;
    _277.y = -_277.y;
    float2 _284 = (_277.xy * 0.5f) + 0.5f.xx;
    bool _286 = _284.x < 0.0f;
    bool _292;
    if (!_286)
    {
        _292 = _284.x > 1.0f;
    }
    else
    {
        _292 = _286;
    }
    bool _298;
    if (!_292)
    {
        _298 = _284.y < 0.0f;
    }
    else
    {
        _298 = _292;
    }
    bool _304;
    if (!_298)
    {
        _304 = _284.y > 1.0f;
    }
    else
    {
        _304 = _298;
    }
    if (_304)
    {
        return 1.0f;
    }
    float4 _314 = shadowMaps.Sample(_shadowMaps_sampler, float3(_284, float(light.ShadowIndex)));
    float2 _315 = _314.xy;
    float _320 = abs(_273.w) / light.Radius;
    float _321 = _315.x;
    float _325 = max(_315.y - (_321 * _321), 9.9999997473787516355514526367188e-05f);
    if (_320 <= _321)
    {
        return 1.0f;
    }
    return _325 / (_325 + ((_320 - _321) * (_320 - _321)));
}

void frag_main()
{
    albedo = subAlbedo.Sample(_subAlbedo_sampler, inUV).xyz;
    normal = subNormal.Sample(_subNormal_sampler, inUV).xyz;
    materialData = subMetalicRoughnessAO.Sample(_subMetalicRoughnessAO_sampler, inUV).xyz;
    fragPosition = subPosition.Sample(_subPosition_sampler, inUV).xyz;
    metallic = materialData.x;
    roughness = max(materialData.y, 0.039999999105930328369140625f);
    roughness = max(roughness * roughness, 0.001000000047497451305389404296875f);
    ambientOcclusion = materialData.z;
    viewVector = normalize(vpData_cameraPosition.xyz - fragPosition);
    float3 outGoingLight = 0.0f.xxx;
    Light param;
    for (uint i = 0u; i < 4u; i++)
    {
        if (lightData_Lights[i].Type == 1u)
        {
            Light _139 = param;
            _139.Position = lightData_Lights[i].Position;
            _139.Colour = lightData_Lights[i].Colour;
            _139.Intensity = lightData_Lights[i].Intensity;
            _139.Radius = lightData_Lights[i].Radius;
            _139.Type = lightData_Lights[i].Type;
            _139.Angle = lightData_Lights[i].Angle;
            _139.ShadowIndex = lightData_Lights[i].ShadowIndex;
            param = _139;
            float _160 = 1.0f - clamp(length(_139.Position - fragPosition) / _139.Radius, 0.0f, 1.0f);
            float3 _169 = normalize(_139.Position - fragPosition);
            float3 _172 = normalize(_169 + viewVector);
            float _176 = 1.0f - max(dot(viewVector, _172), 0.0f);
            float3 _184 = lerp(0.039999999105930328369140625f.xxx, albedo, metallic.xxx);
            float3 _187 = _184 + ((1.0f.xxx - _184) * ((((_176 * _176) * _176) * _176) * _176));
            float _193 = roughness * roughness;
            float _196 = max(dot(normal, _172), 9.9999997473787516355514526367188e-05f);
            float _200 = ((_196 * _196) * (_193 - 1.0f)) + 1.0f;
            float _208 = max(dot(normal, viewVector), 9.9999997473787516355514526367188e-05f);
            float _210 = roughness * 0.5f;
            float _218 = max(dot(normal, _169), 9.9999997473787516355514526367188e-05f);
            float _220 = roughness * 0.5f;
            Light _87 = _139;
            float3 _88 = _169;
            outGoingLight += (((((((1.0f.xxx - _187) * (1.0f - metallic)) * (albedo / 3.1415927410125732421875f.xxx)) + ((_187 * ((_193 / max((_200 * _200) * 3.1415927410125732421875f, 9.9999997473787516355514526367188e-05f)) * ((_208 / max((_208 * (1.0f - _210)) + _210, 9.9999997473787516355514526367188e-05f)) * (_218 / max((_218 * (1.0f - _220)) + _220, 9.9999997473787516355514526367188e-05f))))) / max((4.0f * max(dot(viewVector, normal), 9.9999997473787516355514526367188e-05f)) * max(dot(_169, normal), 9.9999997473787516355514526367188e-05f), 9.9999997473787516355514526367188e-05f).xxx)) * (_139.Colour * ((_160 * _160) * _139.Intensity))) * max(dot(_169, normal), 9.9999997473787516355514526367188e-05f)) * Shadow(_87, _88));
        }
    }
    outColour = float4(outGoingLight, 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    inUV = stage_input.inUV;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.outColour = outColour;
    return stage_output;
}
