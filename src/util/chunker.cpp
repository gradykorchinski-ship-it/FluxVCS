#include "flux/util/chunker.hpp"
#include <algorithm>

namespace flux {

std::vector<std::pair<size_t, size_t>> ContentChunker::chunk(std::span<const uint8_t> data) {
    std::vector<std::pair<size_t, size_t>> chunks;
    
    if (data.empty()) {
        return chunks;
    }
    
    size_t offset = 0;
    std::vector<uint8_t> window(WINDOW_SIZE);
    
    while (offset < data.size()) {
        size_t chunk_start = offset;
        size_t chunk_size = 0;
        
        // Scan for chunk boundary
        while (offset < data.size() && chunk_size < MAX_CHUNK_SIZE) {
            chunk_size++;
            offset++;
            
            // Only check for boundaries after minimum chunk size
            if (chunk_size >= MIN_CHUNK_SIZE && offset >= WINDOW_SIZE) {
                // Get rolling window
                size_t window_start = offset - WINDOW_SIZE;
                std::span<const uint8_t> window_data(
                    data.data() + window_start, 
                    WINDOW_SIZE
                );
                
                uint64_t hash = rolling_hash(window_data);
                
                if (is_chunk_boundary(hash)) {
                    break;
                }
            }
        }
        
        chunks.emplace_back(chunk_start, chunk_size);
    }
    
    return chunks;
}

uint64_t ContentChunker::rolling_hash(std::span<const uint8_t> window) {
    uint64_t hash = 0;
    
    for (size_t i = 0; i < window.size(); ++i) {
        hash = (hash * POLYNOMIAL) + window[i];
    }
    
    return hash;
}

bool ContentChunker::is_chunk_boundary(uint64_t hash) {
    // Check if lower bits match pattern (creates ~64KB average chunks)
    constexpr uint64_t mask = (1ULL << 16) - 1;  // 16 bits = ~64KB average
    return (hash & mask) == 0;
}

} // namespace flux
