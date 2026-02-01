#pragma once
#include <string>
#include <vector>
#include "../../Windows/WindowsHeaders.h"



struct ImageData {
    bool Is16Bit = false;
    void* Pixels;
    uint32_t Width;
    uint32_t Height;
    uint8_t Channels;
    uint64_t TotalSize;
};

class ImageImport
{
    ImageData Data;

public:
    ImageImport(const std::string& fileName, bool is16Bit = false);
    ImageImport(const std::vector<std::string>& fileNames, bool forceNotEmpty = true);

    ~ImageImport();

    const ImageData& GetTextureData()       const { return Data; }
    const uint32_t GetWidth()               const { return Data.Width; }
    const uint32_t GetHeight()              const { return Data.Height; }
    const uint8_t GetChannels()             const { return Data.Channels; }
    const VkDeviceSize GetTotalSize()       const { return Data.TotalSize; }
    const void GetPixels(void*& outPixels, bool& outIs16Bit) const { outPixels = Data.Pixels; outIs16Bit = Data.Is16Bit; }
    
    static ImageData LoadImage(const std::string& imagePath);
    static ImageData LoadImage_16Bit(const std::string& imagePath);
    static ImageData LoadImageSideBySide(const std::vector<std::string>& fileNames, bool forceNotEmpty = true);
};
