#pragma once
#include <string>
#include <vector>
#include "../../Windows/WindowsHeaders.h"
#include "../../RHI_API_Macro.h"

struct RHI_API ImageData 
{
    bool Is16Bit = false;
    void* Pixels;
    uint32_t Width;
    uint32_t Height;
    uint8_t Channels;
    uint64_t TotalSize;
};

class RHI_API ImageImport
{
    ImageData* Data;

public:
    ImageImport(const std::string& fileName, bool is16Bit = false, bool forceNotEmpty = true);
    ImageImport(const std::vector<std::string>& fileNames, std::vector<std::vector<uint8_t>> imageChannelDefaults = {{0}, {0}, {0}, {0}});

    ~ImageImport();

    const ImageData* GetData()       const { return Data; }
    
    static ImageData* LoadImage_8Bit(const std::string& imagePath, bool forceNotEmpty);
    static ImageData* LoadImage_16Bit(const std::string& imagePath, bool forceNotEmpty);
    static ImageData* LoadImageSideBySide(const std::vector<std::string>& fileNames, std::vector<std::vector<uint8_t>> imageChannelDefaults);
};
