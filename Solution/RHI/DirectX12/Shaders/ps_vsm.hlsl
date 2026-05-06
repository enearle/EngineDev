static float2 depth;
static float inDepth;

struct SPIRV_Cross_Input
{
    float inDepth : TEXCOORD0;
};

struct SPIRV_Cross_Output
{
    float2 depth : SV_Target0;
};

void frag_main()
{
    depth.x = inDepth;
    depth.y = depth.x * depth.x;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    inDepth = stage_input.inDepth;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.depth = depth;
    return stage_output;
}
