// The sRGB transfer curve. Textures and the G-buffer albedo hold encoded colour; lighting works on
// linear colour.
#ifndef COLOUR_HLSLI
#define COLOUR_HLSLI

float3 DecodeSrgb(float3 encoded)
{
    float3 isLow = step(encoded, 0.04045);
    return lerp(pow((encoded + 0.055) / 1.055, 2.4), encoded / 12.92, isLow);
}

float3 EncodeSrgb(float3 linearColour)
{
    linearColour = saturate(linearColour);
    float3 isLow = step(linearColour, 0.0031308);
    return lerp(1.055 * pow(linearColour, 1.0 / 2.4) - 0.055, linearColour * 12.92, isLow);
}

#endif
