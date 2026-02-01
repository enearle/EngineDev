#include "ImageImport.h"
#include "stb_image.h"
#include <stdexcept>

ImageImport::ImageImport(const std::string& fileName, bool is16Bit)
{
    if (is16Bit)
        Data = LoadImage_16Bit(fileName);
    else
        Data = LoadImage(fileName);
}

ImageImport::ImageImport(const std::vector<std::string>& fileNames, bool forceNotEmpty)
{
    // Load image data
    Data = LoadImageSideBySide(fileNames, forceNotEmpty);
}

ImageImport::~ImageImport()
{
    delete Data.Pixels;
}

ImageData ImageImport::LoadImage(const std::string& imagePath) {
    ImageData result{};
    int width, height, channels;
    uint8_t bytesPerChannel = 1;

    std::string path = imagePath + ".png";
    
    result.Pixels = stbi_load(path.c_str(), &width, &height, &channels, 0);
    
    if (!result.Pixels) {
        throw std::runtime_error("Failed to load image at: " + path);
    }

    if (channels == 3)
    {
        stbi_uc* temp = new stbi_uc[width * height * 4];
        for (int i = 0; i < width * height; i++)
        {
            temp[i * 4] = static_cast<stbi_uc*>(result.Pixels)[i * 3];
            temp[i * 4 + 1] = static_cast<stbi_uc*>(result.Pixels)[i * 3 + 1];
            temp[i * 4 + 2] = static_cast<stbi_uc*>(result.Pixels)[i * 3 + 2];
            temp[i * 4 + 3] = 0;
        }
        delete static_cast<stbi_uc*>(result.Pixels);
        result.Pixels = temp;
        channels = 4;
    }
    
    result.Width = static_cast<uint32_t>(width);
    result.Height = static_cast<uint32_t>(height);
    result.Channels = static_cast<uint8_t>(channels);
    result.TotalSize = static_cast<VkDeviceSize>(result.Width * result.Height * result.Channels * bytesPerChannel);
    
    return result;
}

ImageData ImageImport::LoadImage_16Bit(const std::string& imagePath) {
    ImageData result{};
    int width, height, channels;
    uint8_t bytesPerChannel = 2;

    std::string path = imagePath + ".png";
    
    result.Pixels = stbi_load_16(path.c_str(), &width, &height, &channels, 0);
    
    if (!result.Pixels) {
        throw std::runtime_error("Failed to load image at: " + path);
    }

    if (channels == 3)
    {
        stbi_us* temp = new stbi_us[width * height * 4];
        for (int i = 0; i < width * height; i++)
        {
            temp[i * 4] = static_cast<stbi_us*>(result.Pixels)[i * 3];
            temp[i * 4 + 1] = static_cast<stbi_us*>(result.Pixels)[i * 3 + 1];
            temp[i * 4 + 2] = static_cast<stbi_us*>(result.Pixels)[i * 3 + 2];
            temp[i * 4 + 3] = 0;
        }
        delete static_cast<stbi_us*>(result.Pixels);
        result.Pixels = temp;
        channels = 4;
    }
    
    result.Width = static_cast<uint32_t>(width);
    result.Height = static_cast<uint32_t>(height);
    result.Channels = static_cast<uint8_t>(channels);
    result.TotalSize = static_cast<VkDeviceSize>(result.Width * result.Height * result.Channels * bytesPerChannel);
    
    return result;
}

ImageData ImageImport::LoadImageSideBySide(const std::vector<std::string>& fileNames, bool forceNotEmpty)
{
    ImageData result{};
    
    if (fileNames.empty() || fileNames.size() > 4)
        throw std::runtime_error("LoadImageSideBySide requires between 1 and 4 file names.");
    
    std::vector<stbi_uc*> loadedFiles(fileNames.size(), nullptr);
    std::vector<uint8_t> channelsPerImage(fileNames.size(), 0);
    std::vector<uint32_t> bytesPerChannel(fileNames.size(), 0);
    uint32_t totalChannels = 0;
    int baseWidth = 0, baseHeight = 0;
    
    for (size_t i = 0; i < fileNames.size(); ++i)
    {
        int width, height, channels;
        std::string path = fileNames[i] + ".png";
        
        loadedFiles[i] = stbi_load(path.c_str(), &width, &height, &channels, 0);
        
        if (!loadedFiles[i] && forceNotEmpty)
            throw std::runtime_error("Failed to load image at: " + path);
        
        if (loadedFiles[i])
        {
            channelsPerImage[i] = static_cast<uint8_t>(channels);
            totalChannels += channelsPerImage[i];

            if (baseWidth == 0)
            {
                baseWidth = width;
                baseHeight = height;
            }
            else if (loadedFiles[i] && (width != baseWidth || height != baseHeight))
            {
                throw std::runtime_error("All images must have the same dimensions.");
            }
        }
    }

    if (totalChannels > 4)
        throw std::runtime_error("All images' channels must total 4.");
    
    bool anyLoaded = false;
    for (const auto& file : loadedFiles)
        if (file != nullptr)
            anyLoaded = true;
    
    if (!anyLoaded)
        throw std::runtime_error("Failed to load any images from the provided file names.");
    
    for (size_t i = 0; i < loadedFiles.size(); ++i)
    {
        if (!loadedFiles[i])
        {
            loadedFiles[i] = new stbi_uc[baseWidth * baseHeight];
            std::fill(loadedFiles[i], loadedFiles[i] + baseWidth * baseHeight, 0);
        }
    }
    
    result.Pixels = new stbi_uc[baseWidth * baseHeight * totalChannels];
    
    for (int pixelIndex = 0; pixelIndex < baseWidth * baseHeight; ++pixelIndex)
    {
        uint32_t outputChannel = 0;
        for (size_t imageIndex = 0; imageIndex < loadedFiles.size(); ++imageIndex)
        {
            for (size_t channelIndex = 0; channelIndex < channelsPerImage[imageIndex]; ++channelIndex)
            {
                static_cast<stbi_uc*>(result.Pixels)[pixelIndex * totalChannels + outputChannel++] = 
                    loadedFiles[imageIndex][pixelIndex * channelsPerImage[imageIndex] + channelIndex];
            }
        }
        if (totalChannels == 3)
            static_cast<stbi_uc*>(result.Pixels)[pixelIndex * totalChannels + outputChannel] = 0;
    }
    
    for (size_t i = 0; i < loadedFiles.size(); ++i)
    {
        if (channelsPerImage[i] > 0)
            stbi_image_free(loadedFiles[i]);
        else
            delete[] loadedFiles[i];
    }
    
    result.Width = static_cast<uint32_t>(baseWidth);
    result.Height = static_cast<uint32_t>(baseHeight);
    result.Channels = totalChannels;
    result.TotalSize = static_cast<VkDeviceSize>(result.Width * result.Height * result.Channels);
    
    return result;
}