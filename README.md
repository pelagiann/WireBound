# WireBound

High-performance LAN file distribution over TCP. A server memory-maps a file and streams it in chunks to any number of simultaneous clients. Each client verifies the received file with SHA-256.

## How it works

The server maps the file into the OS page cache on startup. After the first client warms the cache, every subsequent client is served entirely from RAM — no repeated disk reads. Each client gets its own thread and receives the file in fixed-size chunks over a persistent TCP connection.

## Build

**Requirements:** Windows, CMake 3.15+, C++17 compiler (MSVC or MinGW), Ninja (recommended)

```
cmake -S . -B build -G Ninja
cmake --build build
```

Produces `build/Server.exe` and `build/Client.exe`.

## Usage

**Server**
```
build\Server.exe <file> [chunk-size-kb] [compress 0|1]
```
Defaults to 256 KB chunks, compression off. Pass `1` as the third argument to enable LZ4 compression. Listens on port 6767. Press Ctrl+C to shut down, active transfers finish before the process exits.

**Client**
```
build\Client.exe <server-ip> [port]
```
Defaults to port 6767. Saves the received file to the current directory and prints a PASS/FAIL SHA-256 result.

## Benchmark

Place a test file in `benchmark/` and run the sweep script. It tests 1/2/4/8 simultaneous clients at 256 KB and 1024 KB chunk sizes and saves per-run logs to `benchmark/logs/`.

```
fsutil file createnew benchmark\test.bin 1073741824
benchmark\benchmark.bat
```

To regenerate graphs from the logs:
```
python benchmark\make_graphs.py
```

**Results (loopback, 1 GB file):**

| Chunk Size | Clients | Avg Per-Client | Aggregate  |
|------------|---------|----------------|------------|
| 256 KB     | 1       | 112.4 MB/s     | 112.4 MB/s |
| 256 KB     | 4       | 100.2 MB/s     | 400.8 MB/s |
| 256 KB     | 8       | 71.3 MB/s      | 570.4 MB/s |
| 1024 KB    | 1       | 112.2 MB/s     | 112.2 MB/s |
| 1024 KB    | 4       | 94.8 MB/s      | 379.0 MB/s |
| 1024 KB    | 8       | 77.0 MB/s      | 615.8 MB/s |

Single-client throughput (~112 MB/s) sits at the Gigabit Ethernet ceiling. The loopback sweep is a scaling study — the per-client bottleneck is SHA-256 hashing (picosha2, software-only), not the network path.

### Real-network results

The table above is loopback, which deliberately takes the network out of the picture. Tested between two Windows machines on the same 5 GHz Wi-Fi with a 164 MB log file that compresses about 12x, with LZ4 compression off and then on:

| Mode | Per-client throughput |
|------|-----------------------|
| Uncompressed | ~8 MB/s |
| LZ4 | ~118 MB/s |

That is roughly a 14x speedup. On loopback the link is not the bottleneck so compression does nothing, but on Wi-Fi the link is the bottleneck so sending about 12x fewer bytes finishes about 12x sooner. With two clients connected at once and no compression the link saturates and the transfers serialize, one flow dominating while the other crawls until the first finishes. With compression on the link has room to spare so both clients run in parallel. Already-compressed files such as video or installers do not shrink, in which case the server sends them uncompressed with no penalty, so compression stays an optional flag rather than always-on.

## Project layout

```
src/
  Server.cpp                  accept loop, worker threads, session stats
  Client.cpp                  receive loop, SHA-256 verification, progress display
  server/
    mapped_chunk_source.cpp   memory-mapped file + streaming SHA-256

include/
  chunk_source.hpp            abstract ChunkSource interface
  mapped_chunk_source.hpp     RAII file mapping wrapper
  progress_tracker.hpp        atomic progress counters, console mutex
  protocol.h                  wire types: FileMetaWire, ChunkHeader, PacketType

benchmark/
  benchmark.bat               multi-config sweep script
  make_graphs.py              throughput graphs from log output

third_party/
  picosha2.h                  header-only SHA-256
```

## Documentation

A detailed writeup covering the architecture, protocol design, implementation and benchmark analysis is in [`docs/writeup.md`](docs/writeup.md). A formatted Word version is also available at [`docs/WireBound_Writeup.docx`](docs/WireBound_Writeup.docx).
