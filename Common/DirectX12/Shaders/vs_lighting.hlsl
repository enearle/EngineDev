struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

static float3 positions[6] = 
{
    float3(-1.0f, 1.0f, 0.0f),
    float3(-1.0f, -1.0f, 0.0f),
    float3(1.0f, -1.0f, 0.0f),
    float3(-1.0f, 1.0f, 0.0f),
    float3(1.0f, -1.0f, 0.0f),
    float3(1.0f, 1.0f, 0.0f)
};

VSOutput main(uint vertexID : SV_VertexID)
{
    VSOutput output;
    
    output.position = float4(positions[vertexID], 1.0f);
    output.uv = (positions[vertexID] * float2(1, -1) + float2(1,1)) * 0.5f;
    
    return output;
}