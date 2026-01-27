struct VS_INPUT
{
    uint vertexID : SV_VertexID;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    
    // Define triangle vertices (normalized device coordinates)
    // Top vertex - Red
    float3 positions[3] = {
        float3(0.0f, 0.5f, 0.0f),      // Top
        float3(-0.5f, -0.5f, 0.0f),    // Bottom left
        float3(0.5f, -0.5f, 0.0f)      // Bottom right
    };
    
    // Define colors for each vertex (RGB)
    float4 colors[3] = {
        float4(1.0f, 0.0f, 0.0f, 1.0f),    // Red
        float4(0.0f, 1.0f, 0.0f, 1.0f),    // Green
        float4(0.0f, 0.0f, 1.0f, 1.0f)     // Blue
    };
    
    // Get vertex data based on vertex ID
    float3 position = positions[input.vertexID];
    float4 color = colors[input.vertexID];
    
    // Output position in clip space (already in NDC, so just convert to homogeneous)
    output.position = float4(position, 1.0f);
    output.color = color;
    
    return output;
}