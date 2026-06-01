#pragma once
#include "../ENGINE_API_Macro.h"
#include <cstdint>
#include <string>

struct alignas(16) ENGINE_API AssetID {
    uint64_t first = 0;
    uint64_t second = 0;
    
    bool IsValid() const;
    
    void Clear();

    // Equality operator
    bool operator==(const AssetID& other) const;
    
    // Less-than operator (allows use in std::map and std::set)
    bool operator<(const AssetID& other) const;

    // Helper to generate a random AssetUUID (Version 4)
    static AssetID Generate();

    // Convert to standard 36-character AssetUUID string
    std::string to_string() const;

    // Parse from standard 36-character UUID string
    static AssetID from_string(const std::string& str);
};
