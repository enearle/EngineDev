#pragma once
#include "../ENGINE_API_Macro.h"
#include <cstdint>
#include <string>

struct alignas(16) ENGINE_API AssetID {
    uint64_t first;
    uint64_t second;

    // Equality operator
    bool operator==(const AssetID& other) const;
    
    // Less-than operator (allows use in std::map and std::set)
    bool operator<(const AssetID& other) const;

    // Helper to generate a random AssetUUID (Version 4)
    static AssetID Generate();

    // Convert to standard 36-character AssetUUID string
    std::string to_string() const;
};
