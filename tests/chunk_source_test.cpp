#include "mapped_chunk_source.hpp"
#include "picosha2.h"

#include <cstdio>
#include <vector>
#include <stdexcept>

// -----------------------------------------------------------------------
// Day 2 test — verifies MappedChunkSource is correct
//
// What this proves:
//   1. The file opens and maps without error
//   2. Metadata (size, chunk count) is computed correctly
//   3. get_chunk() returns the right pointer and length for every chunk
//   4. Reassembling all chunks produces byte-for-byte identical content
//   5. SHA-256 of the reassembled data matches the hash stored in FileMeta
//
// Usage: chunk_source_test.exe <path-to-any-file>
// -----------------------------------------------------------------------

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: chunk_source_test.exe <file>\n");
        return 1;
    }

    try {
        //Create the chunk source
        maxxcast::MappedChunkSource src(argv[1]);
        const maxxcast::FileMeta& meta = src.metadata();

        // Print metadata
        printf("=== Metadata ===\n");
        printf("  Filename   : %s\n",   meta.filename.c_str());
        printf("  Total size : %llu bytes\n", meta.total_size);
        printf("  Chunk size : %u bytes (%u MB)\n",
               meta.chunk_size, meta.chunk_size / (1024 * 1024));
        printf("  Chunk count: %llu\n",  meta.chunk_count);
        printf("  SHA-256    : %s\n\n",  meta.sha256_hex.c_str());

        // Reassemble all chunks into one buffer
        printf("Reassembling %llu chunk(s)...\n", meta.chunk_count);

        std::vector<uint8_t> reassembled;
        reassembled.reserve(meta.total_size); // pre-allocate to avoid reallocations

        for (uint64_t i = 0; i < src.chunk_count(); ++i) {
            maxxcast::ChunkView chunk = src.get_chunk(i);

            if (chunk.index != i) {
                printf("  FAIL: chunk %llu returned wrong index %llu\n", i, chunk.index);
                return 1;
            }

            bool is_last = (i == meta.chunk_count - 1);
            size_t expected_len = is_last
                ? (size_t)(meta.total_size - i * meta.chunk_size)
                : (size_t)meta.chunk_size;

            if (chunk.len != expected_len) {
                printf("  FAIL: chunk %llu has len %zu, expected %zu\n",
                       i, chunk.len, expected_len);
                return 1;
            }

            printf("  Chunk %3llu : offset=%10llu  len=%7zu  ptr=%p  [OK]\n",
                   i, i * meta.chunk_size, chunk.len, (void*)chunk.data);

            // Append chunk bytes to reassembled buffer
            reassembled.insert(reassembled.end(), chunk.data, chunk.data + chunk.len);
        }

        // Size check
        printf("\nReassembled size : %zu bytes\n", reassembled.size());
        if (reassembled.size() != meta.total_size) {
            printf("FAIL: size mismatch (expected %llu, got %zu)\n",
                   meta.total_size, reassembled.size());
            return 1;
        }
        printf("Size check       : PASS\n");

        // Hash check
        std::string recomputed = picosha2::hash256_hex_string(
            reassembled.begin(), reassembled.end()
        );

        printf("Recomputed SHA-256: %s\n", recomputed.c_str());
        printf("Original   SHA-256: %s\n", meta.sha256_hex.c_str());

        if (recomputed == meta.sha256_hex) {
            printf("\nRESULT: PASS — chunks are correct, hashes match.\n");
            printf("MappedChunkSource is ready for Day 3 integration.\n");
        } else {
            printf("\nRESULT: FAIL — SHA-256 mismatch. Chunking is broken.\n");
            return 1;
        }

    } catch (const std::exception& e) {
        printf("ERROR: %s\n", e.what());
        return 1;
    }

    return 0;
}
