// Paged textures: the only code that knows the page table's form. The table is a hash of 16-byte
// entries in the frame ring (x, y, texture << 8 | level, slot), each page within ProbeSteps of its
// hash; see Renderer/Paging.h.
#ifndef PAGING_HLSLI
#define PAGING_HLSLI

#define PAGE_SIZE 128
#define PAGE_GUTTER 4
#define PAGE_SLOT_SIZE 136
#define PAGING_PROBE_STEPS 4
#define PAGING_LEVELS_PROBED 4
#define PAGING_EMPTY 0xFFFFFFFF
#define PAGING_NO_LEVEL 0xFFFFFFFF

// Mirrors PagingConstants in Renderer/Paging.cpp.
cbuffer PagingConstants : register(b0, space3)
{
    uint PagingTableBase;
    uint PagingTableMask;
    uint PagingUserBase;
    uint PagingSlotsPerSide;
    float2 PagingAtlasTexelSize;
    uint2 PagingFeedbackSize;
    uint2 PagingJitter;
    uint PagingIsTinted;
    uint PagingTexturesBase;
    uint PagingLevelBias;           // Lookups start this many levels coarser than wanted
    uint3 PagingPadding;
};

// The frame ring: the table, the user words and the per-texture entries all live in it.
StructuredBuffer<uint4> PageTable : register(t1, space3);
RWStructuredBuffer<uint2> PagingFeedback : register(u2, space3);
Texture2D PagingAtlas0 : register(t3, space3);      // Colour or pigment fields
Texture2D PagingAtlas1 : register(t4, space3);      // Height, slope along U, slope along V
SamplerState PagingAtlasSampler : register(s2, space0);

// Mirrors PagedTextureDesc in Renderer/Paging.h: two entries per texture.
struct PagedTextureInfo
{
    uint LevelCount;
    float2 Millimetres;
    bool IsPeriodic;
    uint BandTable;
    uint ChartMapLayer;
};

PagedTextureInfo LoadPagedTexture(uint texture)
{
    uint4 first = PageTable[PagingTexturesBase + texture * 2];
    uint4 second = PageTable[PagingTexturesBase + texture * 2 + 1];
    PagedTextureInfo info;
    info.LevelCount = first.x;
    info.Millimetres = asfloat(first.yz);
    info.IsPeriodic = first.w != 0;
    info.BandTable = second.x;
    info.ChartMapLayer = second.y;
    return info;
}

// Mirrors HashPage in Renderer/Paging.cpp.
uint HashPage(uint texture, uint level, uint x, uint y)
{
    uint hash = texture * 0x9E3779B1u ^ level * 0x85EBCA77u;
    hash ^= x * 0xC2B2AE3Du;
    hash = (hash ^ (hash >> 15)) * 0x27D4EB2Fu;
    hash ^= y * 0x165667B1u;
    hash = (hash ^ (hash >> 13)) * 0x9E3779B1u;
    return hash ^ (hash >> 16);
}

bool FindPage(uint texture, uint level, uint2 page, out uint slot)
{
    uint key = (texture << 8) | level;
    uint index = HashPage(texture, level, page.x, page.y);
    bool isFound = false;
    slot = 0;
    [unroll]
    for (uint step = 0; step < PAGING_PROBE_STEPS; step++)
    {
        uint4 entry = PageTable[PagingTableBase + ((index + step) & PagingTableMask)];
        if (!isFound && entry.z == key && entry.x == page.x && entry.y == page.y)
        {
            slot = entry.w;
            isFound = true;
        }
    }
    return isFound;
}

uint2 GetPageUserWords(uint slot)
{
    return PageTable[PagingUserBase + slot].xy;
}

uint2 PageAt(float2 uv, uint level)
{
    uint pagesPerSide = 1u << level;
    return min(uint2(uv * pagesPerSide), pagesPerSide - 1);
}

// Where a texture's texels for this pixel are: the wanted level, whose texels cover one to two
// pixels, or the finest resident page among it and its parents, PAGING_LEVELS_PROBED levels in all,
// falling back to level 0, which is pinned.
struct PageResolve
{
    bool IsFound;
    uint Slot;
    uint Level;             // The level found, or PAGING_NO_LEVEL
    float2 AtlasUV;
    float2 WithinPage;      // 0 to 1 across the found page
    uint2 Page;             // The found page
    uint WantedLevel;
    uint2 WantedPage;
};

PageResolve ResolvePage(uint texture, float2 uv, float2 uvDdx, float2 uvDdy)
{
    uint levelCount = LoadPagedTexture(texture).LevelCount;
    uv = clamp(uv, 0.0, 0.99999994);
    float levelZeroTexelsPerPixel = max(length(uvDdx), length(uvDdy)) * PAGE_SIZE;
    float wanted = floor(-log2(max(levelZeroTexelsPerPixel, 1e-20)));

    PageResolve result;
    result.WantedLevel = (uint)clamp(wanted, 0.0, float(levelCount - 1));
    result.WantedPage = PageAt(uv, result.WantedLevel);
    result.Level = PAGING_NO_LEVEL;
    result.IsFound = false;
    result.Slot = 0;
    result.AtlasUV = 0.0;
    result.WithinPage = 0.0;
    result.Page = 0;

    uint level = result.WantedLevel - min(PagingLevelBias, result.WantedLevel);
    [unroll]
    for (uint probe = 0; probe < PAGING_LEVELS_PROBED; probe++)
    {
        uint probedLevel = level - min(probe, level);
        uint probedSlot = 0;
        if (!result.IsFound && FindPage(texture, probedLevel, PageAt(uv, probedLevel), probedSlot))
        {
            result.Slot = probedSlot;
            result.Level = probedLevel;
            result.IsFound = true;
        }
    }
    if (!result.IsFound && FindPage(texture, 0, uint2(0, 0), result.Slot))
    {
        result.Level = 0;
        result.IsFound = true;
    }
    if (!result.IsFound)
        return result;

    result.Page = PageAt(uv, result.Level);
    result.WithinPage = uv * float(1u << result.Level) - float2(result.Page);
    float2 slotOrigin = float2(result.Slot % PagingSlotsPerSide, result.Slot / PagingSlotsPerSide) * PAGE_SLOT_SIZE;
    result.AtlasUV = (slotOrigin + PAGE_GUTTER + result.WithinPage * PAGE_SIZE) * PagingAtlasTexelSize;
    return result;
}

float4 SamplePagingAtlas0(PageResolve resolved)
{
    return resolved.IsFound ? PagingAtlas0.SampleLevel(PagingAtlasSampler, resolved.AtlasUV, 0) : float4(0.5, 0.5, 0.5, 1.0);
}

float4 SamplePagingAtlas1(PageResolve resolved)
{
    return resolved.IsFound ? PagingAtlas1.SampleLevel(PagingAtlasSampler, resolved.AtlasUV, 0) : float4(0.0, 0.0, 0.0, 0.0);
}

struct PagedSample
{
    float4 Colour;
    uint Level;
    uint WantedLevel;
    uint2 WantedPage;
};

// Resolve and one fetch of atlas image 0.
PagedSample SamplePagedTexture(uint texture, float2 uv, float2 uvDdx, float2 uvDdy)
{
    PageResolve resolved = ResolvePage(texture, uv, uvDdx, uvDdy);
    PagedSample result;
    result.Colour = SamplePagingAtlas0(resolved);
    result.Level = resolved.Level;
    result.WantedLevel = resolved.WantedLevel;
    result.WantedPage = resolved.WantedPage;
    return result;
}

// One pixel of each 8 x 8 block, chosen by this frame's jitter, records the page it wants: level
// and x, then texture and y, keeping 24 bits of each coordinate.
void WritePagingFeedback(float4 pixelPosition, uint texture, uint level, uint2 page)
{
    uint2 pixel = uint2(pixelPosition.xy);
    if (any((pixel & 7) != PagingJitter))
        return;
    uint2 block = pixel >> 3;
    if (block.x >= PagingFeedbackSize.x || block.y >= PagingFeedbackSize.y)
        return;
    PagingFeedback[block.y * PagingFeedbackSize.x + block.x] = uint2((level << 24) | (page.x & 0xFFFFFF), (texture << 24) | (page.y & 0xFFFFFF));
}

float3 PagingLevelTint(uint level)
{
    static const float3 Tints[8] = {
        float3(1.0, 0.0, 0.0), float3(1.0, 0.5, 0.0), float3(1.0, 1.0, 0.0), float3(0.0, 1.0, 0.0),
        float3(0.0, 1.0, 1.0), float3(0.0, 0.0, 1.0), float3(0.5, 0.0, 1.0), float3(1.0, 0.0, 1.0),
    };
    return level == PAGING_NO_LEVEL ? float3(0.0, 0.0, 0.0) : Tints[level % 8];
}

#endif
