#include "Common.hlsli"

struct VSOutput
{
    float4 position : SV_Position;
    float depth : TEXCOORD0;
};

VSOutput main(float3 inPosition : POSITION, uint instanceID : SV_InstanceID, uint viewID : SV_ViewID)
{
    InstanceData instance = LoadInstance(instanceID);
    float4 worldPosition = mul(instance.World, float4(inPosition, 1.0));

    VSOutput output;
    output.position = mul(LightViewProjection[viewID], worldPosition);
    output.depth = output.position.w / Lights[viewID].Radius;
    return output;
}
