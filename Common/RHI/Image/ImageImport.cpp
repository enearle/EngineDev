#define STB_IMAGE_IMPLEMENTATION
#include "ImageImport.h"
#include "stb_image.h"
#include <stdexcept>

ImageImport::ImageImport(const std::string& fileName, bool is16Bit, bool forceNotEmpty)
{
    if (is16Bit)
        Data = LoadImage_16Bit(fileName, forceNotEmpty);
    else
        Data = LoadImage_8Bit(fileName, forceNotEmpty);
}

ImageImport::ImageImport(const std::vector<std::string>& fileNames, std::vector<std::vector<uint8_t>> imagecChannelDefaults)
{
    // Load image data
    Data = LoadImageSideBySide(fileNames, imagecChannelDefaults);
}

ImageImport::~ImageImport()
{
    if (Data->Pixels)
    {
        if (Data->Is16Bit)
            stbi_image_free(Data->Pixels);
        else
            stbi_image_free(Data->Pixels);
    }
    delete Data;
}

ImageData* ImageImport::LoadImage_8Bit(const std::string& imagePath, bool forceNotEmpty) {
    ImageData* result = new ImageData{};
    int width, height, channels;
    uint8_t bytesPerChannel = 1;

    std::string path = imagePath + ".png";
    
    result->Pixels = stbi_load(path.c_str(), &width, &height, &channels, 0);
    
    if (!result->Pixels && forceNotEmpty) {
        throw std::runtime_error("Failed to load image at: " + path);
    }
    if (!result->Pixels) return nullptr;

    if (channels == 3)
    {
        stbi_uc* temp = new stbi_uc[width * height * 4];
        for (int i = 0; i < width * height; i++)
        {
            temp[i * 4] = static_cast<stbi_uc*>(result->Pixels)[i * 3];
            temp[i * 4 + 1] = static_cast<stbi_uc*>(result->Pixels)[i * 3 + 1];
            temp[i * 4 + 2] = static_cast<stbi_uc*>(result->Pixels)[i * 3 + 2];
            temp[i * 4 + 3] = 0;
        }
        delete static_cast<stbi_uc*>(result->Pixels);
        result->Pixels = temp;
        channels = 4;
    }
    
    result->Width = static_cast<uint32_t>(width);
    result->Height = static_cast<uint32_t>(height);
    result->Channels = static_cast<uint8_t>(channels);
    result->TotalSize = static_cast<VkDeviceSize>(result->Width * result->Height * result->Channels * bytesPerChannel);
    
    return result;
}

ImageData* ImageImport::LoadImage_16Bit(const std::string& imagePath, bool forceNotEmpty) {
    ImageData* result = new ImageData{};
    int width, height, channels;
    uint8_t bytesPerChannel = 2;

    std::string path = imagePath + ".png";
    
    result->Pixels = stbi_load_16(path.c_str(), &width, &height, &channels, 0);
    
    if (!result->Pixels && forceNotEmpty) {
        throw std::runtime_error("Failed to load image at: " + path);
    }
    if (!result->Pixels) return nullptr;

    if (channels == 3)
    {
        stbi_us* temp = new stbi_us[width * height * 4];
        for (int i = 0; i < width * height; i++)
        {
            temp[i * 4] = static_cast<stbi_us*>(result->Pixels)[i * 3];
            temp[i * 4 + 1] = static_cast<stbi_us*>(result->Pixels)[i * 3 + 1];
            temp[i * 4 + 2] = static_cast<stbi_us*>(result->Pixels)[i * 3 + 2];
            temp[i * 4 + 3] = 0;
        }
        delete static_cast<stbi_us*>(result->Pixels);
        result->Pixels = temp;
        channels = 4;
    }
    
    result->Width = static_cast<uint32_t>(width);
    result->Height = static_cast<uint32_t>(height);
    result->Channels = static_cast<uint8_t>(channels);
    result->TotalSize = static_cast<VkDeviceSize>(result->Width * result->Height * result->Channels * bytesPerChannel);
    
    return result;
}

ImageData* ImageImport::LoadImageSideBySide(const std::vector<std::string>& fileNames, std::vector<std::vector<uint8_t>> imagecChannelDefaults)
{
    ImageData* result = new ImageData{};

    if (fileNames.empty() || fileNames.size() > 4)
        throw std::runtime_error("LoadImageSideBySide requires between 1 and 4 file names.");

    if (imagecChannelDefaults.size() != fileNames.size())
        throw std::runtime_error("LoadImageSideBySide requires a channel default for each image.");

    std::vector<stbi_uc*> loadedFiles(fileNames.size(), nullptr);
    std::vector<bool> loadedFromStbi(fileNames.size(), false);
    std::vector<uint32_t> channelsPerImage(fileNames.size(), 0);

    uint32_t totalChannels = 0;
    int baseWidth = 0, baseHeight = 0;

    for (size_t i = 0; i < fileNames.size(); ++i)
    {
        int width = 0, height = 0, channels = 0;
        std::string path = fileNames[i] + ".png";

        loadedFiles[i] = stbi_load(path.c_str(), &width, &height, &channels, 0);

        if (loadedFiles[i])
        {
            loadedFromStbi[i] = true;

            if (baseWidth == 0)
            {
                baseWidth = width;
                baseHeight = height;
            }
            else if (width != baseWidth || height != baseHeight)
            {
                throw std::runtime_error("All images must have the same dimensions.");
            }

            channelsPerImage[i] = static_cast<uint32_t>(channels);
            
            if (imagecChannelDefaults[i].size() != channelsPerImage[i])
            {
                throw std::runtime_error(
                    "channelDefaults[" + std::to_string(i) + "] size must match loaded image channels.");
            }
        }
        else
        {
            channelsPerImage[i] = static_cast<uint32_t>(imagecChannelDefaults[i].size());
        }

        totalChannels += channelsPerImage[i];
        if (totalChannels > 4)
            throw std::runtime_error("All images' channels must total 4 or less.");
    }

    bool anyLoaded = false;
    for (const auto* file : loadedFiles)
        if (file != nullptr)
            anyLoaded = true;

    if (!anyLoaded)
        throw std::runtime_error("Failed to load any images from the provided file names.");

    if (baseWidth == 0 || baseHeight == 0)
        throw std::runtime_error("Loaded images had invalid dimensions.");
    
    if (totalChannels == 0)
        throw std::runtime_error("Total output channels is 0. Provide defaults for missing images or load at least one channel.");

    if (totalChannels == 3)
        result->Pixels = new stbi_uc[static_cast<size_t>(baseWidth) * static_cast<size_t>(baseHeight) * static_cast<size_t>(totalChannels + 1)];
    else
        result->Pixels = new stbi_uc[static_cast<size_t>(baseWidth) * static_cast<size_t>(baseHeight) * static_cast<size_t>(totalChannels)];

    for (int pixelIndex = 0; pixelIndex < baseWidth * baseHeight; ++pixelIndex)
    {
        uint32_t outputChannel = 0;

        for (size_t imageIndex = 0; imageIndex < loadedFiles.size(); ++imageIndex)
        {
            const uint32_t nCh = channelsPerImage[imageIndex];
            if (nCh == 0)
                continue;

            if (loadedFiles[imageIndex])
            {
                for (uint32_t channelIndex = 0; channelIndex < nCh; ++channelIndex)
                {
                    static_cast<stbi_uc*>(result->Pixels)[pixelIndex * totalChannels + outputChannel++] =
                        loadedFiles[imageIndex][pixelIndex * nCh + channelIndex];
                }
            }
            else
            {
                for (uint32_t channelIndex = 0; channelIndex < nCh; ++channelIndex)
                {
                    const uint8_t fallback =
                        (channelIndex < imagecChannelDefaults[imageIndex].size()) ? imagecChannelDefaults[imageIndex][channelIndex] : 0;

                    static_cast<stbi_uc*>(result->Pixels)[pixelIndex * totalChannels + outputChannel++] =
                        static_cast<stbi_uc>(fallback);
                }
            }
        }

        if (totalChannels == 3)
            static_cast<stbi_uc*>(result->Pixels)[pixelIndex * totalChannels + outputChannel] = 0;
    }

    for (size_t i = 0; i < loadedFiles.size(); ++i)
    {
        if (loadedFromStbi[i] && loadedFiles[i])
            stbi_image_free(loadedFiles[i]);
    }

    result->Width = static_cast<uint32_t>(baseWidth);
    result->Height = static_cast<uint32_t>(baseHeight);
    result->Channels = static_cast<uint8_t>(totalChannels);
    result->TotalSize = static_cast<VkDeviceSize>(result->Width * result->Height * result->Channels);

    return result;
}