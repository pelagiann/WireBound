#pragma once

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>

namespace maxxcast {

// All threads serialize console writes through this single mutex.
inline std::mutex g_console_mtx;

// Tracks the send progress of one client connection.
// Uses atomics so it's safe to update from a sender thread and read from a monitor/logger thread simultaneously.

struct ClientProgress
{
    int      id           = 0;
    uint64_t total_chunks = 0;
    uint64_t total_bytes  = 0;

    std::atomic<uint64_t> chunks_sent { 0 };
    std::atomic<uint64_t> bytes_sent  { 0 };
    std::atomic<bool>     complete    { false };
    std::atomic<bool>     failed      { false };

    // Print a single-line progress update.
    void print_inline() const
    {
        double pct = (total_chunks > 0)
            ? (double)chunks_sent.load() / (double)total_chunks * 100.0
            : 0.0;
        double mb_sent  = bytes_sent.load()  / (1024.0 * 1024.0);
        double mb_total = total_bytes        / (1024.0 * 1024.0);

        std::lock_guard<std::mutex> lk(g_console_mtx);
        printf("\r  [Client %d] chunk %llu/%llu  (%.1f%%)  %.2f / %.2f MB",
               id,
               (unsigned long long)chunks_sent.load(),
               (unsigned long long)total_chunks,
               pct,
               mb_sent,
               mb_total);
        fflush(stdout);
    }
};

} 
