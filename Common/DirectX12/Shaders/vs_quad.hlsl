struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float2 txCoords : TEXCOORD;
};

VS_OUTPUT main(uint vertexID : SV_VertexID)
{
    VS_OUTPUT output;
    
    // Define triangle vertices (normalized device coordinates)
    // Top vertex - Red
    float3 positions[6] = {
        float3(-0.5f, 0.5f, 0.0f),      // Top left
        float3(-0.5f, -0.5f, 0.0f),    // Bottom left
        float3(0.5f, -0.5f, 0.0f),      // Bottom right
        float3(-0.5f, 0.5f, 0.0f),
        float3(0.5f, -0.5f, 0.0f),
        float3(0.5f, 0.5f, 0.0f)      // Top right
    };
    
    // TexCoords
    float2 uvs[6] =
    {
        float2(0.0f, 0.0f),
        float2(0.0f, 1.0f),
        float2(1.0f, 1.0f),

        float2(0.0f, 0.0f),
        float2(1.0f, 1.0f),
        float2(1.0f, 0.0f)
    };
    
    // Get vertex data based on vertex ID
    output.position = float4(positions[vertexID], 1.0f);
    output.txCoords = uvs[vertexID];
    
    return output;
}