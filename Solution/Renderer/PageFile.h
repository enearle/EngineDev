#pragma once
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "RENDERER_API_Macro.h"
#include "Paging.h"

// A paged texture on disk: a header, a directory of level offsets, then every tile of every level,
// coarsest level first and each level in rows. A tile is one page with its gutter: 136 x 136 raw
// R8G8B8A8 texels, the gutter holding the neighbouring texels of the same level, clamped at the
// texture's edge.
//
// Page files are built once into a folder git ignores, and rebuilt only when the source is newer.
class RENDERER_API PageFile
{
public:
    static constexpr uint32_t TileSize = Paging::SlotSize;
    static constexpr uint64_t TileBytes = static_cast<uint64_t>(TileSize) * TileSize * 4;

    struct Header
    {
        char Magic[4] = { 'P', 'A', 'G', 'E' };
        uint32_t PageSize = Paging::PageSize;
        uint32_t Gutter = Paging::Gutter;
        uint32_t Format = 0;
        uint32_t LevelCount = 0;
    };

    // Cuts any image stb_image loads, square with a power-of-two side of at least the page size,
    // into pages with box-filtered levels.
    static void Build(const std::string& imagePath, const std::string& pagePath);

    // A size x size pattern whose texels encode their level (red), position (green and blue), 8-texel
    // checks and white page borders. Written tile by tile; the image is never held in memory.
    static void BuildTestPattern(const std::string& pagePath, uint32_t size);
    static void WriteTestPatternTile(uint32_t level, uint32_t pageX, uint32_t pageY, uint8_t* tile);

    static uint64_t GetFileSize(uint32_t levelCount);
    static uint32_t CountPages(uint32_t levelCount);

    bool Open(const std::string& pagePath);
    void ReadTile(uint32_t level, uint32_t pageX, uint32_t pageY, uint8_t* tile);
    uint32_t GetLevelCount() const { return FileHeader.LevelCount; }

private:
    std::ifstream File;
    Header FileHeader;
    std::vector<uint64_t> LevelOffsets;
};

// Fills pages from a page file. One IO thread reads tiles into pooled buffers; Fill copies a tile
// into the frame ring and from there into the slot.
class RENDERER_API FilePageProducer : public PageProducer
{
public:
    explicit FilePageProducer(const std::string& pagePath);
    ~FilePageProducer() override;

    uint32_t GetLevelCount() const { return LevelCount; }

    void Begin(const PageKey& page) override;
    bool IsReady(const PageKey& page) override;
    void Fill(PipelineExecutor* executor, const PageKey& page, const PageSlot& slot) override;
    void Cancel(const PageKey& page) override;

private:
    struct PageKeyHash
    {
        size_t operator()(const PageKey& page) const;
    };

    std::string Path;
    uint32_t LevelCount = 0;
    std::thread Reader;
    std::mutex Lock;
    std::condition_variable Wake;
    bool IsStopping = false;
    std::deque<PageKey> Waiting;
    std::unordered_map<PageKey, std::unique_ptr<uint8_t[]>, PageKeyHash> Read;
    std::vector<std::unique_ptr<uint8_t[]>> FreeTiles;
    PageKey Reading = {};
    bool IsReading = false;
    bool IsReadingCancelled = false;

    void ReadTiles();
};
