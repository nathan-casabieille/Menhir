#include "ByteTracker.hpp"
#include <stdexcept>

using namespace asterix;

// ── FSPEC parsing ─────────────────────────────────────────────────────────────

static std::vector<size_t> parseFspec(
    std::span<const uint8_t> data, size_t& fspec_bytes_out)
{
    std::vector<size_t> present;
    size_t k = 0;
    while (k < data.size()) {
        uint8_t b = data[k];
        // Bits 7..1 are item presence flags; bit 0 is FX
        for (int bit = 7; bit >= 1; --bit) {
            if (b & (1u << bit))
                present.push_back(k * 7 + (7 - bit));
        }
        ++k;
        if (!(b & 0x01)) break;  // FX=0 → last FSPEC byte
    }
    fspec_bytes_out = k;
    return present;
}

// ── Item byte-size computation ────────────────────────────────────────────────

static size_t extendedSize(std::span<const uint8_t> data, size_t offset) {
    size_t n = 0;
    while (offset + n < data.size()) {
        ++n;
        if (!(data[offset + n - 1] & 0x01)) break;
    }
    return n;
}

static size_t compoundSize(const DataItemDef& item, std::span<const uint8_t> data, size_t offset) {
    if (offset >= data.size()) return 0;

    // Parse PSF (same FX structure as FSPEC)
    size_t psf_bytes = 0;
    while (offset + psf_bytes < data.size()) {
        uint8_t b = data[offset + psf_bytes];
        ++psf_bytes;
        if (!(b & 0x01)) break;
    }

    size_t total = psf_bytes;
    // Determine present sub-items from PSF
    for (size_t k = 0; k < psf_bytes; ++k) {
        uint8_t b = data[offset + k];
        for (int bit = 7; bit >= 1; --bit) {
            if (b & (1u << bit)) {
                size_t slot = k * 7 + (7 - bit);
                if (slot < item.compound_sub_items.size()) {
                    const auto& si = item.compound_sub_items[slot];
                    if (si.name != "-")
                        total += si.fixed_bytes;
                }
            }
        }
    }
    return total;
}

static size_t itemSize(const DataItemDef& item, std::span<const uint8_t> data, size_t offset) {
    if (offset >= data.size()) return 0;

    switch (item.type) {
    case ItemType::Fixed:
        return item.fixed_bytes;

    case ItemType::Extended:
        return extendedSize(data, offset);

    case ItemType::Repetitive: {
        // Each byte: 7-bit value + FX bit; stop when FX=0
        return extendedSize(data, offset);
    }

    case ItemType::RepetitiveGroup: {
        if (offset >= data.size()) return 0;
        uint8_t count = data[offset];
        return 1 + static_cast<size_t>(count) * (item.rep_group_bits / 8);
    }

    case ItemType::RepetitiveGroupFX: {
        size_t group_bytes = (item.rep_group_bits + 1) / 8;  // includes FX bit
        if (group_bytes == 0) return 0;
        size_t n = 0;
        while (offset + n + group_bytes <= data.size()) {
            uint8_t last = data[offset + n + group_bytes - 1];
            n += group_bytes;
            if (!(last & 0x01)) break;
        }
        return n;
    }

    case ItemType::SP:
    case ItemType::Explicit: {
        if (offset >= data.size()) return 0;
        size_t len = data[offset];
        return 1 + len;
    }

    case ItemType::Compound:
        return compoundSize(item, data, offset);
    }
    return 0;
}

// ── Public entry point ────────────────────────────────────────────────────────

TrackedRecord trackRecord(
    std::span<const uint8_t> record_bytes,
    const CategoryDef& cat,
    const std::string& variation,
    int block_offset)
{
    TrackedRecord result;

    // Find UAP variation
    auto vit = cat.uap_variations.find(variation);
    if (vit == cat.uap_variations.end()) return result;
    const auto& uap = vit->second;

    // Parse FSPEC
    size_t fspec_bytes = 0;
    std::vector<size_t> present_indices = parseFspec(record_bytes, fspec_bytes);

    result.fspec = {block_offset, block_offset + static_cast<int>(fspec_bytes)};

    // Walk each present item
    size_t offset = fspec_bytes;
    for (size_t uap_idx : present_indices) {
        if (uap_idx >= uap.size()) continue;
        const std::string& item_id = uap[uap_idx];
        if (item_id == "-" || item_id == "rfs") {
            // Sentinel slots: skip (no bytes consumed)
            continue;
        }
        auto iit = cat.items.find(item_id);
        if (iit == cat.items.end()) continue;

        int item_start = block_offset + static_cast<int>(offset);
        size_t sz = itemSize(iit->second, record_bytes, offset);
        offset += sz;
        int item_end = block_offset + static_cast<int>(offset);

        result.items[item_id] = {item_start, item_end};
        result.item_order.push_back(item_id);
    }

    return result;
}
