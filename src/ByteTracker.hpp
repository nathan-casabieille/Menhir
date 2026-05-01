#pragma once
#include <ASTERIXCodec/Types.hpp>
#include <map>
#include <span>
#include <string>
#include <vector>

struct ByteRange {
    int start{-1};
    int end{-1};    // exclusive
    bool valid() const { return start >= 0 && end > start; }
    int length() const { return end - start; }
};

struct TrackedRecord {
    ByteRange fspec;
    // item_id → byte range within the full block buffer
    std::map<std::string, ByteRange> items;
    // Order in which items were decoded (for color assignment)
    std::vector<std::string> item_order;
};

// Compute byte ranges for each data item within a single ASTERIX record.
// 'record_bytes' must start at the first FSPEC byte of the record.
// 'block_offset' is added to all ranges so they're in full-block coordinates.
TrackedRecord trackRecord(
    std::span<const uint8_t> record_bytes,
    const asterix::CategoryDef& cat,
    const std::string& variation,
    int block_offset = 0
);
