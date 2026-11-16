#include "PageFile.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include "Image/stb_image.h"
#include "RHI/BufferAllocator.h"
#include "RHI/PipelineExecutor.h"

using namespace RHIStructures;

uint32_t PageFile::CountPages(uint32_t levelCount)
{
    uint32_t count = 0;
    for (uint32_t level = 0; level < levelCount; level++)
        count += 1u << (2 * level);
    return count;
}

uint64_t PageFile::GetFileSize(uint32_t levelCount)
{
    return sizeof(Header) + levelCount * sizeof(uint64_t) + CountPages(levelCount) * TileBytes;
}

static uint32_t LevelCountForSize(uint32_t size)
{
    uint32_t levelCount = 1;
    while ((Paging::PageSize << (levelCount - 1)) < size)
        levelCount++;
    return levelCount;
}

static bool IsExistingFileValid(const std::string& pagePath, uint32_t levelCount)
{
    return std::filesystem::exists(pagePath) && std::filesystem::file_size(pagePath) == PageFile::GetFileSize(levelCount);
}

// Writes the header and level directory, then asks writeTile for every tile, coarsest level first.
template <typename TileWriter>
static void WritePageFile(const std::string& pagePath, uint32_t levelCount, TileWriter writeTile)
{
    std::filesystem::create_directories(std::filesystem::path(pagePath).parent_path());
    std::string temporaryPath = pagePath + ".partial";
    std::ofstream file(temporaryPath, std::ios::binary | std::ios::trunc);
    if (!file)
        throw std::runtime_error("Cannot write the page file " + temporaryPath);

    PageFile::Header header;
    header.Format = static_cast<uint32_t>(Format::R8G8B8A8_UNORM);
    header.LevelCount = levelCount;
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    uint64_t offset = sizeof(header) + levelCount * sizeof(uint64_t);
    for (uint32_t level = 0; level < levelCount; level++)
    {
        file.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
        offset += (1ull << (2 * level)) * PageFile::TileBytes;
    }

    std::vector<uint8_t> tile(PageFile::TileBytes);
    for (uint32_t level = 0; level < levelCount; level++)
        for (uint32_t y = 0; y < (1u << level); y++)
            for (uint32_t x = 0; x < (1u << level); x++)
            {
                writeTile(level, x, y, tile.data());
                file.write(reinterpret_cast<const char*>(tile.data()), tile.size());
            }

    file.close();
    if (!file)
        throw std::runtime_error("Failed writing the page file " + temporaryPath);
    std::filesystem::rename(temporaryPath, pagePath);
}

void PageFile::WriteTestPatternTile(uint32_t level, uint32_t pageX, uint32_t pageY, uint8_t* tile)
{
    int64_t levelSize = static_cast<int64_t>(Paging::PageSize) << level;
    for (uint32_t j = 0; j < TileSize; j++)
    {
        int64_t ty = std::clamp<int64_t>(static_cast<int64_t>(pageY) * Paging::PageSize + j - Paging::Gutter, 0, levelSize - 1);
        for (uint32_t i = 0; i < TileSize; i++)
        {
            int64_t tx = std::clamp<int64_t>(static_cast<int64_t>(pageX) * Paging::PageSize + i - Paging::Gutter, 0, levelSize - 1);
            uint8_t* texel = tile + (static_cast<size_t>(j) * TileSize + i) * 4;
            bool isPageBorder = tx % Paging::PageSize == 0 || ty % Paging::PageSize == 0;
            bool isOddCheck = (((tx >> 3) ^ (ty >> 3)) & 1) != 0;
            texel[0] = isPageBorder ? 255 : static_cast<uint8_t>(level * 32 + (isOddCheck ? 24 : 0));
            texel[1] = isPageBorder ? 255 : static_cast<uint8_t>(tx * 256 / levelSize);
            texel[2] = isPageBorder ? 255 : static_cast<uint8_t>(ty * 256 / levelSize);
            texel[3] = 255;
        }
    }
}

void PageFile::BuildTestPattern(const std::string& pagePath, uint32_t size)
{
    uint32_t levelCount = LevelCountForSize(size);
    if (IsExistingFileValid(pagePath, levelCount))
        return;
    WritePageFile(pagePath, levelCount, WriteTestPatternTile);
}

void PageFile::Build(const std::string& imagePath, const std::string& pagePath)
{
    if (std::filesystem::exists(pagePath) && std::filesystem::last_write_time(pagePath) >= std::filesystem::last_write_time(imagePath))
        return;

    int width = 0, height = 0, channels = 0;
    stbi_uc* pixels = stbi_load(imagePath.c_str(), &width, &height, &channels, 4);
    if (!pixels)
        throw std::runtime_error("Cannot load " + imagePath + " to build a page file");
    uint32_t size = static_cast<uint32_t>(width);
    bool isPowerOfTwo = (size & (size - 1)) == 0;
    if (width != height || !isPowerOfTwo || size < Paging::PageSize)
    {
        stbi_image_free(pixels);
        throw std::runtime_error(imagePath + " must be square with a power-of-two side of at least " + std::to_string(Paging::PageSize));
    }

    // levels[0] is the full image, the finest level; each next one is a 2 x 2 box filter of it.
    uint32_t levelCount = LevelCountForSize(size);
    std::vector<std::vector<uint8_t>> levels(levelCount);
    levels[0].assign(pixels, pixels + static_cast<size_t>(size) * size * 4);
    stbi_image_free(pixels);
    for (uint32_t i = 1; i < levelCount; i++)
    {
        uint32_t side = size >> i;
        const std::vector<uint8_t>& finer = levels[i - 1];
        levels[i].resize(static_cast<size_t>(side) * side * 4);
        for (uint32_t y = 0; y < side; y++)
            for (uint32_t x = 0; x < side; x++)
                for (uint32_t c = 0; c < 4; c++)
                {
                    size_t finerRow = static_cast<size_t>(side) * 2 * 4;
                    size_t corner = (static_cast<size_t>(y) * 2 * finerRow) + static_cast<size_t>(x) * 2 * 4 + c;
                    uint32_t sum = finer[corner] + finer[corner + 4] + finer[corner + finerRow] + finer[corner + finerRow + 4];
                    levels[i][(static_cast<size_t>(y) * side + x) * 4 + c] = static_cast<uint8_t>((sum + 2) / 4);
                }
    }

    WritePageFile(pagePath, levelCount, [&](uint32_t level, uint32_t pageX, uint32_t pageY, uint8_t* tile)
    {
        const std::vector<uint8_t>& image = levels[levelCount - 1 - level];
        int64_t side = static_cast<int64_t>(Paging::PageSize) << level;
        for (uint32_t j = 0; j < TileSize; j++)
        {
            int64_t ty = std::clamp<int64_t>(static_cast<int64_t>(pageY) * Paging::PageSize + j - Paging::Gutter, 0, side - 1);
            for (uint32_t i = 0; i < TileSize; i++)
            {
                int64_t tx = std::clamp<int64_t>(static_cast<int64_t>(pageX) * Paging::PageSize + i - Paging::Gutter, 0, side - 1);
                std::memcpy(tile + (static_cast<size_t>(j) * TileSize + i) * 4, image.data() + (ty * side + tx) * 4, 4);
            }
        }
    });
}

bool PageFile::Open(const std::string& pagePath)
{
    File.open(pagePath, std::ios::binary);
    if (!File)
        return false;
    File.read(reinterpret_cast<char*>(&FileHeader), sizeof(FileHeader));
    if (!File || std::memcmp(FileHeader.Magic, "PAGE", 4) != 0 || FileHeader.PageSize != Paging::PageSize || FileHeader.Gutter != Paging::Gutter)
        return false;
    LevelOffsets.resize(FileHeader.LevelCount);
    File.read(reinterpret_cast<char*>(LevelOffsets.data()), LevelOffsets.size() * sizeof(uint64_t));
    return static_cast<bool>(File);
}

void PageFile::ReadTile(uint32_t level, uint32_t pageX, uint32_t pageY, uint8_t* tile)
{
    uint64_t offset = LevelOffsets[level] + (static_cast<uint64_t>(pageY) * (1ull << level) + pageX) * TileBytes;
    File.seekg(static_cast<std::streamoff>(offset));
    File.read(reinterpret_cast<char*>(tile), TileBytes);
    if (!File)
        throw std::runtime_error("Failed reading a tile from a page file");
}

size_t FilePageProducer::PageKeyHash::operator()(const PageKey& page) const
{
    return (static_cast<size_t>(page.Texture) << 56) ^ (static_cast<size_t>(page.Level) << 48) ^ (static_cast<size_t>(page.X) << 24) ^ page.Y;
}

FilePageProducer::FilePageProducer(const std::string& pagePath) : Path(pagePath)
{
    PageFile file;
    if (!file.Open(pagePath))
        throw std::runtime_error("Cannot open the page file " + pagePath);
    LevelCount = file.GetLevelCount();
    Reader = std::thread(&FilePageProducer::ReadTiles, this);
}

FilePageProducer::~FilePageProducer()
{
    {
        std::lock_guard<std::mutex> guard(Lock);
        IsStopping = true;
    }
    Wake.notify_all();
    Reader.join();
}

void FilePageProducer::ReadTiles()
{
    PageFile file;
    file.Open(Path);
    std::unique_lock<std::mutex> lock(Lock);
    while (true)
    {
        Wake.wait(lock, [this] { return IsStopping || !Waiting.empty(); });
        if (IsStopping)
            return;

        PageKey page = Waiting.front();
        Waiting.pop_front();
        std::unique_ptr<uint8_t[]> tile;
        if (!FreeTiles.empty())
        {
            tile = std::move(FreeTiles.back());
            FreeTiles.pop_back();
        }
        else
            tile = std::make_unique<uint8_t[]>(PageFile::TileBytes);
        Reading = page;
        IsReading = true;
        IsReadingCancelled = false;

        lock.unlock();
        file.ReadTile(page.Level, page.X, page.Y, tile.get());
        lock.lock();

        IsReading = false;
        if (IsReadingCancelled)
            FreeTiles.push_back(std::move(tile));
        else
            Read[page] = std::move(tile);
    }
}

void FilePageProducer::Begin(const PageKey& page)
{
    {
        std::lock_guard<std::mutex> guard(Lock);
        Waiting.push_back(page);
    }
    Wake.notify_one();
}

bool FilePageProducer::IsReady(const PageKey& page)
{
    std::lock_guard<std::mutex> guard(Lock);
    return Read.contains(page);
}

void FilePageProducer::Fill(PipelineExecutor* executor, const PageKey& page, const PageSlot& slot)
{
    std::unique_ptr<uint8_t[]> tile;
    {
        std::lock_guard<std::mutex> guard(Lock);
        auto found = Read.find(page);
        if (found == Read.end())
            return;
        tile = std::move(found->second);
        Read.erase(found);
    }

    uint32_t rowPitch = CopyRowPitch(Format::R8G8B8A8_UNORM, PageFile::TileSize);
    FrameAllocation staging = BufferAllocator::GetInstance()->AllocateFrameData(static_cast<uint64_t>(rowPitch) * PageFile::TileSize, CopyPlacementAlignment);
    for (uint32_t row = 0; row < PageFile::TileSize; row++)
        std::memcpy(static_cast<uint8_t*>(staging.Address) + static_cast<size_t>(row) * rowPitch,
                    tile.get() + static_cast<size_t>(row) * PageFile::TileSize * 4, PageFile::TileSize * 4);
    executor->CopyBufferToImage(staging.Buffer, staging.Offset, rowPitch, slot.Images[0],
                                { 0, 0, slot.X, slot.Y, PageFile::TileSize, PageFile::TileSize });

    std::lock_guard<std::mutex> guard(Lock);
    FreeTiles.push_back(std::move(tile));
}

void FilePageProducer::Cancel(const PageKey& page)
{
    std::lock_guard<std::mutex> guard(Lock);
    std::erase(Waiting, page);
    auto found = Read.find(page);
    if (found != Read.end())
    {
        FreeTiles.push_back(std::move(found->second));
        Read.erase(found);
    }
    if (IsReading && Reading == page)
        IsReadingCancelled = true;
}
