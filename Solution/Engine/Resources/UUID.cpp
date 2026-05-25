#include "UUID.h"
#include <iomanip>
#include <sstream>
#include <random>
bool AssetID::operator==(const AssetID& other) const
{
    return first == other.first && second == other.second;
}

bool AssetID::operator<(const AssetID& other) const
{
    if (first != other.first) return first < other.first;
    return second < other.second;
}

AssetID AssetID::Generate()
{
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dist;

    AssetID id;
    id.first = dist(gen);
    id.second = dist(gen);

    // Apply RFC 4122 version 4 and variant 1 bits
    id.first = (id.first & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    id.second = (id.second & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    return id;
}

std::string AssetID::to_string() const
{
    std::stringstream ss;
    uint64_t part1 = (first >> 32) & 0xFFFFFFFF;
    uint64_t part2 = (first >> 16) & 0xFFFF;
    uint64_t part3 = first & 0xFFFF;
    uint64_t part4 = (second >> 48) & 0xFFFF;
    uint64_t part5 = second & 0xFFFFFFFFFFFF;

    ss << std::hex << std::setfill('0')
       << std::setw(8) << part1 << "-"
       << std::setw(4) << part2 << "-"
       << std::setw(4) << part3 << "-"
       << std::setw(4) << part4 << "-"
       << std::setw(12) << part5;

    return ss.str();
}

